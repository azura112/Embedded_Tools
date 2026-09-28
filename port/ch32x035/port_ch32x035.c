/**
 * @file    port_ch32x035.c
 * @brief   CH32X035 平台适配实现 (port.h 契约)
 *
 * 时钟假设: HSI 48MHz(SystemCoreClock = 48000000, PD BMC 时序要求)。
 *   - 时基 = TIM1 更新中断, 48MHz/48/1000 → 精确 1ms(与 V1.7 逐配置相同);
 *
 * ⚠ 时基不可放 SysTick(实机教训 2026-09-26): WCH debug.c 的 Delay_Us/
 *   Delay_Ms 以轮询方式独占 SysTick —— 每次调用重写 CMP、清 CNT、启停 STE,
 *   且以 |= 保留其余位。若时基放 SysTick: 延时把 CMP 改成延时值 → tick 只在
 *   延时期间以错误频率触发, 毫秒时基失真(PD 定时器/调度周期全乱); 延时轮询
 *   的标志还会被 tick ISR 抢清, 相互干扰。SysTick 必须完整留给延时函数,
 *   周期时基用 TIM1(V1.7 的既有架构)。
 *
 * 临界区语义: mstatus 保存恢复 + 嵌套计数。
 *   - ENTER: 首次进入时保存 mstatus 并清 MIE/MPIE(与 core_riscv.h
 *     __disable_irq 同码); 之后仅计数(可嵌套);
 *   - EXIT : 计数归零时恢复进入前 mstatus(允许在"已关中断"环境中
 *     再进临界区); 计数不可能被 ISR 破坏: 临界区内中断被屏蔽。
 *
 * 中断安全: port_tick_get_ms() 可在 ISR 中调用(单读)。
 *
 * 看门狗 (CH32X035 RM IWDG): LSI 典型 ~47kHz, 超时 = (RLDR+1)*div/LSI。
 *  - PSCR/RLDR 在 LSI 时钟域, 写入需 ~2 LSI 周期(~85us)同步:
 *    必须依次等 PVU 清零(改分频后)、RVU 清零(改重装后)再 Enable ——
 *    不等待则计数器从错误暂态值起跑, 本板实测造成秒级复位风暴
 *    (V1.7 实机教训, 时序与本文件保持一致);
 *  - IWDG 启动后不可停且跨软复位仍运行 → port_wdt_disable 恒 false
 *    (契约明示); 应用须在 main() 首行无条件 port_wdt_feed() 兜底
 *    "上电继承短超时"场景。
 */
#include "port.h"
#include "port_ch32x035.h"
#include "et_config.h"
#include "ch32x035.h"

/* ============ 时基: TIM1 1ms 更新中断 ============ */

static volatile uint32_t g_tick_ms = 0u;    /* port.h 32 位毫秒时基   */

/* PD 协议栈兼容计数器(EVT USBPD_SNK 惯例): 8 位自由回绕 */
volatile uint8_t Tim_Ms_Cnt = 0u;

void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void)
{
    if( TIM_GetITStatus( TIM1, TIM_IT_Update ) != RESET )
    {
        g_tick_ms++;
        Tim_Ms_Cnt++;
        TIM_ClearITPendingBit( TIM1, TIM_IT_Update );
    }
}

void port_ch32x035_tick_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_APB2PeriphClockCmd( RCC_APB2Periph_TIM1, ENABLE );

    /* 48MHz / 48 = 1MHz, /1000 = 1ms(与 V1.7 的 TIM1_Init(999, 48-1) 相同) */
    TIM_TimeBaseInitStructure.TIM_Period = 999;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 48 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0x00;
    TIM_TimeBaseInit( TIM1, &TIM_TimeBaseInitStructure );
    TIM_ClearITPendingBit( TIM1, TIM_IT_Update );

    NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init( &NVIC_InitStructure );

    TIM_ITConfig( TIM1, TIM_IT_Update, ENABLE );
    TIM_Cmd( TIM1, ENABLE );
}

port_tick_ms_t port_tick_get_ms(void)
{
    return g_tick_ms;
}

/* ============ 临界区: mstatus 保存恢复 + 嵌套计数 ============ */

static uint32_t g_crit_saved_mstatus = 0u;
static uint32_t g_crit_nest          = 0u;

void port_critical_enter(void)
{
    uint32_t ms;

    __asm__ __volatile__ ("csrr %0, mstatus" : "=r" (ms));
    if (g_crit_nest == 0u) {
        g_crit_saved_mstatus = ms;
        __asm__ __volatile__ ("csrc 0x800, %0" :: "r" (0x88u) : "memory");
        /* ⚠ 无 fence.i(实机#12): fence.i 会冲刷取指流, 在本板上触发
         * 2 字节指令流错位(非法指令, PC 落在任意执行点) —— 中断屏蔽
         * 只需 CSR 写, 不需要指令流同步 */
    }
    g_crit_nest++;
}

void port_critical_exit(void)
{
    if (g_crit_nest > 0u) {
        g_crit_nest--;
        if (g_crit_nest == 0u) {
            __asm__ __volatile__ ("csrw 0x800, %0"
                                  :: "r" (g_crit_saved_mstatus) : "memory");
        }
    }
}

/* ============ 日志底层: USART1 阻塞输出(波特率由应用 USART_Printf_Init 配) ============ */

void port_putc(char c)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
        /* 等待发送数据寄存器空 */
    }
    USART_SendData(USART1, (uint16_t)(uint8_t)c);
}

/* ============ 看门狗: IWDG (port.h 契约) ============ */

#ifndef PORT_CH32X035_LSI_HZ
#define PORT_CH32X035_LSI_HZ    47000u  /* LSI 典型 ~47kHz(取保守值; 实际略偏时超时更短=安全向) */
#endif

#define WDT_SYNC_GUARD          20000u  /* PVU/RVU 同步等待上限(≈ms 级, 远大于 2 LSI 周期) */

/* PSCR 编码 = 分频值(4..256)对应 0..6 */
static const uint16_t wdt_div_tab[7] = { 4u, 8u, 16u, 32u, 64u, 128u, 256u };

bool port_wdt_enable(uint32_t timeout_ms)
{
    uint32_t ticks, rlr, guard;
    uint32_t idx = 0u;

    if (timeout_ms < PORT_FLASH_ERASE_MS_MAX * 2u) {
        return false;                           /* 契约下限: 保证擦除喂狗窗口 */
    }
    ticks = (timeout_ms * (PORT_CH32X035_LSI_HZ / 1000u)) + 1u;
    rlr   = 0u;
    for (idx = 0u; idx < 7u; idx++) {
        rlr = ticks / wdt_div_tab[idx];
        if ((ticks % wdt_div_tab[idx]) != 0u) {
            rlr++;
        }
        if (rlr <= 0xFFFu) {
            break;                              /* 12 位 RLDR 可容纳 */
        }
    }
    if (idx >= 7u) {
        return false;                           /* 超出 IWDG 可表达上限 */
    }
    rlr -= 1u;                                  /* 超时 = (RLDR+1)*div/LSI */

    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler((uint8_t)idx);
    for (guard = 0u; guard < WDT_SYNC_GUARD; guard++) {     /* 等 PVU 清零 */
        if (IWDG_GetFlagStatus(IWDG_FLAG_PVU) == RESET) break;
    }
    if (guard >= WDT_SYNC_GUARD) return false;
    IWDG_SetReload((uint16_t)rlr);
    for (guard = 0u; guard < WDT_SYNC_GUARD; guard++) {     /* 等 RVU 清零 */
        if (IWDG_GetFlagStatus(IWDG_FLAG_RVU) == RESET) break;
    }
    if (guard >= WDT_SYNC_GUARD) return false;
    IWDG_ReloadCounter();
    IWDG_Enable();
    IWDG_ReloadCounter();                       /* 首周期即完整超时 */
    return true;
}

void port_wdt_feed(void)
{
    IWDG_ReloadCounter();                       /* 🔒ISR-safe: 单寄存器写 */
}

bool port_wdt_disable(void)
{
    return false;                               /* IWDG 语义: 启动后不可停 */
}

/* ============ flash 参数区 (port.h 契约, V1.9) ============
 * CH32X035 控制器特性(vendor ch32x035_flash.c 为准):
 *  - 擦除页 1KB(PER+ADDR+STRT), 编程 = 256B 快编程块(PAGE_PG+BUF_LOAD);
 *  - ⚠⭐ 主 flash 无读-写并行(实机#4-#9 定案): 擦写进行中的任何取指
 *    —— 包括等待循环自身的新取指 —— 都会读到垃圾, CPU 执行垃圾即
 *    非法指令复位。中断屏蔽(mstatus 与 PFIC 层)、缩短循环均无法规避,
 *    vendor FLASH_ErasePage 同样复现。vendor 自己的 .Bcode 方案是把
 *    flash 操作函数放进 0x1FFF0000 的独立 BOOT FLASH —— 本 port 采用
 *    等价思路: 擦/写核心序列整体拷贝到 RAM 执行(fence.i 后经函数指针
 *    调用), 例程自含解锁与 BSY 轮询, 擦写窗口内物理上零 flash 取指;
 *  - 例程硬约束: 只允许使用实参、立即数与寄存器绝对地址 —— 禁止引用
 *    静态数据/字符串常量/其他函数(拷贝后这类引用全部失效);
 *  - 写路径 = 读-合并-整 256B 块重编程(无额外磨损); et_kv 仅写已擦区,
 *    合并恒满足 1→0 契约; 违约时回读校验不符 → 按短写如实上报;
 *  - 参数区 = 片内 flash 尾部 PORT_FLASH_SECTOR_COUNT 个 1KB 扇区,
 *    Link.ld 代码区 60K 预留尾部 2KB;
 *  - 读(含合并读/回读校验)走 0x00000000 代码别名; 擦/写寄存器接口走
 *    0x08000000 别名(仅寄存器写, 无数据读)。
 */

#ifndef PORT_CH32X035_FLASH_SIZE
#define PORT_CH32X035_FLASH_SIZE    (62u * 1024u)   /* CH32X035G8U6: 62KB */
#endif

#define PORT_FLASH_AREA_SIZE    ((uint32_t)PORT_FLASH_SECTOR_SIZE * \
                                 (uint32_t)PORT_FLASH_SECTOR_COUNT)
#define PORT_FLASH_AREA_BASE    (0x08000000u + PORT_CH32X035_FLASH_SIZE - \
                                 PORT_FLASH_AREA_SIZE)      /* 擦/写接口 */
#define PORT_FLASH_AREA_BASE_RD (0x00000000u + PORT_CH32X035_FLASH_SIZE - \
                                 PORT_FLASH_AREA_SIZE)      /* 读/校验 */

#define X035_FLASH_KEY1         0x45670123u
#define X035_FLASH_KEY2         0xCDEF89ABu
/* FLASH 寄存器偏移(FLASH_TypeDef, 基址 0x4001A000) */
#define X035_REG_KEYR           0x04u
#define X035_REG_STATR          0x0Cu
#define X035_REG_CTLR           0x10u
#define X035_REG_ADDR           0x14u
#define X035_REG_MODEKEYR       0x24u

#define FLASH_PROG_BLOCK        256u    /* 快编程块 */

#define FLASH_PROG_WORDS        (FLASH_PROG_BLOCK / 4u)

#define X035_SR_BSY             ((uint32_t)0x00000001)
#define X035_SR_WRPRTERR        ((uint32_t)0x00000010)

/* 中断门控总开关: 启动期(尚未使能任何中断)做 flash 自检时关闭 ——
 * 避免 unmask 把四个中断提前打开。运行期必须保持开启。 */
static bool g_flash_irq_gate = true;

void port_flash_irq_gate_set(bool on)
{
    g_flash_irq_gate = on;
}

/* flash 擦写期间的 PFIC 级中断屏蔽: 即便走 RAM 执行, 主循环期间被
 * 打断也会拉长窗口, 统一屏蔽四源(TIM1/USART1/USBFS/USBPD)。 */
static void flash_irq_mask(void)
{
    if (!g_flash_irq_gate) {
        return;
    }
    NVIC_DisableIRQ( TIM1_UP_IRQn );
    NVIC_DisableIRQ( USART1_IRQn );
    NVIC_DisableIRQ( USBFS_IRQn );
    NVIC_DisableIRQ( USBPD_IRQn );
}

static void flash_irq_unmask(void)
{
    if (!g_flash_irq_gate) {
        return;
    }
    /* 先清屏蔽期间积压的 pending, 再使能 */
    NVIC_ClearPendingIRQ( TIM1_UP_IRQn );
    NVIC_ClearPendingIRQ( USART1_IRQn );
    NVIC_ClearPendingIRQ( USBFS_IRQn );
    NVIC_ClearPendingIRQ( USBPD_IRQn );
    NVIC_EnableIRQ( TIM1_UP_IRQn );
    NVIC_EnableIRQ( USART1_IRQn );
    NVIC_EnableIRQ( USBFS_IRQn );
    NVIC_EnableIRQ( USBPD_IRQn );
}

/* 操作完成后等待控制器恢复(此时已可安全取指, 但数据访问仍稍候) */
static void flash_settle(void)
{
    uint32_t i;

    for (i = 0u; i < 1000u; i++) {              /* ≈20us @48MHz */
        __NOP();
    }
}

/* ---- RAM 驻留擦写核心: 整函数拷贝到 RAM 后经函数指针执行 ----
 * 硬约束: 函数体内只允许 实参/立即数/局部变量 —— 不得引用任何静态
 * 数据、字符串常量或其他函数(拷贝到任意地址后这类引用全部失效)。 */
static void __attribute__((noinline, section(".flashopram")))
flash_op_in_ram(uint32_t op, uint32_t fbase, uint32_t addr,
                uint32_t nwords, const uint32_t *words)
{
    volatile uint32_t *keyr     = (volatile uint32_t *)(fbase + X035_REG_KEYR);
    volatile uint32_t *modekeyr = (volatile uint32_t *)(fbase + X035_REG_MODEKEYR);
    volatile uint32_t *statr    = (volatile uint32_t *)(fbase + X035_REG_STATR);
    volatile uint32_t *ctlr     = (volatile uint32_t *)(fbase + X035_REG_CTLR);
    volatile uint32_t *addr_r   = (volatile uint32_t *)(fbase + X035_REG_ADDR);
    uint32_t i;
    *keyr = X035_FLASH_KEY1;
    *keyr = X035_FLASH_KEY2;

    if (op == 0u) {                             /* 1KB 页擦除 */
        *ctlr &= (0xFFFFFFDFu & 0xFFFDFFFFu);   /* 清 OPTER / PAGE_ER */
        *ctlr |= 0x00000002u;                   /* PER */
        *addr_r = addr;
        *ctlr |= 0x00000040u;                   /* STRT */
        while ((*statr & 0x01u) != 0u) { }
        *ctlr &= 0xFFFFFFFDu;
    } else {                                    /* 256B 快编程 */
        *modekeyr = X035_FLASH_KEY1;            /* 快编程模式解锁 */
        *modekeyr = X035_FLASH_KEY2;
        *ctlr &= (0xFFFFFFDFu & 0xFFFDFFFFu);
        *ctlr |= 0x00010000u;                   /* PAGE_PG */
        *ctlr |= 0x00080000u;                   /* BUF_RST */
        while ((*statr & 0x01u) != 0u) { }
        for (i = 0u; i < nwords; i++) {
            *(volatile uint32_t *)addr = words[ i ];    /* 写地址即装填缓冲 */
            *ctlr |= 0x00040000u;               /* BUF_LOAD */
            while ((*statr & 0x01u) != 0u) { }
            addr += 4u;
        }
        *addr_r = addr - (nwords * 4u);
        *ctlr |= 0x00000040u;                   /* STRT */
        while ((*statr & 0x01u) != 0u) { }
        *ctlr &= ~0x00010000u;
    }
    *ctlr |= 0x00000080u;                       /* 重新上锁 */
}

/* 区段结束哨兵: 与上函数同段相邻, 用于取拷贝长度 */
static void __attribute__((noinline, section(".flashopram")))
flash_op_in_ram_end(void) { }

static uint32_t g_flashop_buf[ 128 ];           /* 512B RAM 例程区 */
static bool     g_flashop_ready = false;

static void flash_ram_prepare(void)
{
    uint32_t len = (uint32_t)(uintptr_t)flash_op_in_ram_end -
                   (uint32_t)(uintptr_t)flash_op_in_ram;
    const uint32_t *src = (const uint32_t *)(uintptr_t)flash_op_in_ram;
    uint32_t *dst = g_flashop_buf;
    uint32_t i;

    for (i = 0u; i < ((len + 3u) / 4u); i++) {
        dst[ i ] = src[ i ];
    }
    __asm__ __volatile__ ("fence.i" ::: "memory");  /* 写指令内存后必须同步 */
    g_flashop_ready = true;
}

typedef void (*flash_op_fn_t)(uint32_t op, uint32_t fbase, uint32_t addr,
                              uint32_t nwords, const uint32_t *words);

static void flash_ram_run(uint32_t op, uint32_t addr,
                          uint32_t nwords, const uint32_t *words)
{
    if (!g_flashop_ready) {
        flash_ram_prepare();
    }
    ((flash_op_fn_t)(void *)g_flashop_buf)( op, (uint32_t)FLASH, addr, nwords, words );
}

bool port_flash_read(uint32_t offset, void *buf, uint32_t len)
{
    const uint8_t *src;
    uint8_t *dst;

    if ((buf == NULL) || (offset > PORT_FLASH_AREA_SIZE) ||
        (len > PORT_FLASH_AREA_SIZE - offset)) {
        return false;
    }
    src = (const uint8_t *)(PORT_FLASH_AREA_BASE_RD + offset);
    dst = (uint8_t *)buf;
    while (len-- > 0u) {
        *dst++ = *src++;
    }
    return true;
}

uint32_t port_flash_write(uint32_t offset, const void *buf, uint32_t len)
{
    static uint32_t merge[ FLASH_PROG_WORDS ];  /* 读-合并缓冲(🏠MAIN 单上下文) */
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t done = 0u;
    uint32_t i;

    if ((src == NULL) || (len == 0u) ||
        ((offset & 3u) != 0u) || ((len & 3u) != 0u)) {
        return 0u;
    }
    if ((offset > PORT_FLASH_AREA_SIZE) ||
        (len > PORT_FLASH_AREA_SIZE - offset)) {
        return 0u;
    }

    PORT_CRITICAL_ENTER();
    flash_irq_mask( );                          /* PFIC 级屏蔽 */

    while (done < len) {
        uint32_t abs    = PORT_FLASH_AREA_BASE + offset + done;
        uint32_t blk    = abs & ~(FLASH_PROG_BLOCK - 1u);
        uint32_t inner  = abs - blk;
        uint32_t chunk  = FLASH_PROG_BLOCK - inner;
        uint8_t *m      = (uint8_t *)merge;

        if (chunk > len - done) {
            chunk = len - done;
        }

        (void)port_flash_read(blk - PORT_FLASH_AREA_BASE, m, FLASH_PROG_BLOCK);
        for (i = 0u; i < chunk; i++) {
            m[ inner + i ] &= src[ done + i ];  /* 1→0 合并 */
        }
        flash_ram_run( 1u, blk, FLASH_PROG_WORDS, merge );

        /* 回读校验(0x0 别名): 不符(含 0→1 违约)按故障截断, 如实上报短写 */
        for (i = 0u; i < chunk; i++) {
            if (*(volatile uint8_t *)(PORT_FLASH_AREA_BASE_RD + offset + done + i) !=
                m[ inner + i ]) {
                PORT_CRITICAL_EXIT();
                flash_irq_unmask( );
                return done;
            }
        }
        done += chunk;
    }

    PORT_CRITICAL_EXIT();
    flash_irq_unmask( );
    return done;
}

bool port_flash_erase_sector(uint32_t sector_index)
{
    bool ok = false;
    uint32_t addr;
    uint32_t i;

    if (sector_index >= PORT_FLASH_SECTOR_COUNT) {
        return false;
    }
    addr = PORT_FLASH_AREA_BASE + sector_index * PORT_FLASH_SECTOR_SIZE;

    PORT_CRITICAL_ENTER();
    flash_irq_mask( );                          /* PFIC 级屏蔽 */
    flash_ram_run( 0u, addr, 0u, NULL );
    flash_settle( );
    ok = ((FLASH->STATR & X035_SR_WRPRTERR) == 0u);
    /* 回读验证(0x0 别名): 首 16B 应全 0xFF, 把"假擦除"变可观测 */
    if (ok) {
        for (i = 0u; i < 16u; i++) {
            if (*(volatile uint8_t *)(PORT_FLASH_AREA_BASE_RD +
                                      sector_index * PORT_FLASH_SECTOR_SIZE + i) != 0xFFu) {
                ok = false;
                break;
            }
        }
    }
    PORT_CRITICAL_EXIT();
    flash_irq_unmask( );
    return ok;
}

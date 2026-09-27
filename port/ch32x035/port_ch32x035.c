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
        __asm__ __volatile__ ("fence.i" ::: "memory");
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
 *  - 擦除页 1KB(FLASH_ErasePage, F1 风格 PER+ADDR+STRT, 调用方解锁);
 *  - 无字编程公开路径: 快编程 = 256B 块缓冲(BUF_RST/BUF_LOAD + STRT,
 *    256B 对齐), ROM_WRITE 即此封裝;
 *  - 因此 port_flash_write 用"读-合并-整 256B 块重编程": 无需擦除、无额外
 *    磨损(编程只拉 1→0); et_kv 仅写已擦区, 合并恒满足 1→0 契约;
 *    调用方违反(需 0→1)时回读校验不符 → 按短写如实上报;
 *  - 参数区 = 片内 flash 尾部 PORT_FLASH_SECTOR_COUNT 个 1KB 扇区,
 *    Link.ld 已把代码区缩到 60K 预留出该 2KB。
 *
 * ⚠ 双别名(实机教训 2026-09-26): 代码在 0x00000000 别名执行, 0x08000000
 *   是 F1 风格编程接口地址 —— 厂商驱动只对它做"写"。对 0x08000000 做
 *   数据"读"触发访问故障(HardFault, 复位风暴 ~60ms/圈)。因此:
 *   读(含合并读/回读校验)一律走 0x00000000 别名; 擦写走 0x08000000
 *   接口地址(ROM_WRITE 同款)。 */

#ifndef PORT_CH32X035_FLASH_SIZE
#define PORT_CH32X035_FLASH_SIZE    (62u * 1024u)   /* CH32X035G8U6: 62KB */
#endif

#define PORT_FLASH_AREA_SIZE    ((uint32_t)PORT_FLASH_SECTOR_SIZE * \
                                 (uint32_t)PORT_FLASH_SECTOR_COUNT)
/* 读/校验用: 代码别名 */
#define PORT_FLASH_AREA_BASE_RD (0x00000000u + PORT_CH32X035_FLASH_SIZE - \
                                 PORT_FLASH_AREA_SIZE)
/* 擦/写寄存器接口用: 编程别名 */
#define PORT_FLASH_AREA_BASE    (0x08000000u + PORT_CH32X035_FLASH_SIZE - \
                                 PORT_FLASH_AREA_SIZE)

/* 控制器位定义(与 vendor ch32x035_flash.c 同值, 该文件未导出) */
#define X035_CR_STRT_Set        ((uint32_t)0x00000040)
#define X035_CR_LOCK_Set        ((uint32_t)0x00000080)
#define X035_CR_OPTER_Reset     ((uint32_t)0xFFFFFFDF)
#define X035_CR_PAGE_PG         ((uint32_t)0x00010000)
#define X035_CR_PAGE_ER_Reset   ((uint32_t)0xFFFDFFFF)
#define X035_CR_BUF_LOAD        ((uint32_t)0x00040000)
#define X035_CR_BUF_RST         ((uint32_t)0x00080000)
#define X035_SR_BSY             ((uint32_t)0x00000001)
#define X035_SR_WRPRTERR        ((uint32_t)0x00000010)
#define X035_FLASH_KEY1         ((uint32_t)0x45670123)
#define X035_FLASH_KEY2         ((uint32_t)0xCDEF89AB)

#define FLASH_PROG_BLOCK        256u    /* 快编程块 */
#define FLASH_PROG_WORDS        (FLASH_PROG_BLOCK / 4u)
#define FLASH_OP_GUARD          1000000u

/* 等待空闲: 必须 always_inline —— 擦写进行中禁止发生新的取指
 * (ROM_ERASE/ROM_WRITE 的等待循环同为函数内联形态) */
static inline __attribute__((always_inline)) void flash_wait_idle(void)
{
    uint32_t guard = FLASH_OP_GUARD;

    while ((FLASH->STATR & X035_SR_BSY) != 0u) {
        if (--guard == 0u) {
            return;                             /* 硬件卡死: 上层回读校验兜底 */
        }
    }
}

/* FPEC + 快速模式两级解锁(vendor ROM_ERASE/ROM_WRITE 同款; X035 擦写
 * 必须 MODEKEYR 解锁, 缺失则控制器进错误态 → 后续所有 flash 取指故障) */
static inline __attribute__((always_inline)) void flash_unlock_both(void)
{
    FLASH->KEYR = X035_FLASH_KEY1;
    FLASH->KEYR = X035_FLASH_KEY2;
    FLASH->MODEKEYR = X035_FLASH_KEY1;
    FLASH->MODEKEYR = X035_FLASH_KEY2;
}

/* 整 256B 块编程(words = 块完整目标内容); 调用方已解锁 */
static void flash_program_block(uint32_t blk, const uint32_t *words)
{
    uint32_t i;

    FLASH->CTLR &= (X035_CR_OPTER_Reset & X035_CR_PAGE_ER_Reset);
    FLASH->CTLR |= X035_CR_PAGE_PG;
    FLASH->CTLR |= X035_CR_BUF_RST;
    flash_wait_idle();

    for (i = 0u; i < FLASH_PROG_WORDS; i++) {
        *(volatile uint32_t *)blk = words[ i ];     /* 写地址即装填缓冲 */
        FLASH->CTLR |= X035_CR_BUF_LOAD;
        flash_wait_idle();
        blk += 4u;
    }

    FLASH->ADDR = blk - FLASH_PROG_BLOCK;
    FLASH->CTLR |= X035_CR_STRT_Set;
    flash_wait_idle();
    FLASH->CTLR &= ~X035_CR_PAGE_PG;
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
    flash_unlock_both( );                       /* KEYR + MODEKEYR 两级解锁 */

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
        flash_program_block(blk, merge);

        /* 回读校验(0x0 别名): 不符(含 0→1 违约)按故障截断, 如实上报短写 */
        for (i = 0u; i < chunk; i++) {
            if (*(volatile uint8_t *)(PORT_FLASH_AREA_BASE_RD + offset + done + i) !=
                m[ inner + i ]) {
                FLASH->CTLR |= X035_CR_LOCK_Set;
                PORT_CRITICAL_EXIT();
                return done;
            }
        }
        done += chunk;
    }

    FLASH->CTLR |= X035_CR_LOCK_Set;            /* 重新上锁 */
    PORT_CRITICAL_EXIT();
    return done;
}

bool port_flash_erase_sector(uint32_t sector_index)
{
    bool ok = false;
    uint32_t addr;

    if (sector_index >= PORT_FLASH_SECTOR_COUNT) {
        return false;
    }
    addr = PORT_FLASH_AREA_BASE + sector_index * PORT_FLASH_SECTOR_SIZE;

    PORT_CRITICAL_ENTER();
    flash_unlock_both( );                       /* KEYR + MODEKEYR 两级解锁 */
    /* 序列与 vendor ROM_ERASE 的 1KB 分支逐行同款(等待内联) */
    FLASH->CTLR &= (X035_CR_OPTER_Reset & X035_CR_PAGE_ER_Reset);
    FLASH->CTLR |= X035_CR_PER_Set;
    FLASH->ADDR = addr;
    FLASH->CTLR |= X035_CR_STRT_Set;
    flash_wait_idle( );
    FLASH->CTLR &= X035_CR_PER_Reset;
    FLASH->CTLR |= X035_CR_LOCK_Set;
    ok = ((FLASH->STATR & X035_SR_WRPRTERR) == 0u);
    PORT_CRITICAL_EXIT();
    return ok;
}

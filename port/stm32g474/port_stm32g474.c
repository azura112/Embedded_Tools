/**
 * @file    port_stm32g474.c
 * @brief   STM32G474 平台适配实现 (port.h 契约, RM0440)
 *
 * 时钟: 复位默认 HSI16, port_stm32g474_init() 按源 CubeMX 工程 (.ioc) 的时钟树
 *   升频: HSI16 /1 *18 /2 = 144MHz (FLASH 4 等待周期, Range 1)。
 *   - SysTick = HCLK/8 = 18MHz, RELOAD=17999 → 精确 1ms;
 *   - USART1 内核时钟 = PCLK2(CCIPR 复位默认) = 144MHz, BRR=1250 → 115200-8-N-1;
 *   - PLL 失锁兜底: 回退 HSI16 16MHz (BRR/SysTick 同步换算)。
 * 缓存: FLASH_ACR 仅保留等待周期, 预取/ICACHE/DCACHE 全关 —— G4 擦写后缓存
 *   不会残留旧数据, et_kv 读回校验语义与 F103 (无缓存) 完全一致。
 *
 * 临界区语义: 与 F103 相同 —— PRIMASK 保存/恢复 + 嵌套计数 (Cortex-M4)。
 *
 * flash 参数区 (RM0440 3.6/3.7, 前提 DBANK=1 双 bank 2KB 页):
 *  - 参数区 = 片内 flash 末端 16 页 (32KB) = 全局页 240..255 = bank2 页 112..127;
 *    页擦: CR.PER + CR.PNB(bank 内页号) + CR.BKER(=1) + CR.STRT;
 *  - 编程粒度为 64 位双字: CR.PG 后向同一双字连续写两个 32 位字;
 *    双字只能编程一次 (目标非全 1 → PROGERR), 端口层对"4B 对齐但跨半双字"
 *    的调用做伴随字全 1 校验后合并编程 (storage 两模块已按 8B 槽适配);
 *  - 双 bank 支持边执行边擦写: 代码在 bank1, 参数区在 bank2, 取指不冲突
 *    (port 内仍以临界区包住完整擦写序列, 与契约一致);
 *  - 单页擦除典型 ~22.1ms / 上限 24.6ms (数据手册 tERASE),
 *    PORT_FLASH_ERASE_MS_MAX 取 40ms 保守值。
 */
#include "port.h"
#include "port_stm32g474.h"
#include "stm32g474_min.h"
#include "et_config.h"

#if (PORT_FLASH_SECTOR_SIZE != 2048u)
#error "stm32g474: PORT_FLASH_SECTOR_SIZE must be 2048 (G4 dual-bank 2KB page, see README)"
#endif
#if (PORT_FLASH_SECTOR_COUNT < 4u)
#error "stm32g474: PORT_FLASH_SECTOR_COUNT too small (et_kv 2 + bootctl 3 needed)"
#endif

/* 时钟就绪标志: init 据此配置 BRR/SysTick; putc 无需感知(轮询 TXE) */
static volatile uint32_t g_tick_ms = 0u;

/* 临界区嵌套状态: enter/exit 共享; 临界区内中断被屏蔽, 本身无需保护 */
static uint32_t g_crit_saved_primask = 0u;
static uint32_t g_crit_nest          = 0u;

/* SysTick 中断: 仅累加, 自然回绕由上层无符号减法消化 */
void SysTick_Handler(void)
{
    g_tick_ms++;
}

void port_critical_enter(void)
{
    uint32_t pm;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (pm));
    if (g_crit_nest == 0u) {
        g_crit_saved_primask = pm;
        __asm__ __volatile__ ("cpsid i" ::: "memory");
    }
    g_crit_nest++;
}

void port_critical_exit(void)
{
    if (g_crit_nest > 0u) {
        g_crit_nest--;
        if (g_crit_nest == 0u) {
            __asm__ __volatile__ ("msr primask, %0"
                                  :: "r" (g_crit_saved_primask) : "memory");
        }
    }
}

port_tick_ms_t port_tick_get_ms(void)
{
    return g_tick_ms;
}

void port_putc(char c)
{
    while ((USART1_ISR & USART_ISR_TXE) == 0u) {
        /* 轮询等待发送数据寄存器空 */
    }
    USART1_TDR = (uint8_t)c;
}

/* ===================== flash 参数区 (port.h 契约, RM0440) ===================== */

/* 参数区 = 片内 flash 末端 PORT_FLASH_SECTOR_COUNT 页 (默认 16×2KB = 32KB)。
 * 全局页号 = 512K/2K - COUNT + sector_index; 全部落在 bank2 (页 128..255) */
#define PORT_G4_FLASH_SIZE      (512u * 1024u)
#define PORT_G4_PAGE_SIZE       2048u
#define PORT_G4_PAGES_PER_BANK  128u
#define PORT_FLASH_AREA_SIZE    ((uint32_t)PORT_FLASH_SECTOR_COUNT * \
                                 (uint32_t)PORT_FLASH_SECTOR_SIZE)
#define PORT_FLASH_AREA_BASE    (0x08000000u + PORT_G4_FLASH_SIZE - \
                                 PORT_FLASH_AREA_SIZE)
#define PORT_G4_PARAM_FIRST_PG  (PORT_G4_FLASH_SIZE / PORT_G4_PAGE_SIZE - \
                                 (uint32_t)PORT_FLASH_SECTOR_COUNT)

/* 忙等待上限: 144MHz 下 2×10^6 次循环 ≈ 70ms, 大于页擦上限 24.6ms,
 * 超时即视为 flash 硬件故障(擦写卡死) */
#define FLASH_BUSY_TIMEOUT      2000000u

static bool flash_wait_idle(void)
{
    uint32_t guard = FLASH_BUSY_TIMEOUT;

    while ((FLASH_SR & FLASH_SR_BSY) != 0u) {
        if (--guard == 0u) {
            return false;
        }
    }
    return true;
}

/* KEYR 解锁 (RM0440 3.7.2): 两键值须按序写入; 调用方已在临界区内 */
static bool flash_unlock(void)
{
    if ((FLASH_CR & FLASH_CR_LOCK) == 0u) {
        return true;                    /* 已解锁 (上次操作未上锁) */
    }
    FLASH_KEYR = FLASH_KEY1;
    FLASH_KEYR = FLASH_KEY2;
    return (FLASH_CR & FLASH_CR_LOCK) == 0u;
}

/* 清残留错误标志并重新上锁 (错误位写 1 清零; 操作完成后必须退出 PG/PER) */
static void flash_lock_idle(void)
{
    FLASH_SR = FLASH_SR_ERR_ALL | FLASH_SR_EOP;
    FLASH_CR &= ~(uint32_t)(FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_PNB_Msk |
                            FLASH_CR_BKER | FLASH_CR_STRT);
    FLASH_CR |= (uint32_t)FLASH_CR_LOCK;
}

bool port_flash_read(uint32_t offset, void *buf, uint32_t len)
{
    const uint8_t *src;
    uint8_t *dst;

    if ((buf == NULL) || (offset > PORT_FLASH_AREA_SIZE) ||
        (len > PORT_FLASH_AREA_SIZE - offset)) {
        return false;
    }
    src = (const uint8_t *)(PORT_FLASH_AREA_BASE + offset);
    dst = (uint8_t *)buf;
    while (len-- > 0u) {
        *dst++ = *src++;
    }
    return true;
}

uint32_t port_flash_write(uint32_t offset, const void *buf, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t done = 0u;

    /* 契约: 4B 对齐; G4 编程粒度为 64 位双字, 半双字(4B)情形按
     * "伴随字必须为全 1(从未编程)" 校验后合并编程 (见文件头注) */
    if ((src == NULL) || ((offset & 3u) != 0u) || ((len & 3u) != 0u)) {
        return 0u;
    }
    if ((offset > PORT_FLASH_AREA_SIZE) ||
        (len > PORT_FLASH_AREA_SIZE - offset)) {
        return 0u;
    }

    PORT_CRITICAL_ENTER();
    if (flash_wait_idle() && flash_unlock()) {
        FLASH_SR = FLASH_SR_ERR_ALL;            /* 清历史错误 */
        FLASH_CR |= (uint32_t)FLASH_CR_PG;      /* 编程使能 */
        while (done < len) {
            uint32_t addr = PORT_FLASH_AREA_BASE + offset + done;
            uint32_t w0 = (uint32_t)src[done + 0u] |
                          ((uint32_t)src[done + 1u] << 8) |
                          ((uint32_t)src[done + 2u] << 16) |
                          ((uint32_t)src[done + 3u] << 24);

            if ((addr & 7u) == 0u) {
                if ((done + 8u) <= len) {       /* 完整双字 */
                    uint32_t w1 = (uint32_t)src[done + 4u] |
                                  ((uint32_t)src[done + 5u] << 8) |
                                  ((uint32_t)src[done + 6u] << 16) |
                                  ((uint32_t)src[done + 7u] << 24);

                    *(volatile uint32_t *)addr = w0;
                    *(volatile uint32_t *)(addr + 4u) = w1;
                    done += 8u;
                } else {                        /* 尾随半字: 伴随字须未编程 */
                    if (*(volatile uint32_t *)(addr + 4u) != 0xFFFFFFFFu) {
                        break;
                    }
                    *(volatile uint32_t *)addr = w0;
                    *(volatile uint32_t *)(addr + 4u) = 0xFFFFFFFFu;
                    done += 4u;
                }
            } else {                            /* 半字起始: 前伴随字须未编程 */
                if (*(volatile uint32_t *)(addr - 4u) != 0xFFFFFFFFu) {
                    break;
                }
                *(volatile uint32_t *)(addr - 4u) = 0xFFFFFFFFu;
                *(volatile uint32_t *)addr = w0;
                done += 4u;
            }
            if (!flash_wait_idle()) {
                break;
            }
            /* 只允许 1→0 写 + 双字单次编程: 违例由硬件置错误位, 按故障截断 */
            if ((FLASH_SR & FLASH_SR_ERR_ALL) != 0u) {
                break;
            }
        }
        flash_lock_idle();
    }
    PORT_CRITICAL_EXIT();
    return done;
}

bool port_flash_erase_sector(uint32_t sector_index)
{
    bool ok = false;
    uint32_t global_page;
    uint32_t cr_page;

    if (sector_index >= PORT_FLASH_SECTOR_COUNT) {
        return false;
    }

    global_page = PORT_G4_PARAM_FIRST_PG + sector_index;
    if (global_page >= PORT_G4_PAGES_PER_BANK) {    /* 参数区恒在 bank2 */
        cr_page = (1u << 11) |                      /* BKER=1 */
                  ((global_page - PORT_G4_PAGES_PER_BANK)
                   << FLASH_CR_PNB_Pos);
    } else {
        cr_page = global_page << FLASH_CR_PNB_Pos;
    }

    PORT_CRITICAL_ENTER();
    if (flash_wait_idle() && flash_unlock()) {
        FLASH_SR = FLASH_SR_ERR_ALL;
        FLASH_CR = (FLASH_CR & ~(FLASH_CR_PG | FLASH_CR_PNB_Msk |
                                 FLASH_CR_BKER)) | cr_page;
        FLASH_CR |= (uint32_t)FLASH_CR_PER;     /* 页擦除使能 + 页号 */
        FLASH_CR |= (uint32_t)FLASH_CR_STRT;    /* 启动 */
        ok = flash_wait_idle();
        flash_lock_idle();
    }
    PORT_CRITICAL_EXIT();
    return ok;
}

/* 看门狗 (RM0440 42): LSI≈32kHz, 超时 = (RLR+1)*div/32kHz。
 * 契约下限 = PORT_FLASH_ERASE_MS_MAX*2 保证擦除期间有喂狗窗口;
 * IWDG 启动后不可停 → port_wdt_disable 恒返回 false (契约明示)。 */
bool port_wdt_enable(uint32_t timeout_ms)
{
    static const uint32_t div_tab[7] = { 4u, 8u, 16u, 32u, 64u, 128u, 256u };
    uint32_t ticks, div, rlr;
    uint32_t idx;
    uint32_t guard;

    if (timeout_ms < PORT_FLASH_ERASE_MS_MAX * 2u) {
        return false;                           /* 契约下限 */
    }
    ticks = timeout_ms * 32u;                   /* 32kHz → 32 tick/ms */
    div   = 0u;
    rlr   = 0u;
    for (idx = 0u; idx < 7u; idx++) {
        div = div_tab[idx];
        rlr  = ticks / div;
        if ((ticks % div) != 0u) {
            rlr++;
        }
        if (rlr <= 0xFFFu) {
            break;                              /* 12 位 RLR 可容纳 */
        }
    }
    if (idx >= 7u) {
        return false;                           /* 超出 IWDG 可表达上限 */
    }
    rlr -= 1u;

    PORT_CRITICAL_ENTER();
    IWDG_KR = IWDG_KEY_UNLOCK;                  /* 解锁 PR/RLR */
    IWDG_PR = idx;
    IWDG_RLR = rlr;
    guard = FLASH_BUSY_TIMEOUT;                 /* 等 PVU/RVU 同步完成 */
    while (((IWDG_SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u) && (guard != 0u)) {
        guard--;
    }
    IWDG_KR = IWDG_KEY_START;                   /* 启动 (不可停) */
    PORT_CRITICAL_EXIT();
    return (guard != 0u);
}

void port_wdt_feed(void)
{
    IWDG_KR = IWDG_KEY_FEED;                    /* 🔒ISR-safe: 单寄存器写 */
}

bool port_wdt_disable(void)
{
    return false;                               /* IWDG 语义: 不可停 */
}

/* ===================== 平台初始化 (非 port.h 契约, 供 main 调用) ===================== */

/* 致命配置错误: port 层不依赖 debug 层, 经 USART1 原样打印后停机 */
static void port_panic(const char *msg)
{
    while (*msg != '\0') {
        port_putc(*msg++);
    }
    port_putc('\r');
    port_putc('\n');
    for (;;) {
        __asm__ __volatile__ ("wfi");
    }
}

/* 时钟升频失败时的兜底频点 (HSI16), 供 BRR/SysTick 换算 */
static uint32_t g_sysclk_hz = 16000000u;

/* 轮询等待寄存器位, 返回是否在超限内等到 */
static bool reg_wait_set(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t guard = FLASH_BUSY_TIMEOUT;

    while ((*reg & mask) == 0u) {
        if (--guard == 0u) {
            return false;
        }
    }
    return true;
}

/* 时钟树: 与源 CubeMX 工程一致 HSI16/1*18/2 = 144MHz (见文件头注)。
 * 顺序 (RM0440): 先设 FLASH 等待周期, 再开 PLL、切 SYSCLK。 */
static void clock_init_144mhz(void)
{
    uint32_t pllcfgr = 0u;

    /* PWR 时钟 + 显式 Range 1 (复位默认即 Range 1, 与 CubeMX SCALE1 对齐) */
    RCC_APB1ENR1 |= RCC_APB1ENR1_PWREN;
    PWR_CR1 = (PWR_CR1 & ~(uint32_t)PWR_CR1_VOS_MSK) | PWR_CR1_VOS_RANGE1;

    if (((RCC_CR & RCC_CR_HSIRDY) == 0u) && !reg_wait_set(&RCC_CR, RCC_CR_HSIRDY)) {
        return;                                 /* HSI 异常: 留在复位默认时钟 */
    }

    FLASH_ACR = (FLASH_ACR & ~(uint32_t)FLASH_ACR_LATENCY_Msk) | 4u;

    /* HSI16 /1 *18 /2: PLLM=0, PLLN=18, PLLP=0(÷2, 未用), PLLQ=2(÷6, 未用),
     * PLLR=0(÷2), PLLREN=1 */
    pllcfgr = (18u << RCC_PLLCFGR_PLLN_Pos) |
              (2u << RCC_PLLCFGR_PLLQ_Pos) |
              (uint32_t)RCC_PLLCFGR_PLLREN;
    RCC_PLLCFGR = pllcfgr;
    RCC_CR |= (uint32_t)RCC_CR_PLLON;
    if (!reg_wait_set(&RCC_CR, RCC_CR_PLLRDY)) {
        return;                                 /* PLL 失锁: 回退 HSI16 */
    }

    RCC_CFGR = (RCC_CFGR & ~3u) | (uint32_t)RCC_CFGR_SW_PLL;
    if (reg_wait_set(&RCC_CFGR, RCC_CFGR_SWS_PLL)) {
        g_sysclk_hz = 144000000u;
    }
}

void port_stm32g474_init(void)
{
    /* 外设时钟: GPIOA(USART1) / GPIOC(LED) / GPIOD(按键) / PWR */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN |
                   RCC_AHB1ENR_GPIODEN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;
    RCC_APB1ENR1 |= RCC_APB1ENR1_PWREN;

    /* PC0 LED: 推挽输出, 下拉, 初始低 (与源工程 MX_GPIO_Init 一致;
     * 本板 LED 高电平点亮, 低电平点亮请对调 demo 内 LED 开关宏) */
    GPIOC_MODER = (GPIOC_MODER & ~(3u << 0)) | (1u << 0);
    GPIOC_PUPDR = (GPIOC_PUPDR & ~(3u << 0)) | (2u << 0);
    GPIOC_BSRR = (1u << 16);                    /* 预置低 = 灭 */

    /* PD15 按键: 输入 + 上拉 (按下为低; 源工程为下降沿事件, demo 内改轮询) */
    GPIOD_MODER &= ~(3u << 30);
    GPIOD_PUPDR = (GPIOD_PUPDR & ~(3u << 30)) | (1u << 30);

    /* USART1 PA9(TX)/PA10(RX): AF7 复用推挽 */
    GPIOA_MODER = (GPIOA_MODER & ~((3u << 18) | (3u << 20))) |
                  (2u << 18) | (2u << 20);
    GPIOA_AFRH = (GPIOA_AFRH & ~((0xFu << 4) | (0xFu << 8))) |
                 (7u << 4) | (7u << 8);

    clock_init_144mhz();

    /* USART1 115200-8-N-1: BRR = fPCLK/115200 (oversampling=16 复位默认) */
    USART1_BRR = (g_sysclk_hz + 57600u) / 115200u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;

    /* flash 几何以 DBANK=1(出厂默认) 为前提, 违反即明确报错停机 */
    if ((FLASH_OPTR & FLASH_OPTR_DBANK) == 0u) {
        port_panic("PORT ERR: DBANK=0, 2KB-page geometry unsupported");
    }

    /* SysTick: HCLK/8 → 1ms */
    SYST_RVR = (g_sysclk_hz / 8000u) - 1u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_ENABLE | SYST_CSR_TICKINT;      /* CLKSOURCE=0 → HCLK/8 */
}

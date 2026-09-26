/**
 * @file    port_ch32x035.c
 * @brief   CH32X035 平台适配实现 (port.h 契约)
 *
 * 时钟假设: HSI 48MHz(SystemCoreClock = 48000000, PD BMC 时序要求)。
 *   - SysTick = HCLK 直驱, CMP = 48000 → 精确 1ms, STRE 自动重装
 *     (初始化序列与 WCH EVT FreeRTOS 移植一致);
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
 *    不等待则计数器从错误暂态值起跑, 本板实测造成 ~6ms 复位风暴
 *    (V1.7 实机教训, 时序与本文件保持一致);
 *  - IWDG 启动后不可停且跨软复位仍运行 → port_wdt_disable 恒 false
 *    (契约明示); 应用须在 main() 首行无条件 port_wdt_feed() 兜底
 *    "上电继承短超时"场景。
 */
#include "port.h"
#include "port_ch32x035.h"
#include "et_config.h"
#include "ch32x035.h"

/* ============ 时基: SysTick 1ms 中断 ============ */

static volatile uint32_t g_tick_ms = 0u;    /* port.h 32 位毫秒时基   */

/* PD 协议栈兼容计数器(EVT USBPD_SNK 惯例): 8 位自由回绕 */
volatile uint8_t Tim_Ms_Cnt = 0u;

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void)
{
    g_tick_ms++;
    Tim_Ms_Cnt++;
    SysTick->SR = 0;                        /* 清 CNTIF(core 寄存器写 0 清) */
}

void port_ch32x035_tick_init(void)
{
    NVIC_SetPriority(SysTick_IRQn, 0xf0);   /* 低优先级: 不与 USBFS/PD ISR 争抢 */
    SysTick->CTLR = 0u;
    SysTick->SR   = 0u;
    SysTick->CNT  = 0u;
    SysTick->CMP  = SystemCoreClock / 1000u;    /* 48000 @48MHz → 1ms */
    SysTick->CTLR = 0xfu;                   /* STE|STIE|STCLK(HCLK)|STRE */
    /* PFIC 门控必须显式打开: STIE 只是外设侧使能, 缺本行则中断永不触发,
     * 时基冻结在 0 → 调度器永不到期 → 无人喂狗 → IWDG 周期复位
     * (WCH EVT SYSTICK_Interrupt 例程与 FreeRTOS 移植同款) */
    NVIC_EnableIRQ(SysTick_IRQn);
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

/**
 * @file    port_stm32f103.c
 * @brief   STM32F103 平台适配实现 (port.h 契约)
 *
 * 时钟假设: 复位后默认 HSI 8MHz, 未开 PLL。
 *   - SysTick = HCLK/8 = 1MHz, RELOAD=999 → 精确 1ms;
 *   - USART1 挂 APB2 (PCLK2 = 8MHz), BRR=0x045 → 115200-8-N-1 (误差 +0.64%)。
 *
 * 临界区语义: PRIMASK 保存/恢复 + 嵌套计数。
 *   - ENTER: 首次进入时保存 PRIMASK 并 cpsid i; 之后仅计数(可嵌套);
 *   - EXIT : 计数归零时恢复进入前状态(允许在"已关中断"环境中再进临界区);
 *   - 计数不可能被 ISR 破坏: 临界区内中断被屏蔽, ISR 的进入/退出天然串行化。
 *
 * 中断安全: port_tick_get_ms() 可在 ISR 中调用(单读)。
 */
#include "port.h"
#include "port_stm32f103.h"
#include "stm32f103_min.h"

static volatile uint32_t g_tick_ms = 0u;    /* SysTick 中断累加, 1ms 一跳 */

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
    while ((USART1_SR & USART_SR_TXE) == 0u) {
        /* 轮询等待发送数据寄存器空 */
    }
    USART1_DR = (uint8_t)c;
}

/* ===================== 平台初始化 (非 port.h 契约, 供 main 调用) ===================== */

void port_stm32f103_init(void)
{
    /* 外设时钟: AFIO / GPIOA(按键) / GPIOC(LED) / USART1 */
    RCC_APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN |
                   RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* PC13 LED 推挽输出 2MHz (BluePill 板载灯低电平点亮): CRH[23:20] = 0x2 */
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC_BSRR = (1u << 13);            /* 预置高 = 灭 */

    /* USART1 TX=PA9: 复用推挽 50MHz, CRH[7:4] = 0xB */
    GPIOA_CRH = (GPIOA_CRH & ~(0xFu << 4)) | (0xBu << 4);
    USART1_BRR = 0x045u;                /* 8MHz/16/115200 = 4.34 → 4+5/16 */
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;

    /* SysTick: HCLK/8 = 1MHz, 999 + 1 = 1000 计数 → 1ms */
    SYST_RVR = 999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_ENABLE | SYST_CSR_TICKINT;      /* CLKSOURCE=0 → HCLK/8 */
}

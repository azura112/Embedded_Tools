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
 *
 * flash 参数区 (PM0056):
 *  - F1 中容量页 1KB: CR.PER 页擦 + CR.STRT 启动, BSY 忙等 (PM0056 3.3.2);
 *  - 编程粒度为 16 位半字, 4B 对齐的写拆成两个半字编程 (PM0056 3.3.1);
 *  - 解锁: KEYR 依次写入 KEY1/KEY2 (PM0056 3.3.4), 序列不得被打断;
 *  - F1 没有页地址选择寄存器: 页擦目标 = STRT 执行时刻的取指所在页, 因此
 *    参数区必须与代码/常量页分离。参数区取片内 flash 末端 16 扇区
 *    (代码从 0x08000000 起, 链接脚本 ASSERT 兜底, 见 stm32f103c8t6.ld);
 *  - 擦写期间同区取指停顿: 按契约仅 🏠MAIN 线程执行擦写, port 内部以
 *    临界区包住完整擦写序列 (ISR 不可能在擦写中途打断取指冲突)。
 */
#include "port.h"
#include "port_stm32f103.h"
#include "stm32f103_min.h"
#include "et_config.h"

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

/* ===================== flash 参数区 (port.h 契约, PM0056) ===================== */

/* 参数区 = 片内 flash 末端 PORT_FLASH_SECTOR_COUNT 个扇区 (C8T6: 64K 尾部 16K)。
 * 若使用更大容量同封装芯片, 用 -DPORT_STM32F103_FLASH_SIZE 覆盖。 */
#ifndef PORT_STM32F103_FLASH_SIZE
#define PORT_STM32F103_FLASH_SIZE   (64u * 1024u)
#endif
#define PORT_FLASH_AREA_SIZE        ((uint32_t)PORT_FLASH_SECTOR_COUNT * \
                                     (uint32_t)PORT_FLASH_SECTOR_SIZE)
#define PORT_FLASH_AREA_BASE        (0x08000000u + PORT_STM32F103_FLASH_SIZE - \
                                     PORT_FLASH_AREA_SIZE)

/* 忙等待上限: 8MHz 下约 10^6 次循环(≈1s), 远大于 20ms 典型页擦耗时,
 * 超时即视为 flash 硬件故障(擦写卡死) */
#define FLASH_BUSY_TIMEOUT          1000000u

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

/* KEYR 解锁 (PM0056 3.3.4): 两键值须按序写入; 序列被打断会解锁失败。
 * 调用方 (port_flash_write/erase_sector) 已在临界区内, 序列原子性有保证。 */
static bool flash_unlock(void)
{
    if ((FLASH_CR & FLASH_CR_LOCK) == 0u) {
        return true;                    /* 已解锁 (上次操作未上锁) */
    }
    FLASH_KEYR = FLASH_KEY1;
    FLASH_KEYR = FLASH_KEY2;
    return (FLASH_CR & FLASH_CR_LOCK) == 0u;
}

/* 复位 CR 模式位并重新上锁 (PM0056 3.3.1: 操作完成后必须清 PG/PER 并置 LOCK) */
static void flash_lock_idle(void)
{
    FLASH_CR &= ~(uint32_t)(FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_MER |
                            FLASH_CR_STRT);
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

    /* 契约: 4B 对齐; F1 编程粒度为 16 位半字, 4B 块内拆两个半字编程 */
    if ((src == NULL) || ((offset & 3u) != 0u) || ((len & 3u) != 0u)) {
        return 0u;
    }
    if ((offset > PORT_FLASH_AREA_SIZE) ||
        (len > PORT_FLASH_AREA_SIZE - offset)) {
        return 0u;
    }

    PORT_CRITICAL_ENTER();
    if (flash_unlock()) {
        FLASH_CR |= (uint32_t)FLASH_CR_PG;      /* 编程使能 */
        for (done = 0u; done < len; done += 4u) {
            uint32_t addr = PORT_FLASH_AREA_BASE + offset + done;
            uint32_t sr;

            *(volatile uint16_t *)(addr + 0u) =
                (uint16_t)((uint16_t)src[done + 0u] |
                           (uint16_t)((uint16_t)src[done + 1u] << 8));
            if (!flash_wait_idle()) {
                break;
            }
            /* 只允许 1→0 写: 违反(未擦重写)硬件置 PGERR/WRPRTERR, 按故障截断 */
            sr = FLASH_SR;
            if ((sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0u) {
                break;
            }
            *(volatile uint16_t *)(addr + 2u) =
                (uint16_t)((uint16_t)src[done + 2u] |
                           (uint16_t)((uint16_t)src[done + 3u] << 8));
            if (!flash_wait_idle()) {
                break;
            }
            sr = FLASH_SR;
            if ((sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0u) {
                break;
            }
        }
        /* done 仅在整块(两半字)成功后才推进: 短写一律按 4B 块裁剪上报,
         * 语义与宿主机模拟一致 —— 短写 = 掉电/故障, 调用方校验 */
        flash_lock_idle();
    }
    PORT_CRITICAL_EXIT();
    return done;
}

bool port_flash_erase_sector(uint32_t sector_index)
{
    bool ok = false;

    if (sector_index >= PORT_FLASH_SECTOR_COUNT) {
        return false;
    }

    PORT_CRITICAL_ENTER();
    if (flash_unlock()) {
        FLASH_CR |= (uint32_t)FLASH_CR_PER;     /* 页擦除使能 */
        FLASH_CR |= (uint32_t)FLASH_CR_STRT;    /* 启动: 目标页 = 当前取指页 (见头注) */
        ok = flash_wait_idle();
        flash_lock_idle();
    }
    PORT_CRITICAL_EXIT();
    return ok;
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

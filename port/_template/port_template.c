/**
 * @file    port_template.c
 * @brief   新平台 port 契约骨架 (v2.0 P3-2) —— 复制为 port_<芯片>.c 填空
 *
 * 与 ../port.h 契约逐项对齐; TODO 处为最小实现点。
 * 编译示例(按需替换宏与包含):
 *   <cc> -DPORT_FLASH_SECTOR_SIZE=2048 -DPORT_FLASH_SECTOR_COUNT=16 \
 *        -DPORT_FLASH_ERASE_MS_MAX=40 -c port_template.c
 */
#include "port.h"

/* ===================== 几何守卫 (v1.6 G4 教训: 漏配必须编译失败) ===================== */
#if !defined(PORT_FLASH_SECTOR_SIZE) || !defined(PORT_FLASH_SECTOR_COUNT)
#error "port geometry macros missing: -DPORT_FLASH_SECTOR_SIZE/-DPORT_FLASH_SECTOR_COUNT"
#endif
#if (PORT_FLASH_SECTOR_COUNT < 4u)
#error "parameter area too small (et_kv 2 + bootctl 3 needed)"
#endif

/* ===================== 时基 ===================== */
port_tick_ms_t port_tick_get_ms(void)
{
    /* TODO: 返回 1ms 粒度单调毫秒(允许自然回绕) */
    return 0u;
}

/* ===================== 字符输出 ===================== */
void port_putc(char c)
{
    /* TODO: 阻塞式发送单字节(控制台/日志共用; 可等 TXE 轮询) */
    (void)c;
}

/* ===================== 临界区 (嵌套计数, 退出恢复进入前 PRIMASK) ===================== */
static uint32_t g_crit_saved = 0u;
static uint32_t g_crit_nest  = 0u;

void port_critical_enter(void)
{
    /* TODO: 保存当前中断屏蔽态 -> 关中断; 仅最外层保存 */
    if (g_crit_nest == 0u) {
        g_crit_saved = 0u;      /* TODO: READ_PRIMASK() */
        /* TODO: __disable_irq() */
    }
    g_crit_nest++;
}

void port_critical_exit(void)
{
    if (g_crit_nest > 0u) {
        g_crit_nest--;
        if (g_crit_nest == 0u) {
            /* TODO: RESTORE_PRIMASK(g_crit_saved) —— 注意是"恢复", 不是"开中断" */
        }
    }
}

/* ===================== Flash 参数区 (offset = 参数区内偏移) ===================== */
bool port_flash_read(uint32_t offset, void *buf, uint32_t len)
{
    /* TODO: memcpy(参数区基址 + offset); 越界返回 false */
    (void)offset; (void)buf; (void)len;
    return false;
}

uint32_t port_flash_write(uint32_t offset, const void *buf, uint32_t len)
{
    /* TODO: 按平台编程粒度写; 返回实际写入字节数(短写=失败截断)。
     * 粒度教训: G4 为 64 位双字且目标须全 1, 半字/重编程会 PROGERR ——
     * 4B 对齐语义需"伴随字全 1 校验后合并编程"。 */
    (void)offset; (void)buf; (void)len;
    return 0u;
}

bool port_flash_erase_sector(uint32_t sector_index)
{
    /* TODO: 擦参数区内扇区(sector_index < PORT_FLASH_SECTOR_COUNT);
     * 阻塞耗时须 ≤ PORT_FLASH_ERASE_MS_MAX。 */
    (void)sector_index;
    return false;
}

/* ===================== 看门狗 (ET_MODULE_WDT=0 时整组可删) ===================== */
bool port_wdt_enable(uint32_t timeout_ms)
{
    /* 契约下限: 擦除窗口 ×2; 先 START 让慢时钟起振, 再等同步, 最后喂狗
     * (v1.9 G474 冷启动时序教训, 见 port/stm32g474/README.md) */
    if (timeout_ms < PORT_FLASH_ERASE_MS_MAX * 2u) {
        return false;
    }
    return false;               /* TODO */
}

void port_wdt_feed(void)
{
    /* TODO: 单寄存器喂狗(ISR-safe 要求在此自证) */
}

bool port_wdt_disable(void)
{
    return false;               /* 多数 MCU IWDG 语义: 启动后不可停 */
}

/**
 * @file    port_host.c
 * @brief   宿主机(PC)平台适配实现 —— 仅测试用
 *
 * 说明:
 *  - 时基为纯软件虚拟时钟, 由测试代码通过 port_host_tick_set/advance 控制;
 *  - 单线程宿主环境, 临界区为空实现;
 *  - 移植到真实 MCU 时请替换为目标平台的 port 实现(SysTick/开关中断/串口)。
 */
#include "port.h"
#include "port_host.h"

#include <stdio.h>

static volatile uint32_t g_virtual_ticks = 0u;

/* 输出捕获状态(测试用) */
static char    *g_cap_buf = NULL;
static uint32_t g_cap_cap = 0u;
static uint32_t g_cap_n = 0u;
static int      g_cap_active = 0;

void port_host_capture_start(char *buf, uint32_t cap)
{
    if ((buf == NULL) || (cap == 0u)) {
        return;
    }
    buf[0]      = '\0';
    g_cap_buf   = buf;
    g_cap_cap   = cap;
    g_cap_n     = 0u;
    g_cap_active = 1;
}

uint32_t port_host_capture_stop(void)
{
    g_cap_active = 0;
    if (g_cap_buf != NULL) {
        g_cap_buf[(g_cap_n < g_cap_cap) ? g_cap_n : (g_cap_cap - 1u)] = '\0';
    }
    return g_cap_n;
}

void port_host_tick_set(uint32_t ms)
{
    g_virtual_ticks = ms;
}

void port_host_tick_advance(uint32_t delta)
{
    g_virtual_ticks += delta;
}

uint32_t port_host_tick_now(void)
{
    return g_virtual_ticks;
}

void port_critical_enter(void)
{
    /* 单线程空实现 */
}

void port_critical_exit(void)
{
    /* 单线程空实现 */
}

port_tick_ms_t port_tick_get_ms(void)
{
    return g_virtual_ticks;
}

void port_putc(char c)
{
    if (g_cap_active != 0) {
        if (g_cap_n < g_cap_cap) {
            g_cap_buf[g_cap_n++] = c;
        }
        return;
    }
    (void)putchar(c);
}

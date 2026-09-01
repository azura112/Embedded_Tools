/**
 * @file    port_host.h
 * @brief   宿主机(PC)平台适配 —— 仅测试用, 不随库发布到 MCU
 *
 * 核心能力: 虚拟时间注入。sys 层单测通过手动推进时基获得完全确定的行为,
 * 无需真实等待, 也天然覆盖 tick 回绕等边界场景。
 */
#ifndef PORT_HOST_H
#define PORT_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 直接设置虚拟时基当前值(毫秒), 可用于构造回绕前的临界位置 */
void port_host_tick_set(uint32_t ms);

/* 将虚拟时基前进 delta 毫秒 */
void port_host_tick_advance(uint32_t delta);

/* 读取虚拟时基当前值 */
uint32_t port_host_tick_now(void);

/* ---- 输出捕获(测试用): 开启后 port_putc 写入缓冲而非 stdout ---- */
void     port_host_capture_start(char *buf, uint32_t cap);

/* 停止捕获并返回写入长度(缓冲内自动补 NUL) */
uint32_t port_host_capture_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HOST_H */

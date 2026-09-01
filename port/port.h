/**
 * @file    port.h
 * @brief   平台适配层接口契约 (唯一允许接触硬件的层)
 *
 * 分层契约:
 *  - core/ 层【禁止】包含本文件, 保证纯 C 零硬件依赖;
 *  - sys/ drivers/ debug/ 层仅通过本头文件访问硬件能力;
 *  - 各平台在 port/<platform>/ 下提供实现(如 port/stm32f103/, port/host/)。
 *
 * 实现清单:
 *  - PORT_CRITICAL_ENTER / PORT_CRITICAL_EXIT : 主循环与 ISR 共享数据临界区, 必须可嵌套;
 *  - port_tick_get_ms()  : 毫秒级单调递增时基(SysTick 等), 由平台中断维护;
 *  - port_putc()         : 阻塞式单字符输出(debug 日志使用), 平台自行重定向到串口等。
 */
#ifndef PORT_H
#define PORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t port_tick_ms_t;

/* 进入临界区: 保存并关闭中断(或等效机制), 与 EXIT 必须成对且可嵌套 */
#define PORT_CRITICAL_ENTER()   port_critical_enter()
#define PORT_CRITICAL_EXIT()    port_critical_exit()

void    port_critical_enter(void);
void    port_critical_exit(void);

/* 获取毫秒时基(单调递增, 允许自然回绕, 上层用无符号减法比较) */
port_tick_ms_t port_tick_get_ms(void);

/* 日志底层输出: 输出单个字符, 需阻塞直到发送完成 */
void    port_putc(char c);

#ifdef __cplusplus
}
#endif

#endif /* PORT_H */

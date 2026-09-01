/**
 * @file    et_event.h
 * @brief   事件标志组 (ISR 置位 / 主循环消费 的轻量同步原语)
 *
 * 模型:
 *  - 32 个独立事件位, set() 按"或"累积, wait_and_clear() 取走并清除指定位;
 *  - 典型用法: ISR 中 set(EVENT_UART_RX), 主循环轮询 wait_and_clear 后处理;
 *
 * 并发策略:
 *  - 全部 API 任意上下文可用(内部临界区保护读改写);
 *  - 多个生产者同时置位安全; 建议仅主循环消费。
 */
#ifndef ET_EVENT_H
#define ET_EVENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_EVENT

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t flags;
} et_event_group_t;

/* 初始化(清零) */
void     et_event_init(et_event_group_t *g);

/* 置位事件(可一次多位, 按"或"累积); 可在 ISR 调用 */
void     et_event_set(et_event_group_t *g, uint32_t bits);

/* 当前未清除的事件位 */
uint32_t et_event_peek(const et_event_group_t *g);

/* 取走并清除 mask 选中的位, 返回取到的位值; 建议仅主循环调用 */
uint32_t et_event_wait_and_clear(et_event_group_t *g, uint32_t mask);

/* 强制清除指定位(不返回旧值) */
void     et_event_clear(et_event_group_t *g, uint32_t bits);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_EVENT */
#endif /* ET_EVENT_H */

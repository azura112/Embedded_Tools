/**
 * @file    et_stimer.h
 * @brief   软件定时器 (单次/周期, 主循环轮询分发, 不占用额外硬件定时器)
 *
 * 模型:
 *  - 定时器对象由调用方持有(静态/全局), 经注册进入模块内部链表;
 *  - 主循环周期性调用 et_stimer_poll(now) 分发到期回调;
 *  - 回调在临界区外执行; 回调内可安全地 stop 自身/其他定时器或 start 新定时器;
 *  - 周期定时器采用【追赶语义】: 错过多个周期时逐次补发, 平均频率保持不变;
 *    若不希望停顿后出现补发脉冲, 请改用单次模式自行重挂载。
 *
 * 并发策略:
 *  - start/stop/set_arg 可在 ISR 中调用(内部临界区保护链表操作);
 *  - poll 仅允许在主循环上下文调用, 且不可重入;
 *
 * 时基: uint32 毫秒, 单次延迟/周期须 < 2^31 ms(内部以 int32 差值判定到期,
 *       时基自然回绕时依然正确)。
 */
#ifndef ET_STIMER_H
#define ET_STIMER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_STIMER

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*et_stimer_fn)(void *arg);

typedef struct et_stimer {
    struct et_stimer *next;         /* 内部链表指针, 勿动   */
    et_stimer_fn      cb;           /* 到期回调             */
    void             *arg;          /* 回调参数             */
    uint32_t          period_ms;    /* 周期(周期模式有效)   */
    uint32_t          expire_at;    /* 到期时刻(绝对毫秒)   */
    bool              periodic;     /* 是否周期模式         */
    bool              running;      /* 运行标志             */
    bool              in_list;      /* 是否已挂入注册链表   */
} et_stimer_t;

/* 初始化定时器对象(不启动); cb 必须非空 */
bool et_stimer_init(et_stimer_t *t, et_stimer_fn cb, void *arg);

/* 启动单次定时器: delay_ms 后触发一次; 已运行则重新计时 */
bool et_stimer_start_oneshot(et_stimer_t *t, uint32_t delay_ms);

/* 启动周期定时器: 每 period_ms 触发一次(period_ms >= 1); 已运行则重新锚定 */
bool et_stimer_start_periodic(et_stimer_t *t, uint32_t period_ms);

/* 停止(可在 ISR 调用); 未运行返回 false */
bool et_stimer_stop(et_stimer_t *t);

bool et_stimer_is_running(const et_stimer_t *t);

/* 分发所有到期回调; now 为当前毫秒时基(通常传 port_tick_get_ms()) */
void et_stimer_poll(uint32_t now);

/* 复位模块: 解除全部注册并停止全部定时器(热复位场景/测试隔离用) */
void et_stimer_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_STIMER */
#endif /* ET_STIMER_H */

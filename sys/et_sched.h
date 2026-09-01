/**
 * @file    et_sched.h
 * @brief   协作式周期任务调度器 (前后台超级循环专用)
 *
 * 模型:
 *  - 任务为"固定周期函数": 注册时声明 period_ms, 主循环反复调用
 *    et_sched_poll_once(), 到期任务依次执行;
 *  - 错峰: 各任务按注册顺序执行, 建议将周期设为互质以分散负载峰值;
 *  - 漂移吸收: 任务执行耗时导致错过多个周期时, 只补跑一次并重锚定 now,
 *    避免积压风暴(与 et_stimer 策略一致);
 *
 * 并发策略:
 *  - 【仅限主循环上下文】: register/unregister/poll_once 不可在 ISR 调用,
 *    因此内部无需临界区; ISR 与主循环的交互请使用 et_event 或 et_queue;
 *
 * 时基: uint32 毫秒, period_ms >= 1 且 < 2^31。
 */
#ifndef ET_SCHED_H
#define ET_SCHED_H

#include <stdint.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_SCHED

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*et_task_fn)(void *arg);

typedef struct et_task {
    struct et_task *next;           /* 内部链表指针, 勿动 */
    et_task_fn      fn;             /* 任务函数           */
    void           *arg;            /* 任务参数           */
    uint32_t        period_ms;      /* 调度周期           */
    uint32_t        last_run;       /* 上次运行时刻       */
    bool            in_list;        /* 是否已注册         */
} et_task_t;

/* 注册任务: 尾插保持 FIFO 公平性; period_ms >= 1 */
bool et_sched_register(et_task_t *t, et_task_fn fn, void *arg, uint32_t period_ms);

/* 注销任务: 未注册返回 false */
bool et_sched_unregister(et_task_t *t);

/* 扫描一遍到期任务并逐个执行后返回(非阻塞, 适合主循环 + WFI 低功耗模式) */
void et_sched_poll_once(void);

/* 复位: 注销全部任务(热复位场景/测试隔离用) */
void et_sched_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_SCHED */
#endif /* ET_SCHED_H */

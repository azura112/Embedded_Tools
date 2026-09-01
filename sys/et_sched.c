/**
 * @file    et_sched.c
 * @brief   协作式周期任务调度器实现
 */
#include "et_sched.h"
#include "port.h"

#if ET_MODULE_SCHED

#include <stddef.h>

static et_task_t *g_head = NULL;
static et_task_t *g_tail = NULL;    /* 尾插保证 FIFO 公平性 */

bool et_sched_register(et_task_t *t, et_task_fn fn, void *arg, uint32_t period_ms)
{
    ET_ASSERT(t != NULL);
    ET_ASSERT(fn != NULL);
    if ((t == NULL) || (fn == NULL)) {
        return false;
    }
    if ((period_ms == 0u) || t->in_list) {
        return false;
    }
    t->fn        = fn;
    t->arg       = arg;
    t->period_ms = period_ms;
    t->last_run  = port_tick_get_ms();  /* 从注册时刻起算第一个周期 */
    t->next      = NULL;
    if (g_tail == NULL) {
        g_head = t;
    } else {
        g_tail->next = t;
    }
    g_tail    = t;
    t->in_list = true;
    return true;
}

bool et_sched_unregister(et_task_t *t)
{
    et_task_t *it;

    ET_ASSERT(t != NULL);
    if ((t == NULL) || !t->in_list) {
        return false;
    }
    it = g_head;
    if (it == t) {
        g_head = t->next;
        if (g_tail == t) {
            g_tail = NULL;
        }
    } else {
        while ((it != NULL) && (it->next != t)) {
            it = it->next;
        }
        if (it == NULL) {
            return false;
        }
        it->next = t->next;
        if (g_tail == t) {
            g_tail = it;
        }
    }
    t->in_list = false;
    t->next    = NULL;
    return true;
}

void et_sched_poll_once(void)
{
    uint32_t now = port_tick_get_ms();

    for (;;) {
        /* 与 stimer 相同的"单任务处理+重扫"模式:
         * 任务回调中的注册/注销不会破坏遍历一致性 */
        et_task_t *hit = NULL;
        et_task_t *it  = g_head;

        while (it != NULL) {
            /* 无符号减法: 时基回绕时依然正确 */
            if ((uint32_t)(now - it->last_run) >= it->period_ms) {
                hit = it;
                break;
            }
            it = it->next;
        }
        if (hit == NULL) {
            break;
        }

        hit->last_run = now;            /* 重锚定, 错过的周期不补跑 */
        hit->fn(hit->arg);
    }
}

void et_sched_reset(void)
{
    et_task_t *it = g_head;

    while (it != NULL) {
        et_task_t *next = it->next;

        it->in_list = false;
        it->next    = NULL;
        it = next;
    }
    g_head = NULL;
    g_tail = NULL;
}

#endif /* ET_MODULE_SCHED */

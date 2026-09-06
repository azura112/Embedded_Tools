/**
 * @file    et_stimer.c
 * @brief   软件定时器实现
 */
#include "et_stimer.h"
#include "port.h"        /* 临界区与时基均经平台适配层访问 */

#if ET_MODULE_STIMER

#include <stddef.h>

static et_stimer_t *g_head = NULL;

static void st_list_insert(et_stimer_t *t)
{
    PORT_CRITICAL_ENTER();
    t->next   = g_head;
    g_head    = t;
    t->in_list = true;
    PORT_CRITICAL_EXIT();
}

bool et_stimer_init(et_stimer_t *t, et_stimer_fn cb, void *arg)
{
    ET_ASSERT(t != NULL);
    ET_ASSERT(cb != NULL);
    if ((t == NULL) || (cb == NULL)) {
        return false;
    }
    if (t->in_list) {
        return false;                       /* 运行中禁止重复初始化 */
    }
    t->cb        = cb;
    t->arg       = arg;
    t->period_ms = 0u;
    t->expire_at = 0u;
    t->periodic  = false;
    t->running   = false;
    return true;
}

static bool st_arm(et_stimer_t *t, uint32_t span_ms, uint32_t now)
{
    if (span_ms == 0u) {
        return false;
    }
    if (!t->in_list) {
        st_list_insert(t);
    }
    t->period_ms = span_ms;
    t->expire_at = now + span_ms;
    t->running   = true;
    return true;
}

bool et_stimer_start_oneshot(et_stimer_t *t, uint32_t delay_ms)
{
    ET_ASSERT(t != NULL);
    if (t == NULL) {
        return false;
    }
    return st_arm(t, delay_ms, port_tick_get_ms());
}

bool et_stimer_start_periodic(et_stimer_t *t, uint32_t period_ms)
{
    ET_ASSERT(t != NULL);
    if (t == NULL) {
        return false;
    }
    if (!st_arm(t, period_ms, port_tick_get_ms())) {
        return false;
    }
    t->periodic = true;
    return true;
}

bool et_stimer_stop(et_stimer_t *t)
{
    ET_ASSERT(t != NULL);
    if ((t == NULL) || !t->running) {
        return false;
    }
    t->running = false;
    return true;
}

bool et_stimer_is_running(const et_stimer_t *t)
{
    return (t != NULL) && t->running;
}

void et_stimer_poll(uint32_t now)
{
    for (;;) {
        /* 每轮只处理一个到期者后重新扫描:
         * 保证回调中任意增删定时器都不会破坏遍历一致性 */
        et_stimer_t *hit = NULL;
        et_stimer_t *it;

        PORT_CRITICAL_ENTER();
        it = g_head;
        while (it != NULL) {
            if (it->running &&
                ((int32_t)(now - it->expire_at) >= 0)) {
                hit = it;
                break;
            }
            it = it->next;
        }
        PORT_CRITICAL_EXIT();

        if (hit == NULL) {
            break;
        }

        if (hit->periodic) {
            /* 追赶语义: 到期点按周期步进而非重锚定 now,
             * 保证长时间停顿后仍能补足次数、平均频率不变 */
            hit->expire_at += hit->period_ms;
        } else {
            hit->running = false;
        }
        hit->cb(hit->arg);
    }
}

port_tick_ms_t et_stimer_next_due(void)
{
    uint32_t    now = port_tick_get_ms();
    uint32_t    min_rem = PORT_TICK_WAIT_FOREVER;
    et_stimer_t *it;

    PORT_CRITICAL_ENTER();
    it = g_head;
    while (it != NULL) {
        if (it->running) {
            /* 到期判定与 poll 同一算法: int32 差值 (时基回绕安全) */
            uint32_t rem;

            if ((int32_t)(now - it->expire_at) >= 0) {
                rem = 0u;                   /* 已到期, 应立即 poll */
            } else {
                rem = it->expire_at - now;
            }
            if (rem < min_rem) {
                min_rem = rem;
            }
        }
        it = it->next;
    }
    PORT_CRITICAL_EXIT();
    return min_rem;                     /* 无运行中定时器: WAIT_FOREVER */
}

void et_stimer_reset_all(void)
{
    et_stimer_t *it;

    PORT_CRITICAL_ENTER();
    it = g_head;
    while (it != NULL) {
        et_stimer_t *next = it->next;

        it->in_list = false;
        it->running = false;
        it = next;
    }
    g_head = NULL;
    PORT_CRITICAL_EXIT();
}

#endif /* ET_MODULE_STIMER */

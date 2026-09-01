/**
 * @file    test_sched.c
 * @brief   et_sched 单元测试 (虚拟时间驱动)
 */
#include "et_test.h"
#include "et_sched.h"
#include "port_host.h"

static uint32_t g_run_a;
static uint32_t g_run_b;
static uint32_t g_run_c;

static void cb_run_a(void *arg)
{
    (void)arg;
    g_run_a++;
}

static void cb_run_b(void *arg)
{
    (void)arg;
    g_run_b++;
}

static void cb_run_c(void *arg)
{
    (void)arg;
    g_run_c++;
}

static void setup(void)
{
    et_sched_reset();
    port_host_tick_set(0u);
    g_run_a = 0u;
    g_run_b = 0u;
    g_run_c = 0u;
}

static void sc_register_and_periodic_run(void)
{
    static et_task_t t;

    setup();
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 10u));

    /* 每 5ms 推进并轮询一次, 至 35ms: 应在 10/20/30 执行 */
    while (port_host_tick_now() < 35u) {
        port_host_tick_advance(5u);
        et_sched_poll_once();
    }
    ET_CHECK_U32_EQ(3u, g_run_a);

    port_host_tick_advance(5u);                     /* 40ms: 再执行一次 */
    et_sched_poll_once();
    ET_CHECK_U32_EQ(4u, g_run_a);
}

static void sc_register_rejects_bad(void)
{
    static et_task_t t;

    setup();
    ET_CHECK(!et_sched_register(NULL, cb_run_a, NULL, 10u));
    ET_CHECK(!et_sched_register(&t, NULL, NULL, 10u));
    ET_CHECK(!et_sched_register(&t, cb_run_a, NULL, 0u));   /* 非法周期 */
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 10u));
    ET_CHECK(!et_sched_register(&t, cb_run_a, NULL, 20u));  /* 重复注册拒绝 */
    ET_CHECK(et_sched_unregister(&t));
}

static void sc_multi_tasks_interleave(void)
{
    static et_task_t ta;
    static et_task_t tb;

    setup();
    ET_CHECK(et_sched_register(&ta, cb_run_a, NULL, 10u));
    ET_CHECK(et_sched_register(&tb, cb_run_b, NULL, 25u));

    while (port_host_tick_now() < 100u) {
        port_host_tick_advance(1u);
        et_sched_poll_once();
    }
    ET_CHECK_U32_EQ(10u, g_run_a);                  /* 10..100 步进 10 */
    ET_CHECK_U32_EQ(4u, g_run_b);                   /* 25/50/75/100    */
}

static void sc_unregister_stops_task(void)
{
    static et_task_t t;

    setup();
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 5u));
    port_host_tick_advance(5u);
    et_sched_poll_once();
    ET_CHECK_U32_EQ(1u, g_run_a);

    ET_CHECK(et_sched_unregister(&t));
    ET_CHECK(!et_sched_unregister(&t));             /* 重复注销返回 false */

    port_host_tick_advance(100u);
    et_sched_poll_once();
    ET_CHECK_U32_EQ(1u, g_run_a);
}

static void sc_missed_periods_absorbed(void)
{
    static et_task_t t;

    setup();
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 10u));
    port_host_tick_advance(100u);                   /* 一次跳过 10 个周期 */
    et_sched_poll_once();
    ET_CHECK_U32_EQ(1u, g_run_a);                   /* 只补跑一次 */

    port_host_tick_advance(10u);
    et_sched_poll_once();
    ET_CHECK_U32_EQ(2u, g_run_a);                   /* 恢复正常节奏 */
}

static void sc_reregister_after_unregister(void)
{
    static et_task_t t;

    setup();
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 5u));
    ET_CHECK(et_sched_unregister(&t));
    ET_CHECK(et_sched_register(&t, cb_run_b, NULL, 7u));    /* 换函数换周期 */

    port_host_tick_advance(7u);
    et_sched_poll_once();
    ET_CHECK_U32_EQ(0u, g_run_a);
    ET_CHECK_U32_EQ(1u, g_run_b);
}

static void sc_reset_clears_all(void)
{
    static et_task_t ta;
    static et_task_t tb;

    setup();
    ET_CHECK(et_sched_register(&ta, cb_run_a, NULL, 5u));
    ET_CHECK(et_sched_register(&tb, cb_run_b, NULL, 5u));
    et_sched_reset();

    port_host_tick_advance(50u);
    et_sched_poll_once();
    ET_CHECK_U32_EQ(0u, g_run_a);
    ET_CHECK_U32_EQ(0u, g_run_b);

    /* 复位后可重新注册 */
    ET_CHECK(et_sched_register(&ta, cb_run_c, NULL, 5u));
    port_host_tick_advance(5u);
    et_sched_poll_once();
    ET_CHECK_U32_EQ(1u, g_run_c);
}

const et_test_case_t *test_sched_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"sched.register_and_run",     sc_register_and_periodic_run},
        {"sched.register_rejects_bad", sc_register_rejects_bad},
        {"sched.multi_interleave",     sc_multi_tasks_interleave},
        {"sched.unregister_stops",     sc_unregister_stops_task},
        {"sched.missed_absorbed",      sc_missed_periods_absorbed},
        {"sched.reregister_ok",        sc_reregister_after_unregister},
        {"sched.reset_clears",         sc_reset_clears_all},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

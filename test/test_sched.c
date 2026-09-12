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

/* ---- tickless: et_sched_next_due (v1.6) ---- */

static void sc_next_due_empty(void)
{
    setup();
    ET_CHECK_U32_EQ(PORT_TICK_WAIT_FOREVER, et_sched_next_due());
}

static void sc_next_due_single(void)
{
    static et_task_t t;

    setup();
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 100u));
    ET_CHECK_U32_EQ(100u, et_sched_next_due());         /* 注册时刻起算 */
    port_host_tick_advance(40u);
    ET_CHECK_U32_EQ(60u, et_sched_next_due());
    port_host_tick_advance(60u);
    ET_CHECK_U32_EQ(0u, et_sched_next_due());           /* 已到期 → 立即 poll */
}

static void sc_next_due_min_of_multi(void)
{
    static et_task_t ta;
    static et_task_t tb;

    setup();
    ET_CHECK(et_sched_register(&ta, cb_run_a, NULL, 100u));
    ET_CHECK(et_sched_register(&tb, cb_run_b, NULL, 30u));
    ET_CHECK_U32_EQ(30u, et_sched_next_due());          /* 取最近到期者 */
    port_host_tick_advance(30u);
    ET_CHECK_U32_EQ(0u, et_sched_next_due());           /* tb 已到期 → 0 */
    ET_CHECK(et_sched_unregister(&tb));
    ET_CHECK_U32_EQ(70u, et_sched_next_due());          /* 注销后看 ta */
}

static void sc_next_due_unregister_recalc(void)
{
    static et_task_t ta;
    static et_task_t tb;

    setup();
    ET_CHECK(et_sched_register(&ta, cb_run_a, NULL, 100u));
    ET_CHECK(et_sched_register(&tb, cb_run_b, NULL, 30u));
    ET_CHECK(et_sched_unregister(&tb));
    ET_CHECK_U32_EQ(100u, et_sched_next_due());         /* 注销后重算 */
}

static void sc_next_due_matches_poll(void)
{
    static et_task_t t;

    setup();
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 10u));
    port_host_tick_advance(10u);
    ET_CHECK_U32_EQ(0u, et_sched_next_due());
    et_sched_poll_once();                               /* poll 重锚定 last_run */
    ET_CHECK_U32_EQ(10u, et_sched_next_due());          /* 下一周期重新起算 */
    ET_CHECK_U32_EQ(1u, g_run_a);
}

static void sc_next_due_wraparound(void)
{
    static et_task_t t;

    setup();
    port_host_tick_set(0xFFFFFFF0u);                    /* 距回绕 16ms */
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 32u));
    ET_CHECK_U32_EQ(32u, et_sched_next_due());          /* 周期跨回绕仍正确 */
    port_host_tick_advance(0x20u);                      /* 回绕到 0x10 */
    ET_CHECK_U32_EQ(0u, et_sched_next_due());           /* elapsed=32(无符号) */
    et_sched_poll_once();
    ET_CHECK_U32_EQ(1u, g_run_a);
}

/* ===================== v2.2 任务耗时统计 ===================== */

static uint32_t g_run_slow;

static void cb_slow(void *arg)                          /* 任务体内推进虚拟时钟 */
{
    (void)arg;
    port_host_tick_advance(3u);
    g_run_slow++;
}

/* 可变耗时任务: 用 g_var_ms 模拟"第一次 5ms、之后 2ms"的真实负载波动 */
static uint32_t g_var_ms = 5u;

static void cb_var(void *arg)
{
    (void)arg;
    port_host_tick_advance(g_var_ms);
    g_run_b++;
}

static void sc_stats_not_run_is_zero(void)
{
    static et_task_t t;
    uint32_t last = 99u;
    uint32_t max  = 99u;

    setup();
    ET_CHECK(et_sched_register(&t, cb_run_a, NULL, 10u));
    et_sched_task_stats(&t, &last, &max);
    ET_CHECK_U32_EQ(0u, last);                          /* 未运行任务报 0 */
    ET_CHECK_U32_EQ(0u, max);
    et_sched_task_stats(&t, &last, NULL);               /* 输出指针可空 */
    ET_CHECK_U32_EQ(0u, last);
    et_sched_task_stats(&t, NULL, &max);
    ET_CHECK_U32_EQ(0u, max);
}

static void sc_stats_measures_duration(void)
{
    static et_task_t t;
    uint32_t last = 0u;
    uint32_t max  = 0u;

    setup();
    g_run_slow = 0u;
    ET_CHECK(et_sched_register(&t, cb_slow, NULL, 10u));
    port_host_tick_advance(10u);
    et_sched_poll_once();                               /* 任务内推进 3ms */
    ET_CHECK_U32_EQ(1u, g_run_slow);
    et_sched_task_stats(&t, &last, &max);
    ET_CHECK_U32_EQ(3u, last);
    ET_CHECK_U32_EQ(3u, max);
}

static void sc_stats_max_keeps_peak(void)
{
    static et_task_t t;
    uint32_t last = 0u;
    uint32_t max  = 0u;

    setup();
    g_var_ms = 5u;                                      /* 第一次: 5ms */
    ET_CHECK(et_sched_register(&t, cb_var, NULL, 10u));
    port_host_tick_advance(10u);
    et_sched_poll_once();
    et_sched_task_stats(&t, &last, &max);
    ET_CHECK_U32_EQ(5u, last);
    ET_CHECK_U32_EQ(5u, max);

    g_var_ms = 2u;                                      /* 第二次: 2ms —— max 保持峰值 */
    port_host_tick_advance(10u);
    et_sched_poll_once();
    et_sched_task_stats(&t, &last, &max);
    ET_CHECK_U32_EQ(2u, last);
    ET_CHECK_U32_EQ(5u, max);
    ET_CHECK_U32_EQ(2u, g_run_b);
}

static void sc_stats_reset(void)
{
    static et_task_t t;
    uint32_t last = 0u;
    uint32_t max  = 0u;

    setup();
    g_run_slow = 0u;
    ET_CHECK(et_sched_register(&t, cb_slow, NULL, 10u));
    port_host_tick_advance(10u);
    et_sched_poll_once();
    ET_CHECK_U32_EQ(3u, t.max_ms);

    et_sched_task_stats_reset(&t);
    et_sched_task_stats(&t, &last, &max);
    ET_CHECK_U32_EQ(0u, last);
    ET_CHECK_U32_EQ(0u, max);
    ET_CHECK_U32_EQ(1u, g_run_slow);                    /* 调度状态不受影响 */

    port_host_tick_advance(10u);
    et_sched_poll_once();                               /* reset 后重新累计 */
    et_sched_task_stats(&t, &last, &max);
    ET_CHECK_U32_EQ(3u, last);
    ET_CHECK_U32_EQ(3u, max);
}

static void sc_stats_multi_task_independent(void)
{
    static et_task_t ta;
    static et_task_t tb;
    uint32_t la = 0u;
    uint32_t ma = 0u;
    uint32_t lb = 0u;
    uint32_t mb = 0u;

    setup();
    g_var_ms = 5u;
    ET_CHECK(et_sched_register(&ta, cb_slow, NULL, 10u));   /* 3ms */
    ET_CHECK(et_sched_register(&tb, cb_var, NULL, 10u));    /* 5ms */
    /* 注: ta 执行会把虚拟时钟推进 3ms, 同拍的 tb 在本轮被跳过 ——
     * 与真机"任务耗时会挤压同拍后续任务"的行为一致, 由下一拍补跑 */
    port_host_tick_advance(10u);
    et_sched_poll_once();                               /* ta 跑, tb 被挤压 */
    port_host_tick_advance(10u);
    et_sched_poll_once();                               /* ta 再跑, tb 补跑 */
    et_sched_task_stats(&ta, &la, &ma);
    et_sched_task_stats(&tb, &lb, &mb);
    ET_CHECK_U32_EQ(3u, la);
    ET_CHECK_U32_EQ(3u, ma);
    ET_CHECK_U32_EQ(5u, lb);
    ET_CHECK_U32_EQ(5u, mb);
}

static void sc_stats_reregister_restarts(void)
{
    static et_task_t t;
    uint32_t last = 0u;
    uint32_t max  = 0u;

    setup();
    ET_CHECK(et_sched_register(&t, cb_slow, NULL, 10u));
    port_host_tick_advance(10u);
    et_sched_poll_once();
    ET_CHECK_U32_EQ(3u, t.max_ms);
    ET_CHECK(et_sched_unregister(&t));
    ET_CHECK(et_sched_register(&t, cb_slow, NULL, 10u));    /* 重注册: 统计重新起算 */
    et_sched_task_stats(&t, &last, &max);
    ET_CHECK_U32_EQ(0u, last);
    ET_CHECK_U32_EQ(0u, max);
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
        {"sched.next_due_empty",       sc_next_due_empty},
        {"sched.next_due_single",      sc_next_due_single},
        {"sched.next_due_min_multi",   sc_next_due_min_of_multi},
        {"sched.next_due_recalc",      sc_next_due_unregister_recalc},
        {"sched.next_due_match_poll",  sc_next_due_matches_poll},
        {"sched.next_due_wraparound",  sc_next_due_wraparound},
        {"sched.stats_not_run_zero",   sc_stats_not_run_is_zero},
        {"sched.stats_duration",       sc_stats_measures_duration},
        {"sched.stats_max_peak",       sc_stats_max_keeps_peak},
        {"sched.stats_reset",          sc_stats_reset},
        {"sched.stats_independent",    sc_stats_multi_task_independent},
        {"sched.stats_reregister",     sc_stats_reregister_restarts},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

/**
 * @file    test_stimer.c
 * @brief   et_stimer 单元测试 (虚拟时间驱动, 完全确定性)
 */
#include "et_test.h"
#include "et_stimer.h"
#include "port_host.h"

static uint32_t g_cnt_a;
static uint32_t g_cnt_b;
static void    *g_last_arg;

static void cb_inc_a(void *arg)
{
    g_cnt_a++;
    g_last_arg = arg;
}

static void cb_inc_b(void *arg)
{
    g_cnt_b++;
    g_last_arg = arg;
}

/* 回调内停止自身与另一只定时器 */
static et_stimer_t g_tmr_self;
static et_stimer_t g_tmr_other;
static void        cb_stop_both(void *arg)
{
    (void)arg;
    g_cnt_a++;
    (void)et_stimer_stop(&g_tmr_self);
    (void)et_stimer_stop(&g_tmr_other);
}

static void setup(void)
{
    et_stimer_reset_all();
    port_host_tick_set(0u);
    g_cnt_a = 0u;
    g_cnt_b = 0u;
    g_last_arg = NULL;
}

static void st_init_and_start_state(void)
{
    static et_stimer_t t;

    setup();
    ET_CHECK(et_stimer_init(&t, cb_inc_a, NULL));
    ET_CHECK(!et_stimer_is_running(&t));
    ET_CHECK(et_stimer_start_oneshot(&t, 100u));
    ET_CHECK(et_stimer_is_running(&t));
}

static void st_init_rejects_bad(void)
{
    static et_stimer_t t;

    setup();
    ET_CHECK(!et_stimer_init(NULL, cb_inc_a, NULL));
    ET_CHECK(!et_stimer_init(&t, NULL, NULL));
    /* 参数非法时启动失败 */
    ET_CHECK(et_stimer_init(&t, cb_inc_a, NULL));
    ET_CHECK(!et_stimer_start_oneshot(&t, 0u));
    ET_CHECK(!et_stimer_start_periodic(&t, 0u));
    ET_CHECK(!et_stimer_is_running(&t));
}

static void st_oneshot_exact_timing(void)
{
    static et_stimer_t t;
    int                magic = 42;
    uint32_t           i;

    setup();
    ET_CHECK(et_stimer_init(&t, cb_inc_a, &magic));
    ET_CHECK(et_stimer_start_oneshot(&t, 100u));

    for (i = 0u; i < 19u; i++) {                    /* 推进到第 95 ms */
        port_host_tick_advance(5u);
        et_stimer_poll(port_host_tick_now());
        ET_CHECK_U32_EQ(0u, g_cnt_a);
    }
    port_host_tick_advance(5u);                     /* 第 100 ms 到期 */
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(1u, g_cnt_a);
    ET_CHECK(g_last_arg == &magic);
    ET_CHECK(!et_stimer_is_running(&t));            /* 单次触发后自停 */

    port_host_tick_advance(1000u);                  /* 不应再次触发 */
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(1u, g_cnt_a);
}

static void st_periodic_repeat(void)
{
    static et_stimer_t t;

    setup();
    ET_CHECK(et_stimer_init(&t, cb_inc_a, NULL));
    ET_CHECK(et_stimer_start_periodic(&t, 50u));

    port_host_tick_advance(200u);
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(4u, g_cnt_a);                   /* 50/100/150/200 共 4 次 */
    ET_CHECK(et_stimer_is_running(&t));
}

static void st_periodic_catchup(void)
{
    static et_stimer_t t;

    setup();
    ET_CHECK(et_stimer_init(&t, cb_inc_a, NULL));
    ET_CHECK(et_stimer_start_periodic(&t, 50u));

    port_host_tick_advance(160u);                   /* 错过多个周期 */
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(3u, g_cnt_a);                   /* 追赶补发 50/100/150 三次 */

    port_host_tick_advance(50u);                    /* 之后恢复正常节奏 */
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(4u, g_cnt_a);
}

static void st_stop_prevents_fire(void)
{
    static et_stimer_t t;

    setup();
    ET_CHECK(et_stimer_init(&t, cb_inc_a, NULL));
    ET_CHECK(et_stimer_start_oneshot(&t, 30u));
    ET_CHECK(et_stimer_stop(&t));
    ET_CHECK(!et_stimer_is_running(&t));
    ET_CHECK(!et_stimer_stop(&t));                  /* 重复停止返回 false */

    port_host_tick_advance(1000u);
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(0u, g_cnt_a);
}

static void st_restart_rearm(void)
{
    static et_stimer_t t;

    setup();
    ET_CHECK(et_stimer_init(&t, cb_inc_a, NULL));
    ET_CHECK(et_stimer_start_oneshot(&t, 20u));
    port_host_tick_advance(10u);
    ET_CHECK(et_stimer_start_oneshot(&t, 20u));     /* 运行中重启 => 重新计时 */

    port_host_tick_advance(15u);                    /* 距原到期点已过, 但距新锚点未到 */
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(0u, g_cnt_a);

    port_host_tick_advance(5u);
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(1u, g_cnt_a);
}

static void st_multi_timers_independent(void)
{
    static et_stimer_t ta;
    static et_stimer_t tb;

    setup();
    ET_CHECK(et_stimer_init(&ta, cb_inc_a, NULL));
    ET_CHECK(et_stimer_init(&tb, cb_inc_b, NULL));
    ET_CHECK(et_stimer_start_periodic(&ta, 10u));
    ET_CHECK(et_stimer_start_oneshot(&tb, 35u));

    port_host_tick_advance(35u);
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(3u, g_cnt_a);                   /* 10/20/30 */
    ET_CHECK_U32_EQ(1u, g_cnt_b);                   /* 35       */
}

static void st_callback_mutates_list(void)
{
    setup();
    /* 先启动 other 再启动 self => 链表顺序 self 在前, 保证 self 先被分发 */
    ET_CHECK(et_stimer_init(&g_tmr_self, cb_stop_both, NULL));
    ET_CHECK(et_stimer_init(&g_tmr_other, cb_inc_b, NULL));
    ET_CHECK(et_stimer_start_oneshot(&g_tmr_other, 60u));
    ET_CHECK(et_stimer_start_periodic(&g_tmr_self, 20u));

    port_host_tick_advance(80u);
    et_stimer_poll(port_host_tick_now());

    ET_CHECK_U32_EQ(1u, g_cnt_a);                   /* 自停在第一次触发后 */
    ET_CHECK_U32_EQ(0u, g_cnt_b);                   /* other 在触发前被摘除 */
    ET_CHECK(!et_stimer_is_running(&g_tmr_self));
    ET_CHECK(!et_stimer_is_running(&g_tmr_other));
}

static void st_u32_wraparound(void)
{
    static et_stimer_t t;

    setup();
    port_host_tick_set(0xFFFFFF00u);                /* 距回绕仅 256 ms */
    ET_CHECK(et_stimer_init(&t, cb_inc_a, NULL));
    ET_CHECK(et_stimer_start_periodic(&t, 0x40u));  /* 周期 64 ms */

    port_host_tick_advance(0x80u);                  /* 跨越 uint32 回绕 */
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(2u, g_cnt_a);                   /* 追赶补发 +64/+128 两周期 */

    port_host_tick_advance(0xC0u);                  /* 回绕后再补发三周期 */
    et_stimer_poll(port_host_tick_now());
    ET_CHECK_U32_EQ(5u, g_cnt_a);
}

const et_test_case_t *test_stimer_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"stimer.init_and_start",      st_init_and_start_state},
        {"stimer.init_rejects_bad",    st_init_rejects_bad},
        {"stimer.oneshot_exact",       st_oneshot_exact_timing},
        {"stimer.periodic_repeat",     st_periodic_repeat},
        {"stimer.periodic_catchup",    st_periodic_catchup},
        {"stimer.stop_prevents",       st_stop_prevents_fire},
        {"stimer.restart_rearm",       st_restart_rearm},
        {"stimer.multi_independent",   st_multi_timers_independent},
        {"stimer.cb_mutates_list",     st_callback_mutates_list},
        {"stimer.u32_wraparound",      st_u32_wraparound},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

/**
 * @file    test_key.c
 * @brief   et_key 单元测试 (虚拟时间 + 事件序列断言)
 */
#include "et_test.h"
#include "et_key.h"
#include "port_host.h"

#define MAX_EV 32

static bool            g_level;
static et_key_event_t  g_ev[MAX_EV];
static uint32_t        g_ev_n;

static bool read_level(void *user)
{
    (void)user;
    return g_level;
}

static void on_event(struct et_key *k, et_key_event_t ev, void *user)
{
    (void)k;
    (void)user;
    if (g_ev_n < MAX_EV) {
        g_ev[g_ev_n++] = ev;
    }
}

/* 以 5ms 步进扫描 ms 毫秒 */
static void scan_for(et_key_t *k, uint32_t ms)
{
    uint32_t i;

    for (i = 0u; i < ms; i += 5u) {
        port_host_tick_advance(5u);
        et_key_scan(k, port_host_tick_now());
    }
}

static bool ev_is(uint32_t idx, et_key_event_t e)
{
    return (idx < g_ev_n) && (g_ev[idx] == e);
}

static void setup_key(et_key_t *k, const et_key_params_t *prm)
{
    g_level = false;
    g_ev_n  = 0u;
    ET_CHECK(et_key_init(k, read_level, on_event, NULL, prm));
}

static void key_init_rejects_bad(void)
{
    et_key_t k;

    g_level = false;
    g_ev_n  = 0u;
    ET_CHECK(!et_key_init(NULL, read_level, on_event, NULL, NULL));
    ET_CHECK(!et_key_init(&k, NULL, on_event, NULL, NULL));
    ET_CHECK(!et_key_init(&k, read_level, NULL, NULL, NULL));
}

static void key_clean_click(void)
{
    et_key_t k;
    et_key_params_t prm = { 20u, 600u, 0u };

    setup_key(&k, &prm);
    g_level = true;
    scan_for(&k, 60u);
    g_level = false;
    scan_for(&k, 40u);

    ET_CHECK_U32_EQ(3u, g_ev_n);
    ET_CHECK(ev_is(0u, ET_KEY_PRESS));      /* 约 20ms 处确认按下     */
    ET_CHECK(ev_is(1u, ET_KEY_RELEASE));    /* 先确认释放             */
    ET_CHECK(ev_is(2u, ET_KEY_CLICK));      /* 再结算短按             */
}

static void key_glitch_filtered(void)
{
    et_key_t k;
    et_key_params_t prm = { 20u, 600u, 0u };

    setup_key(&k, &prm);
    g_level = true;
    scan_for(&k, 15u);                      /* 小于消抖时间的毛刺 */
    g_level = false;
    scan_for(&k, 40u);
    ET_CHECK_U32_EQ(0u, g_ev_n);

    /* 按压过程中的瞬时断开同样被过滤 */
    g_level = true;
    scan_for(&k, 60u);                      /* 稳定按下           */
    g_level = false;
    scan_for(&k, 10u);                      /* 瞬间抖动           */
    g_level = true;
    scan_for(&k, 40u);
    g_level = false;
    scan_for(&k, 40u);

    ET_CHECK_U32_EQ(3u, g_ev_n);            /* 只有一次完整按压   */
    ET_CHECK(ev_is(0u, ET_KEY_PRESS));
    ET_CHECK(ev_is(1u, ET_KEY_RELEASE));
    ET_CHECK(ev_is(2u, ET_KEY_CLICK));
}

static void key_long_press_no_click(void)
{
    et_key_t k;
    et_key_params_t prm = { 20u, 600u, 0u };
    uint32_t i;

    setup_key(&k, &prm);
    g_level = true;
    scan_for(&k, 700u);
    g_level = false;
    scan_for(&k, 40u);

    ET_CHECK_U32_EQ(3u, g_ev_n);
    ET_CHECK(ev_is(0u, ET_KEY_PRESS));
    ET_CHECK(ev_is(1u, ET_KEY_LONG_PRESS));
    ET_CHECK(ev_is(2u, ET_KEY_RELEASE));
    for (i = 0u; i < g_ev_n; i++) {
        ET_CHECK(g_ev[i] != ET_KEY_CLICK);  /* 长按不产生短按事件 */
    }
}

static void key_repeat_sequence(void)
{
    et_key_t k;
    et_key_params_t prm = { 20u, 200u, 100u };

    setup_key(&k, &prm);
    g_level = true;
    scan_for(&k, 525u);                     /* 按住至 ~525ms */
    g_level = false;
    scan_for(&k, 40u);

    /* 时间线(5ms 步进): PRESS@20, LONG@220, REPEAT@320/420/520, RELEASE@550 */
    ET_CHECK_U32_EQ(6u, g_ev_n);
    ET_CHECK(ev_is(0u, ET_KEY_PRESS));
    ET_CHECK(ev_is(1u, ET_KEY_LONG_PRESS));
    ET_CHECK(ev_is(2u, ET_KEY_REPEAT));
    ET_CHECK(ev_is(3u, ET_KEY_REPEAT));
    ET_CHECK(ev_is(4u, ET_KEY_REPEAT));
    ET_CHECK(ev_is(5u, ET_KEY_RELEASE));
}

static void key_repeat_disabled(void)
{
    et_key_t k;
    et_key_params_t prm = { 20u, 200u, 0u };    /* repeat=0 关闭连发 */

    setup_key(&k, &prm);
    g_level = true;
    scan_for(&k, 900u);
    g_level = false;
    scan_for(&k, 40u);

    ET_CHECK_U32_EQ(3u, g_ev_n);
    ET_CHECK(ev_is(0u, ET_KEY_PRESS));
    ET_CHECK(ev_is(1u, ET_KEY_LONG_PRESS));
    ET_CHECK(ev_is(2u, ET_KEY_RELEASE));
}

const et_test_case_t *test_key_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"key.init_rejects_bad",  key_init_rejects_bad},
        {"key.clean_click",       key_clean_click},
        {"key.glitch_filtered",   key_glitch_filtered},
        {"key.long_no_click",     key_long_press_no_click},
        {"key.repeat_sequence",   key_repeat_sequence},
        {"key.repeat_disabled",   key_repeat_disabled},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

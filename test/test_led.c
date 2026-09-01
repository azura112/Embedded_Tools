/**
 * @file    test_led.c
 * @brief   et_led 单元测试 (虚拟时间采样亮度曲线)
 */
#include "et_test.h"
#include "et_led.h"
#include "port_host.h"

#define MAX_OUT 512

static uint8_t  g_out[MAX_OUT];
static uint32_t g_out_n;
static uint32_t g_write_calls;

static void write_rec(void *user, uint8_t v)
{
    (void)user;
    g_write_calls++;
    if (g_out_n < MAX_OUT) {
        g_out[g_out_n++] = v;
    }
}

static void setup_led(et_led_t *l)
{
    g_out_n = 0u;
    g_write_calls = 0u;
    ET_CHECK(et_led_init(l, write_rec, NULL));
}

/* 推进并逐毫秒刷新(最细粒度, 相位边界无歧义) */
static void poll_for(et_led_t *l, uint32_t ms)
{
    uint32_t i;

    for (i = 0u; i < ms; i++) {
        port_host_tick_advance(1u);
        et_led_poll(l, port_host_tick_now());
    }
}

static uint8_t out_at(uint32_t idx)
{
    return (idx < g_out_n) ? g_out[idx] : 0xFFu;
}

static void led_on_off_cache(void)
{
    et_led_t l;

    setup_led(&l);
    et_led_set_on(&l);
    poll_for(&l, 5u);
    ET_CHECK_U32_EQ(1u, g_write_calls);     /* 仅首次输出 255 */
    ET_CHECK_U32_EQ(255u, out_at(0u));

    et_led_set_off(&l);
    poll_for(&l, 5u);
    ET_CHECK_U32_EQ(2u, g_write_calls);     /* 再输出一次 0 */
    ET_CHECK_U32_EQ(0u, out_at(1u));

    poll_for(&l, 10u);
    ET_CHECK_U32_EQ(2u, g_write_calls);     /* 缓存生效, 无冗余输出 */
}

static void led_blink_timeline(void)
{
    et_led_t l;

    setup_led(&l);
    ET_CHECK(et_led_set_blink(&l, 100u, 30u, 0u));  /* 点亮 30ms/周期 */

    poll_for(&l, 250u);                     /* 覆盖两个半周期 */

    /* 变化沿序列: 亮@0 -> 灭@30 -> 亮@100 -> 灭@130 -> 亮@200 -> 灭@230 */
    ET_CHECK_U32_EQ(6u, g_out_n);
    ET_CHECK_U32_EQ(255u, out_at(0u));
    ET_CHECK_U32_EQ(0u,   out_at(1u));
    ET_CHECK_U32_EQ(255u, out_at(2u));
    ET_CHECK_U32_EQ(0u,   out_at(3u));
    ET_CHECK_U32_EQ(255u, out_at(4u));
    ET_CHECK_U32_EQ(0u,   out_at(5u));
}

static void led_blink_n_stops(void)
{
    et_led_t l;

    setup_led(&l);
    ET_CHECK(et_led_set_blink(&l, 100u, 50u, 3u));

    poll_for(&l, 500u);                     /* 3 个完整周期后应熄灭 */

    /* 序列: 255,0,255,0,255,0 之后不再变化 */
    ET_CHECK_U32_EQ(6u, g_out_n);
    ET_CHECK_U32_EQ(255u, out_at(0u));
    ET_CHECK_U32_EQ(255u, out_at(2u));
    ET_CHECK_U32_EQ(255u, out_at(4u));
    ET_CHECK_U32_EQ(0u,   out_at(5u));

    {
        uint32_t before = g_write_calls;

        poll_for(&l, 300u);                 /* 继续轮询保持安静 */
        ET_CHECK_U32_EQ(before, g_write_calls);
    }
}

static void led_breath_ramp(void)
{
    et_led_t l;

    setup_led(&l);
    ET_CHECK(et_led_set_breath(&l, 200u));  /* 半周期 100ms */

    poll_for(&l, 400u);                     /* 两个完整呼吸周期 */

    /* 整数三角波斜率约 2~3/ms, 逐毫秒轮询时几乎每次都是新亮度 */
    ET_CHECK(g_out_n >= 380u);

    /* 首次 poll 锁定 t0 后 elapsed 从 0 起: 第 k 次 poll 对应 elapsed=k-1 */
    ET_CHECK_U32_EQ(0u,   out_at(0u));      /* e=0   => 起始熄灭       */
    ET_CHECK_U32_EQ(255u, out_at(100u));    /* e=100 => 峰值           */
    ET_CHECK_U32_EQ(0u,   out_at(200u));    /* e=200 => 谷值/周期回绕  */

    /* 第二周期波形与第一周期完全重复 */
    {
        uint32_t i;

        for (i = 0u; (i < 195u) && ((200u + i) < g_out_n); i++) {
            ET_CHECK(out_at(i) == out_at(200u + i));
        }
        ET_CHECK(i >= 150u);                /* 确认比较了足够多的样本 */
    }
}

static void led_invalid_params(void)
{
    et_led_t l;

    setup_led(&l);
    ET_CHECK(!et_led_set_blink(&l, 5u, 50u, 0u));    /* 周期过短 */
    ET_CHECK(!et_led_set_blink(&l, 100u, 0u, 0u));   /* 占空比 0 */
    ET_CHECK(!et_led_set_blink(&l, 100u, 100u, 0u)); /* 占空比 100 */
    ET_CHECK(!et_led_set_breath(&l, 50u));           /* 呼吸周期过短 */
    ET_CHECK(et_led_set_blink(&l, 100u, 50u, 0u));   /* 合法参数通过 */
}

static void led_mode_switch_rebase(void)
{
    et_led_t l;

    setup_led(&l);
    ET_CHECK(et_led_set_blink(&l, 100u, 50u, 0u));
    poll_for(&l, 70u);                      /* 已经历一个熄灭沿(@50ms) */
    ET_CHECK_U32_EQ(2u, g_out_n);

    et_led_set_on(&l);
    poll_for(&l, 10u);
    ET_CHECK_U32_EQ(3u, g_out_n);
    ET_CHECK_U32_EQ(255u, out_at(2u));

    et_led_set_blink(&l, 100u, 50u, 0u);    /* 重新进入闪烁: 相位重置 */
    poll_for(&l, 120u);
    /* 新相位从 0 起: @50 熄灭沿, @100 恢复点亮 */
    ET_CHECK_U32_EQ(5u, g_out_n);
    ET_CHECK_U32_EQ(0u,   out_at(3u));
    ET_CHECK_U32_EQ(255u, out_at(4u));
}

const et_test_case_t *test_led_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"led.on_off_cache",     led_on_off_cache},
        {"led.blink_timeline",   led_blink_timeline},
        {"led.blink_n_stops",    led_blink_n_stops},
        {"led.breath_ramp",      led_breath_ramp},
        {"led.invalid_params",   led_invalid_params},
        {"led.mode_switch",      led_mode_switch_rebase},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

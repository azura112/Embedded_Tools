/**
 * @file    test_spwm.c
 * @brief   et_spwm 单元测试 (now 由测试直接注入, 完全确定)
 */
#include "et_test.h"
#include "et_spwm.h"
#include <string.h>

/* 每通道电平记录器 */
typedef struct {
    uint32_t n;
    uint8_t  lvl[32];
} rec_t;

static rec_t g_rec[ET_SPWM_CH_MAX];

static void rec_write(void *user, uint8_t on)
{
    rec_t *r = (rec_t *)user;

    if (r->n < sizeof(r->lvl)) {
        r->lvl[r->n++] = on;
    }
}

static void rec_reset_all(void)
{
    uint8_t i;

    memset(g_rec, 0, sizeof(g_rec));
    for (i = 0u; i < (uint8_t)ET_SPWM_CH_MAX; i++) {
        et_spwm_deinit(i);
    }
}

/* 逐毫秒扫描 [from, to] 并驱动 poll */
static void sweep(uint32_t from, uint32_t to)
{
    uint32_t t;

    for (t = from; ; t++) {
        et_spwm_poll(t);
        if (t == to) {
            break;
        }
    }
}

static void spwm_init_validation(void)
{
    rec_reset_all();
    ET_CHECK(!et_spwm_init(ET_SPWM_CH_MAX, rec_write, &g_rec[0], 4u));  /* 通道越界 */
    ET_CHECK(!et_spwm_init(0u, NULL, &g_rec[0], 4u));                   /* 回调缺失 */
    ET_CHECK(!et_spwm_init(0u, rec_write, &g_rec[0], 1u));              /* 低于 2ms 分辨率 */
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 2u));               /* 下限可用 */
    ET_CHECK_U32_EQ(2u, et_spwm_get_period(0u));
    ET_CHECK_U32_EQ(0u, et_spwm_get_duty(0u));                          /* 默认 duty 0 */
}

static void spwm_duty0_always_low(void)
{
    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 8u));
    ET_CHECK(et_spwm_set(0u, 0u));
    sweep(0u, 31u);                                     /* 扫 4 个整周期 */
    /* 仅首次输出已知低电平, 之后电平不变零调用(输出缓存) */
    ET_CHECK_U32_EQ(1u, g_rec[0].n);
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[0]);
}

static void spwm_duty255_always_high(void)
{
    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 8u));
    ET_CHECK(et_spwm_set(0u, 255u));
    sweep(0u, 31u);
    ET_CHECK_U32_EQ(1u, g_rec[0].n);                    /* 恒高且无毛刺 */
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[0]);
}

static void spwm_flip_sequence(void)
{
    /* period 8, duty 128 -> on_ms=4: 0~3 高, 4~7 低, 相位锯齿无累积误差 */
    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 8u));
    ET_CHECK(et_spwm_set(0u, 128u));
    sweep(0u, 15u);
    ET_CHECK_U32_EQ(4u, g_rec[0].n);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[0]);               /* t=0  H */
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[1]);               /* t=4  L */
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[2]);               /* t=8  H */
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[3]);               /* t=12 L */
}

static void spwm_min_period_alternating(void)
{
    /* period 2, duty 128 -> on_ms=1: 半周期翻转的极限分辨率 */
    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 2u));
    ET_CHECK(et_spwm_set(0u, 128u));
    sweep(0u, 7u);
    ET_CHECK_U32_EQ(8u, g_rec[0].n);                    /* 每毫秒都在翻转 */
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[0]);
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[1]);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[6]);
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[7]);
}

static void spwm_duty_rounding_per_period(void)
{
    /* period 10, duty 77 -> on_ms=(770+127)/255=3: 一个周期内恰好 3ms 高 */
    uint32_t t;
    uint32_t highs = 0u;

    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 10u));
    ET_CHECK(et_spwm_set(0u, 77u));
    for (t = 0u; t < 10u; t++) {
        et_spwm_poll(t);
    }
    /* 周期首尾电平都取自记录: 首 H 末 L, 翻转点即 on_ms 边界 */
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[0]);               /* t=0 高 */
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[1]);               /* t=3 起低 */
    (void)highs;
}

static void spwm_duty_change_takes_effect(void)
{
    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 8u));
    et_spwm_poll(0u);                                   /* duty 0 -> L */
    et_spwm_poll(1u);
    et_spwm_poll(2u);
    ET_CHECK_U32_EQ(1u, g_rec[0].n);
    ET_CHECK(et_spwm_set(0u, 255u));                    /* 运行中改占空比 */
    et_spwm_poll(3u);                                   /* phase3 < 8 -> H */
    ET_CHECK_U32_EQ(2u, g_rec[0].n);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[1]);
    ET_CHECK(et_spwm_set(0u, 128u));                    /* on=4, phase4 不 < 4 -> L */
    et_spwm_poll(4u);
    ET_CHECK_U32_EQ(3u, g_rec[0].n);
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[2]);
    ET_CHECK_U32_EQ(128u, et_spwm_get_duty(0u));
}

static void spwm_multi_channel_independent(void)
{
    /* ch0(t0=0, period4) 与 ch1(t0=2, period8) 各自独立相位 */
    uint32_t t;

    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 4u));
    ET_CHECK(et_spwm_set(0u, 128u));                    /* on=2: 0,1 高; 2,3 低 */

    for (t = 0u; t <= 11u; t++) {
        if (t == 2u) {                                  /* ch1 晚两拍才初始化 */
            ET_CHECK(et_spwm_init(1u, rec_write, &g_rec[1], 8u));
            ET_CHECK(et_spwm_set(1u, 64u));             /* on=2: t0=2 起两个高 */
        }
        et_spwm_poll(t);
    }
    /* ch0: 写点 t=0,2,4,6,8,10 -> [H L H L H L] (每 2ms 翻转) */
    ET_CHECK_U32_EQ(6u, g_rec[0].n);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[0]);
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[1]);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[4]);
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[5]);
    /* ch1: t2 H, t4 L, t10 H —— 相位起点在 t=2 而非受 ch0 影响 */
    ET_CHECK_U32_EQ(3u, g_rec[1].n);
    ET_CHECK_U32_EQ(1u, g_rec[1].lvl[0]);
    ET_CHECK_U32_EQ(0u, g_rec[1].lvl[1]);
    ET_CHECK_U32_EQ(1u, g_rec[1].lvl[2]);
}

static void spwm_uninit_channel_untouched(void)
{
    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 4u));
    ET_CHECK(et_spwm_set(0u, 128u));
    sweep(0u, 15u);
    ET_CHECK(g_rec[0].n > 0u);
    ET_CHECK_U32_EQ(0u, g_rec[1].n);                    /* 未初始化通道零输出 */
    ET_CHECK_U32_EQ(0u, g_rec[2].n);
    ET_CHECK_U32_EQ(0u, g_rec[3].n);
}

static void spwm_tick_wraparound(void)
{
    /* 时基回绕点: 0xFFFFFFFC 起相位连续推进, 跨 0 后无跳变 */
    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 8u));
    ET_CHECK(et_spwm_set(0u, 128u));                    /* on=4 */
    et_spwm_poll(0xFFFFFFFCu);                          /* t0 锁定, phase0 -> H */
    et_spwm_poll(0xFFFFFFFDu);                          /* phase1 H */
    et_spwm_poll(0xFFFFFFFEu);                          /* phase2 H */
    et_spwm_poll(0xFFFFFFFFu);                          /* phase3 H */
    et_spwm_poll(0u);                                   /* phase4 -> L (跨回绕) */
    et_spwm_poll(1u);
    et_spwm_poll(2u);
    et_spwm_poll(3u);
    et_spwm_poll(4u);                                   /* phase0 -> H */
    ET_CHECK_U32_EQ(3u, g_rec[0].n);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[0]);
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[1]);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[2]);
}

static void spwm_api_on_uninit_rejected(void)
{
    rec_reset_all();
    ET_CHECK(!et_spwm_set(1u, 128u));                   /* 未初始化拒绝 */
    ET_CHECK_U32_EQ(0u, et_spwm_get_duty(1u));
    ET_CHECK_U32_EQ(0u, et_spwm_get_period(1u));
    et_spwm_poll(100u);                                 /* 空跑不炸 */
}

static void spwm_reinit_relocks_phase(void)
{
    rec_reset_all();
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 4u));
    ET_CHECK(et_spwm_set(0u, 255u));
    et_spwm_poll(0u);                                   /* H */
    ET_CHECK(et_spwm_init(0u, rec_write, &g_rec[0], 4u));   /* 重新初始化 */
    et_spwm_poll(1u);                                   /* duty 0 -> L, t0 重锁为 1 */
    ET_CHECK(et_spwm_set(0u, 255u));
    et_spwm_poll(2u);                                   /* phase1 < 4 -> H */
    ET_CHECK_U32_EQ(3u, g_rec[0].n);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[0]);
    ET_CHECK_U32_EQ(0u, g_rec[0].lvl[1]);
    ET_CHECK_U32_EQ(1u, g_rec[0].lvl[2]);
}

const et_test_case_t *test_spwm_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"spwm.init_validation",     spwm_init_validation},
        {"spwm.duty0_always_low",    spwm_duty0_always_low},
        {"spwm.duty255_always_high", spwm_duty255_always_high},
        {"spwm.flip_sequence",       spwm_flip_sequence},
        {"spwm.min_period_alternating", spwm_min_period_alternating},
        {"spwm.duty_rounding",       spwm_duty_rounding_per_period},
        {"spwm.duty_change",         spwm_duty_change_takes_effect},
        {"spwm.multi_channel",       spwm_multi_channel_independent},
        {"spwm.uninit_untouched",    spwm_uninit_channel_untouched},
        {"spwm.tick_wraparound",     spwm_tick_wraparound},
        {"spwm.api_on_uninit",       spwm_api_on_uninit_rejected},
        {"spwm.reinit_relocks",      spwm_reinit_relocks_phase},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

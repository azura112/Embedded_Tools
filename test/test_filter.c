/**
 * @file    test_filter.c
 * @brief   et_filter 单元测试 (期望值均为手算定点数值)
 */
#include "et_test.h"
#include "et_filter.h"

/* ===================== 滑动窗口均值 ===================== */

static int32_t g_ma_buf[8];
static et_movavg_t g_ma;

static void movavg_init_validation(void)
{
    ET_CHECK(!et_movavg_init(NULL, g_ma_buf, 4u));
    ET_CHECK(!et_movavg_init(&g_ma, NULL, 4u));
    ET_CHECK(!et_movavg_init(&g_ma, g_ma_buf, 0u));     /* 窗口至少 1 */
    ET_CHECK(et_movavg_init(&g_ma, g_ma_buf, 4u));
    ET_CHECK_U32_EQ(4u, et_movavg_window(&g_ma));
    ET_CHECK_U32_EQ(0u, et_movavg_count(&g_ma));
}

static void movavg_partial_window_rolling(void)
{
    /* 窗口未满时输出"已有样本均值", 免开机长等待 */
    ET_CHECK(et_movavg_init(&g_ma, g_ma_buf, 4u));
    ET_CHECK_U32_EQ(10, et_movavg_update(&g_ma, 10));
    ET_CHECK_U32_EQ(15, et_movavg_update(&g_ma, 20));   /* (10+20)/2 */
    ET_CHECK_U32_EQ(20, et_movavg_update(&g_ma, 30));   /* (10+20+30)/3 */
    ET_CHECK_U32_EQ(25, et_movavg_update(&g_ma, 40));   /* 全窗: 100/4 */
    ET_CHECK_U32_EQ(4u, et_movavg_count(&g_ma));
}

static void movavg_window_full_overwrite(void)
{
    ET_CHECK(et_movavg_init(&g_ma, g_ma_buf, 3u));
    (void)et_movavg_update(&g_ma, 1);
    (void)et_movavg_update(&g_ma, 2);
    ET_CHECK_U32_EQ(2, et_movavg_update(&g_ma, 3));     /* 1,2,3 */
    ET_CHECK_U32_EQ(3, et_movavg_update(&g_ma, 4));     /* 2,3,4 */
    ET_CHECK_U32_EQ(4, et_movavg_update(&g_ma, 5));     /* 3,4,5 */
    ET_CHECK_U32_EQ(5, et_movavg_update(&g_ma, 6));     /* 4,5,6 */
}

static void movavg_rounding_half_away(void)
{
    ET_CHECK(et_movavg_init(&g_ma, g_ma_buf, 2u));
    ET_CHECK_U32_EQ(1, et_movavg_update(&g_ma, 1));     /* 单样本: 均值 1 */
    ET_CHECK_U32_EQ(2, et_movavg_update(&g_ma, 2));     /* 1.5 -> 2 (远离零) */

    et_movavg_reset(&g_ma);
    (void)et_movavg_update(&g_ma, 1);
    ET_CHECK_U32_EQ(1, et_movavg_update(&g_ma, 1));     /* 整数不漂移 */

    et_movavg_reset(&g_ma);
    (void)et_movavg_update(&g_ma, -1);
    ET_CHECK_U32_EQ((uint32_t)-2, (uint32_t)et_movavg_update(&g_ma, -2)); /* -1.5 -> -2 */
}

static void movavg_negative_samples(void)
{
    ET_CHECK(et_movavg_init(&g_ma, g_ma_buf, 4u));
    (void)et_movavg_update(&g_ma, -10);
    ET_CHECK_U32_EQ(0, et_movavg_update(&g_ma, 10));    /* -10,10 均值 0 */
    ET_CHECK_U32_EQ((uint32_t)-7, (uint32_t)et_movavg_update(&g_ma, -20)); /* -20/3 -> -7 */
    ET_CHECK_U32_EQ(5,  et_movavg_update(&g_ma, 40));   /* 20/4 */
}

static void movavg_reset_semantics(void)
{
    ET_CHECK(et_movavg_init(&g_ma, g_ma_buf, 3u));
    (void)et_movavg_update(&g_ma, 1);
    (void)et_movavg_update(&g_ma, 2);
    (void)et_movavg_update(&g_ma, 3);
    et_movavg_reset(&g_ma);
    ET_CHECK_U32_EQ(0u, et_movavg_count(&g_ma));
    ET_CHECK_U32_EQ(7, et_movavg_update(&g_ma, 7));     /* 复位后从空窗口重新计 */
    ET_CHECK_U32_EQ(7, et_movavg_update(&g_ma, 7));
    ET_CHECK_U32_EQ(7, et_movavg_update(&g_ma, 7));
}

/* ===================== 一阶 IIR 低通 ===================== */

static et_lpf1_t g_lp;

static void lpf1_init_validation(void)
{
    ET_CHECK(!et_lpf1_init(NULL, 100u));
    ET_CHECK(et_lpf1_init(&g_lp, 0u));                  /* k 边界: 0 合法 */
    ET_CHECK(et_lpf1_init(&g_lp, 32767u));              /* k 边界: 最大合法 */
    ET_CHECK_U32_EQ(0, (uint32_t)et_lpf1_output(&g_lp));
}

static void lpf1_first_sample_passthrough(void)
{
    ET_CHECK(et_lpf1_init(&g_lp, 16384u));
    ET_CHECK_U32_EQ(1000, (uint32_t)et_lpf1_update(&g_lp, 1000));
}

static void lpf1_step_response_positive(void)
{
    /* k=0.5(Q15=16384), 0 阶跃到 1000: 手算序列, 终态残差 1 LSB(<32768/k) */
    static const uint32_t expct[] = {
        500, 750, 875, 937, 968, 984, 992, 996, 998, 999, 999
    };
    uint32_t i;

    ET_CHECK(et_lpf1_init(&g_lp, 16384u));
    ET_CHECK_U32_EQ(0, (uint32_t)et_lpf1_update(&g_lp, 0));     /* 首样本直通建初值 */
    for (i = 0u; i < (sizeof(expct) / sizeof(expct[0])); i++) {
        ET_CHECK_U32_EQ(expct[i], (uint32_t)et_lpf1_update(&g_lp, 1000));
    }
}

static void lpf1_negative_step_converges_exact(void)
{
    /* 负阶跃因地板除法每步多收 1 LSB, 恰好精确收敛到 -1000 */
    static const uint32_t expct[] = {
        (uint32_t)-500, (uint32_t)-750, (uint32_t)-875, (uint32_t)-938,
        (uint32_t)-969, (uint32_t)-985, (uint32_t)-993, (uint32_t)-997,
        (uint32_t)-999, (uint32_t)-1000, (uint32_t)-1000
    };
    uint32_t i;

    ET_CHECK(et_lpf1_init(&g_lp, 16384u));
    ET_CHECK_U32_EQ(0, (uint32_t)et_lpf1_update(&g_lp, 0));
    for (i = 0u; i < (sizeof(expct) / sizeof(expct[0])); i++) {
        ET_CHECK_U32_EQ(expct[i], (uint32_t)et_lpf1_update(&g_lp, -1000));
    }
}

static void lpf1_k_boundaries(void)
{
    /* k=0: 输出冻结在首样本 */
    ET_CHECK(et_lpf1_init(&g_lp, 0u));
    (void)et_lpf1_update(&g_lp, 5);
    ET_CHECK_U32_EQ(5, (uint32_t)et_lpf1_update(&g_lp, 100));
    ET_CHECK_U32_EQ(5, (uint32_t)et_lpf1_update(&g_lp, 100));

    /* k=32767(0.99997): 一步到位但存在 1 LSB 稳态死区 */
    ET_CHECK(et_lpf1_init(&g_lp, 32767u));
    (void)et_lpf1_update(&g_lp, 0);
    ET_CHECK_U32_EQ(9, (uint32_t)et_lpf1_update(&g_lp, 10));
    ET_CHECK_U32_EQ(9, (uint32_t)et_lpf1_update(&g_lp, 10));    /* 残差 1, 不再变化 */
}

static void lpf1_set_k_midstream(void)
{
    ET_CHECK(et_lpf1_init(&g_lp, 16384u));
    (void)et_lpf1_update(&g_lp, 0);
    ET_CHECK_U32_EQ(500, (uint32_t)et_lpf1_update(&g_lp, 1000));
    et_lpf1_set_k(&g_lp, 32767u);                       /* 运行中调系数 */
    /* d = floor(32767*500/32768) = 499 -> 999, 残留 1 LSB 属 k=32767 死区 */
    ET_CHECK_U32_EQ(999, (uint32_t)et_lpf1_update(&g_lp, 1000));
    ET_CHECK_U32_EQ(999, (uint32_t)et_lpf1_update(&g_lp, 1000));
}

static void lpf1_reset_reprime(void)
{
    ET_CHECK(et_lpf1_init(&g_lp, 16384u));
    (void)et_lpf1_update(&g_lp, 0);
    (void)et_lpf1_update(&g_lp, 1000);
    et_lpf1_reset(&g_lp);
    ET_CHECK_U32_EQ(0, (uint32_t)et_lpf1_output(&g_lp));
    ET_CHECK_U32_EQ((uint32_t)-50, (uint32_t)et_lpf1_update(&g_lp, -50)); /* 重新直通 */
}

/* ===================== 斜率限制 ===================== */

static et_slew_t g_sl;

static void slew_init_validation(void)
{
    ET_CHECK(!et_slew_init(NULL, 100u));
    ET_CHECK(!et_slew_init(&g_sl, 0u));                 /* 0 步长 = 永久冻结, 拒绝 */
    ET_CHECK(et_slew_init(&g_sl, 1u));
}

static void slew_within_limit_passthrough(void)
{
    ET_CHECK(et_slew_init(&g_sl, 100u));
    ET_CHECK_U32_EQ(1000, (uint32_t)et_slew_update(&g_sl, 1000));   /* 首样本直通 */
    ET_CHECK_U32_EQ(1050, (uint32_t)et_slew_update(&g_sl, 1050));   /* 限幅内无损 */
    ET_CHECK_U32_EQ(1000, (uint32_t)et_slew_update(&g_sl, 1000));
}

static void slew_step_clamped(void)
{
    uint32_t i;

    ET_CHECK(et_slew_init(&g_sl, 100u));
    ET_CHECK_U32_EQ(0, (uint32_t)et_slew_update(&g_sl, 0));
    for (i = 1u; i <= 10u; i++) {                       /* 0 阶跃到 1000: 每步最多 100 */
        ET_CHECK_U32_EQ(i * 100u, (uint32_t)et_slew_update(&g_sl, 1000));
    }
    ET_CHECK_U32_EQ(1000, (uint32_t)et_slew_update(&g_sl, 1000));   /* 到达后跟平 */
}

static void slew_negative_clamped(void)
{
    ET_CHECK(et_slew_init(&g_sl, 100u));
    ET_CHECK_U32_EQ(0, (uint32_t)et_slew_update(&g_sl, 0));
    ET_CHECK_U32_EQ((uint32_t)-100, (uint32_t)et_slew_update(&g_sl, -350));
    ET_CHECK_U32_EQ((uint32_t)-200, (uint32_t)et_slew_update(&g_sl, -350));
    ET_CHECK_U32_EQ((uint32_t)-300, (uint32_t)et_slew_update(&g_sl, -350));
    ET_CHECK_U32_EQ((uint32_t)-350, (uint32_t)et_slew_update(&g_sl, -350)); /* 末步走剩余量 */
}

static void slew_boundary_exact_step(void)
{
    ET_CHECK(et_slew_init(&g_sl, 100u));
    ET_CHECK_U32_EQ(0, (uint32_t)et_slew_update(&g_sl, 0));
    ET_CHECK_U32_EQ(100, (uint32_t)et_slew_update(&g_sl, 100));     /* 恰好等于限幅: 放行 */
}

static void slew_reset_reprime(void)
{
    ET_CHECK(et_slew_init(&g_sl, 100u));
    ET_CHECK_U32_EQ(0, (uint32_t)et_slew_update(&g_sl, 0));
    ET_CHECK_U32_EQ(100, (uint32_t)et_slew_update(&g_sl, 500));
    et_slew_reset(&g_sl);
    ET_CHECK_U32_EQ(0, (uint32_t)et_slew_output(&g_sl));
    ET_CHECK_U32_EQ((uint32_t)-7, (uint32_t)et_slew_update(&g_sl, -7)); /* 重新直通 */
}

const et_test_case_t *test_filter_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"filter.movavg_init_validation",   movavg_init_validation},
        {"filter.movavg_partial_rolling",   movavg_partial_window_rolling},
        {"filter.movavg_full_overwrite",    movavg_window_full_overwrite},
        {"filter.movavg_rounding",          movavg_rounding_half_away},
        {"filter.movavg_negative",          movavg_negative_samples},
        {"filter.movavg_reset",             movavg_reset_semantics},
        {"filter.lpf1_init_validation",     lpf1_init_validation},
        {"filter.lpf1_first_passthrough",   lpf1_first_sample_passthrough},
        {"filter.lpf1_step_positive",       lpf1_step_response_positive},
        {"filter.lpf1_step_negative",       lpf1_negative_step_converges_exact},
        {"filter.lpf1_k_boundaries",        lpf1_k_boundaries},
        {"filter.lpf1_set_k_midstream",     lpf1_set_k_midstream},
        {"filter.lpf1_reset",               lpf1_reset_reprime},
        {"filter.slew_init_validation",     slew_init_validation},
        {"filter.slew_passthrough",         slew_within_limit_passthrough},
        {"filter.slew_step_clamped",        slew_step_clamped},
        {"filter.slew_negative_clamped",    slew_negative_clamped},
        {"filter.slew_boundary",            slew_boundary_exact_step},
        {"filter.slew_reset",               slew_reset_reprime},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

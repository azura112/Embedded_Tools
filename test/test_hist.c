/**
 * @file    test_hist.c
 * @brief   et_hist 单元测试 (期望值均为手算分桶/插值结果)
 */
#include "et_test.h"
#include "et_hist.h"

static uint32_t g_bins[256];
static et_hist_t g_h;

/* ---------- init 校验 ---------- */

static void hist_init_validation(void)
{
    ET_CHECK(!et_hist_init(NULL, g_bins, 5u, 0, 9));
    ET_CHECK(!et_hist_init(&g_h, NULL, 5u, 0, 9));
    ET_CHECK(!et_hist_init(&g_h, g_bins, 0u, 0, 9));        /* 0 桶 */
    ET_CHECK(!et_hist_init(&g_h, g_bins, 256u, 0, 9));      /* 超上限 */
    ET_CHECK(!et_hist_init(&g_h, g_bins, 4u, 5, 5));        /* lo==hi */
    ET_CHECK(!et_hist_init(&g_h, g_bins, 4u, 9, 0));        /* lo>hi */
    ET_CHECK(et_hist_init(&g_h, g_bins, 255u, 0, 254));     /* 上边界合法 */
    ET_CHECK(et_hist_init(&g_h, g_bins, 1u, 0, 9));         /* 下边界合法 */
    ET_CHECK_U32_EQ(0u, et_hist_count(&g_h));
}

/* ---------- 等宽分桶 / 闭区间 ---------- */

static void hist_equal_width_mapping(void)
{
    /* [0,9] 5 桶: width=10, 桶 i = {2i, 2i+1} */
    ET_CHECK(et_hist_init(&g_h, g_bins, 5u, 0, 9));
    et_hist_push(&g_h, 0);                              /* bin0 */
    et_hist_push(&g_h, 1);                              /* bin0 */
    et_hist_push(&g_h, 2);                              /* (2*5)/10=1 */
    et_hist_push(&g_h, 3);                              /* 1 */
    et_hist_push(&g_h, 8);                              /* 4 */
    et_hist_push(&g_h, 9);                              /* hi 下含上含: bin4 */
    ET_CHECK_U32_EQ(6u, et_hist_count(&g_h));
    ET_CHECK_U32_EQ(2u, et_hist_bin(&g_h, 0u));
    ET_CHECK_U32_EQ(2u, et_hist_bin(&g_h, 1u));
    ET_CHECK_U32_EQ(0u, et_hist_bin(&g_h, 2u));
    ET_CHECK_U32_EQ(0u, et_hist_bin(&g_h, 3u));
    ET_CHECK_U32_EQ(2u, et_hist_bin(&g_h, 4u));
    ET_CHECK_U32_EQ(0u, et_hist_bin(&g_h, 5u));         /* 越界桶号报 0 */
}

static void hist_single_bin(void)
{
    uint32_t i;

    ET_CHECK(et_hist_init(&g_h, g_bins, 1u, 0, 9));
    for (i = 0u; i <= 9u; i++) {
        et_hist_push(&g_h, (int32_t)i);                 /* 全部入唯一桶 */
    }
    ET_CHECK_U32_EQ(10u, et_hist_bin(&g_h, 0u));
    ET_CHECK_U32_EQ(10u, et_hist_count(&g_h));
    ET_CHECK_U32_EQ(0u, et_hist_under(&g_h));
    ET_CHECK_U32_EQ(0u, et_hist_over(&g_h));
}

static void hist_under_over_counters(void)
{
    ET_CHECK(et_hist_init(&g_h, g_bins, 5u, 0, 9));
    et_hist_push(&g_h, 0);                              /* 区间内 */
    et_hist_push(&g_h, -5);                             /* under */
    et_hist_push(&g_h, INT32_MIN);                      /* 极端 under */
    et_hist_push(&g_h, 10);                             /* over */
    et_hist_push(&g_h, INT32_MAX);                      /* 极端 over */
    ET_CHECK_U32_EQ(5u, et_hist_count(&g_h));           /* 总计数含越界 */
    ET_CHECK_U32_EQ(2u, et_hist_under(&g_h));
    ET_CHECK_U32_EQ(2u, et_hist_over(&g_h));
    ET_CHECK_U32_EQ(1u, et_hist_bin(&g_h, 0u));
    /* 全部越界: 无分布 */
    ET_CHECK_U32_EQ(0, (uint32_t)et_hist_percentile(&g_h, 50u));
}

/* ---------- 百分位 ---------- */

static void hist_percentile_exact_per_value(void)
{
    /* [0,99] 100 桶: 每桶恰一个值 → 百分位精确 */
    ET_CHECK(et_hist_init(&g_h, g_bins, 100u, 0, 99));
    et_hist_push(&g_h, 0);
    et_hist_push(&g_h, 50);
    et_hist_push(&g_h, 99);
    ET_CHECK_U32_EQ(0,  (uint32_t)et_hist_percentile(&g_h, 0u));
    ET_CHECK_U32_EQ(50, (uint32_t)et_hist_percentile(&g_h, 50u));
    ET_CHECK_U32_EQ(99, (uint32_t)et_hist_percentile(&g_h, 99u));   /* 尾分位 */
    ET_CHECK_U32_EQ(99, (uint32_t)et_hist_percentile(&g_h, 100u));
}

static void hist_percentile_interpolation(void)
{
    /* [0,999] 10 桶(桶宽 100): bin0 = 0..99, bin1 = 100..199
     * 各喂 100 个 → n=200; 插值粗估精度 ±1(桶宽/桶计数) */
    uint32_t i;

    ET_CHECK(et_hist_init(&g_h, g_bins, 10u, 0, 999));
    for (i = 0u; i < 100u; i++) {
        et_hist_push(&g_h, (int32_t)i);                 /* 0..99 */
    }
    for (i = 100u; i < 200u; i++) {
        et_hist_push(&g_h, (int32_t)i);                 /* 100..199 */
    }
    /* p25: rank=50 → bin0 offset 50 → 0 + 99*101/200 = 49 (真值 50, ±1) */
    ET_CHECK((et_hist_percentile(&g_h, 25u) >= 48) &&
             (et_hist_percentile(&g_h, 25u) <= 50));
    /* p50: rank=100 → bin1 offset 0 → 100 (真值 99.5) */
    ET_CHECK((et_hist_percentile(&g_h, 50u) >= 99) &&
             (et_hist_percentile(&g_h, 50u) <= 101));
    /* p75: rank=150 → bin1 offset 50 → 149 (真值 149.5) */
    ET_CHECK((et_hist_percentile(&g_h, 75u) >= 148) &&
             (et_hist_percentile(&g_h, 75u) <= 150));
}

static void hist_percentile_empty_and_single(void)
{
    ET_CHECK(et_hist_init(&g_h, g_bins, 4u, 0, 7));
    ET_CHECK_U32_EQ(0, (uint32_t)et_hist_percentile(&g_h, 50u));    /* 空集 */
    /* 粗估语义: 单样本落在桶 [2,3] → 桶内中点 2 (精度受桶宽限制, 头注声明) */
    et_hist_push(&g_h, 3);
    ET_CHECK_U32_EQ(2, (uint32_t)et_hist_percentile(&g_h, 0u));
    ET_CHECK_U32_EQ(2, (uint32_t)et_hist_percentile(&g_h, 100u));
    /* 桶宽=1(每桶恰一值)时百分位精确 */
    ET_CHECK(et_hist_init(&g_h, g_bins, 8u, 0, 7));
    et_hist_push(&g_h, 3);
    ET_CHECK_U32_EQ(3, (uint32_t)et_hist_percentile(&g_h, 0u));
    ET_CHECK_U32_EQ(3, (uint32_t)et_hist_percentile(&g_h, 50u));
    ET_CHECK_U32_EQ(3, (uint32_t)et_hist_percentile(&g_h, 100u));
}

static void hist_percentile_tail_latency_view(void)
{
    /* 长尾场景: 98 个快样本 + 2 个慢样本 → p99 落在慢桶 */
    ET_CHECK(et_hist_init(&g_h, g_bins, 4u, 0, 99));    /* 桶宽 25 */
    {
        uint32_t i;

        for (i = 0u; i < 98u; i++) {
            et_hist_push(&g_h, 10);                     /* bin0 */
        }
        for (i = 0u; i < 2u; i++) {
            et_hist_push(&g_h, 90);                     /* bin3 */
        }
    }
    /* p50: n=100, rank=50 → bin0 [0,24] 内插值 12 (真值 10, 粗估 ±桶宽/2) */
    ET_CHECK_U32_EQ(12, (uint32_t)et_hist_percentile(&g_h, 50u));
    /* p99: rank=99 → bin3 (cum 98), offset=1/cnt=2 → 75 + 3*24/4 = 93 (真值 90) */
    ET_CHECK_U32_EQ(93, (uint32_t)et_hist_percentile(&g_h, 99u));   /* 长尾可见 */
}

/* ---------- 语义 ---------- */

static void hist_clear_semantics(void)
{
    ET_CHECK(et_hist_init(&g_h, g_bins, 4u, 0, 7));
    et_hist_push(&g_h, 3);
    et_hist_push(&g_h, -1);
    et_hist_push(&g_h, 8);
    ET_CHECK_U32_EQ(3u, et_hist_count(&g_h));
    et_hist_clear(&g_h);
    ET_CHECK_U32_EQ(0u, et_hist_count(&g_h));
    ET_CHECK_U32_EQ(0u, et_hist_under(&g_h));
    ET_CHECK_U32_EQ(0u, et_hist_over(&g_h));
    ET_CHECK_U32_EQ(0u, et_hist_bin(&g_h, 0u));
    et_hist_push(&g_h, 5);                              /* 清后可继续用 */
    ET_CHECK_U32_EQ(1u, et_hist_count(&g_h));
}

static void hist_multi_instance(void)
{
    static et_hist_t ha;
    static et_hist_t hb;
    static uint32_t  ba[4];
    static uint32_t  bb[2];

    ET_CHECK(et_hist_init(&ha, ba, 4u, 0, 7));
    ET_CHECK(et_hist_init(&hb, bb, 2u, 0, 99));
    et_hist_push(&ha, 3);
    et_hist_push(&hb, 80);
    et_hist_push(&hb, 200);                             /* hb over */
    ET_CHECK_U32_EQ(1u, et_hist_count(&ha));
    ET_CHECK_U32_EQ(2u, et_hist_count(&hb));
    ET_CHECK_U32_EQ(1u, et_hist_over(&hb));
    ET_CHECK_U32_EQ(0u, et_hist_over(&ha));
}

static void hist_extreme_range_no_overflow(void)
{
    /* 全 int32 值域 2 桶: 乘法经 int64 不溢出 */
    ET_CHECK(et_hist_init(&g_h, g_bins, 2u, INT32_MIN, INT32_MAX));
    et_hist_push(&g_h, INT32_MIN);                      /* bin0 */
    et_hist_push(&g_h, 0);                              /* (2^31*2)/2^32 = 1 */
    et_hist_push(&g_h, INT32_MAX);                      /* bin1 */
    ET_CHECK_U32_EQ(1u, et_hist_bin(&g_h, 0u));
    ET_CHECK_U32_EQ(2u, et_hist_bin(&g_h, 1u));
    ET_CHECK_U32_EQ(0u, et_hist_under(&g_h));
    ET_CHECK_U32_EQ(0u, et_hist_over(&g_h));
}

const et_test_case_t *test_hist_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"hist.init_validation",       hist_init_validation},
        {"hist.equal_width_mapping",   hist_equal_width_mapping},
        {"hist.single_bin",            hist_single_bin},
        {"hist.under_over_counters",   hist_under_over_counters},
        {"hist.percentile_exact",      hist_percentile_exact_per_value},
        {"hist.percentile_interp",     hist_percentile_interpolation},
        {"hist.percentile_empty_single", hist_percentile_empty_and_single},
        {"hist.percentile_tail_view",  hist_percentile_tail_latency_view},
        {"hist.clear_semantics",       hist_clear_semantics},
        {"hist.multi_instance",        hist_multi_instance},
        {"hist.extreme_range",         hist_extreme_range_no_overflow},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

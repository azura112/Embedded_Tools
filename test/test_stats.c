/**
 * @file    test_stats.c
 * @brief   et_stats 单元测试 (与双精度参照对拍; 定点手算值)
 */
#include "et_test.h"
#include "et_stats.h"

static et_stats_t g_s;

/* 参照(宿主机 double 实现): 与库内"总体方差"口径一致 */
static void ref_of(const int32_t *v, uint32_t n, double *mean, double *var)
{
    uint32_t i;
    double   sum = 0.0;

    for (i = 0u; i < n; i++) {
        sum += (double)v[i];
    }
    *mean = (n > 0u) ? (sum / (double)n) : 0.0;
    sum = 0.0;
    for (i = 0u; i < n; i++) {
        double d = (double)v[i] - *mean;

        sum += d * d;
    }
    if (var != NULL) {
        *var = (n > 0u) ? (sum / (double)n) : 0.0;
    }
}

static int32_t ref_var_q10(const int32_t *v, uint32_t n)
{
    double mean;
    double var;

    ref_of(v, n, &mean, &var);
    return (int32_t)(var * 1024.0 + 0.5);
}

static int32_t ref_mean_round(const int32_t *v, uint32_t n)
{
    double mean;

    ref_of(v, n, &mean, NULL);
    return (mean >= 0.0) ? (int32_t)(mean + 0.5) : -(int32_t)(-mean + 0.5);
}

static void feed(const int32_t *v, uint32_t n)
{
    uint32_t i;

    for (i = 0u; i < n; i++) {
        et_stats_push(&g_s, v[i]);
    }
}

/* ---------- 生命周期 ---------- */

static void stats_init_and_empty(void)
{
    ET_CHECK(!et_stats_init(NULL));
    ET_CHECK(et_stats_init(&g_s));
    ET_CHECK_U32_EQ(0u, et_stats_count(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_min(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_max(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_mean(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_var_q10(&g_s));
}

static void stats_single_sample(void)
{
    ET_CHECK(et_stats_init(&g_s));
    et_stats_push(&g_s, 42);
    ET_CHECK_U32_EQ(1u, et_stats_count(&g_s));
    ET_CHECK_U32_EQ(42, (uint32_t)et_stats_mean(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_var_q10(&g_s));
    ET_CHECK_U32_EQ(42, (uint32_t)et_stats_min(&g_s));
    ET_CHECK_U32_EQ(42, (uint32_t)et_stats_max(&g_s));
}

static void stats_constant_input(void)
{
    uint32_t i;

    ET_CHECK(et_stats_init(&g_s));
    for (i = 0u; i < 10u; i++) {
        et_stats_push(&g_s, 7);
    }
    ET_CHECK_U32_EQ(7, (uint32_t)et_stats_mean(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_var_q10(&g_s));    /* 方差 0 */
    ET_CHECK_U32_EQ(7, (uint32_t)et_stats_min(&g_s));
    ET_CHECK_U32_EQ(7, (uint32_t)et_stats_max(&g_s));
}

/* ---------- 手算定点值 ---------- */

static void stats_two_samples_exact(void)
{
    static const int32_t v[] = { 0, 1000 };

    ET_CHECK(et_stats_init(&g_s));
    feed(v, 2u);
    ET_CHECK_U32_EQ(500, (uint32_t)et_stats_mean(&g_s));
    ET_CHECK_U32_EQ(256000000u, (uint32_t)et_stats_var_q10(&g_s));  /* 250000*1024 */
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_min(&g_s));
    ET_CHECK_U32_EQ(1000, (uint32_t)et_stats_max(&g_s));
}

static void stats_negative_values(void)
{
    static const int32_t v[] = { -10, -20 };

    ET_CHECK(et_stats_init(&g_s));
    feed(v, 2u);
    ET_CHECK_U32_EQ((uint32_t)-15, (uint32_t)et_stats_mean(&g_s));
    ET_CHECK_U32_EQ(25600u, (uint32_t)et_stats_var_q10(&g_s));      /* 25*1024 */
    ET_CHECK_U32_EQ((uint32_t)-20, (uint32_t)et_stats_min(&g_s));
    ET_CHECK_U32_EQ((uint32_t)-10, (uint32_t)et_stats_max(&g_s));
}

static void stats_order_independent(void)
{
    static const int32_t a[] = { 1, 2, 3, 4 };
    static const int32_t b[] = { 4, 3, 2, 1 };
    int32_t mean_a;
    int32_t var_a;
    int32_t mean_b;
    int32_t var_b;

    ET_CHECK(et_stats_init(&g_s));
    feed(a, 4u);
    mean_a = et_stats_mean(&g_s);
    var_a  = et_stats_var_q10(&g_s);

    ET_CHECK(et_stats_init(&g_s));
    feed(b, 4u);
    mean_b = et_stats_mean(&g_s);
    var_b  = et_stats_var_q10(&g_s);

    ET_CHECK_U32_EQ((uint32_t)mean_a, (uint32_t)mean_b);
    ET_CHECK_U32_EQ((uint32_t)var_a, (uint32_t)var_b);
    ET_CHECK_U32_EQ(3, (uint32_t)mean_a);                           /* 2.5 -> 3 (远离零) */
    ET_CHECK_U32_EQ(1280u, (uint32_t)var_a);                        /* 1.25*1024 */
}

/* ---------- 双精度参照对拍 ---------- */

static void stats_vs_double_reference(void)
{
    static const int32_t set1[] = { 3, 1, 4, 1, 5, 9, 2, 6 };
    static const int32_t set2[] = { 0, 1000, 2000 };
    static const int32_t set3[] = { -7, -3, -11, 5, 0, -2 };
    const int32_t *sets[3];
    const uint32_t lens[3] = { 8u, 3u, 6u };
    uint32_t si;

    sets[0] = set1; sets[1] = set2; sets[2] = set3;
    for (si = 0u; si < 3u; si++) {
        int32_t got_m;
        int32_t got_v;
        int32_t exp_m;
        int32_t exp_v;
        int32_t dm;
        int32_t dv;

        ET_CHECK(et_stats_init(&g_s));
        feed(sets[si], lens[si]);
        got_m = et_stats_mean(&g_s);
        got_v = et_stats_var_q10(&g_s);
        exp_m = ref_mean_round(sets[si], lens[si]);
        exp_v = ref_var_q10(sets[si], lens[si]);
        dm = got_m - exp_m;
        dv = got_v - exp_v;
        ET_CHECK((dm >= -1) && (dm <= 1));          /* Q10 均值误差 ≤ 1 */
        ET_CHECK((dv >= -2) && (dv <= 2));          /* Q10 方差误差 ≤ 2 */
    }
}

/* ---------- 语义 ---------- */

static void stats_reset_semantics(void)
{
    static const int32_t v[] = { 5, 6, 7, 8 };
    static const int32_t w[] = { 1, 1 };

    ET_CHECK(et_stats_init(&g_s));
    feed(v, 4u);
    ET_CHECK_U32_EQ(4u, et_stats_count(&g_s));

    et_stats_reset(&g_s);
    ET_CHECK_U32_EQ(0u, et_stats_count(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_mean(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_var_q10(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_min(&g_s));

    feed(w, 2u);                                    /* 复位后从头统计 */
    ET_CHECK_U32_EQ(2u, et_stats_count(&g_s));
    ET_CHECK_U32_EQ(1, (uint32_t)et_stats_mean(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_var_q10(&g_s));
}

static void stats_multi_instance(void)
{
    et_stats_t a;
    et_stats_t b;

    ET_CHECK(et_stats_init(&a));
    ET_CHECK(et_stats_init(&b));
    et_stats_push(&a, 10);
    et_stats_push(&b, -4);
    et_stats_push(&a, 20);
    ET_CHECK_U32_EQ(2u, et_stats_count(&a));
    ET_CHECK_U32_EQ(1u, et_stats_count(&b));
    ET_CHECK_U32_EQ(15, (uint32_t)et_stats_mean(&a));
    ET_CHECK_U32_EQ((uint32_t)-4, (uint32_t)et_stats_mean(&b));
    ET_CHECK_U32_EQ(25600u, (uint32_t)et_stats_var_q10(&a));        /* 25*1024 */
}

/* ---------- 极值 / 溢出防护 ---------- */

static void stats_extreme_saturation(void)
{
    ET_CHECK(et_stats_init(&g_s));
    et_stats_push(&g_s, INT32_MAX);
    et_stats_push(&g_s, INT32_MIN);
    ET_CHECK_U32_EQ(2u, et_stats_count(&g_s));
    ET_CHECK_U32_EQ((uint32_t)INT32_MIN, (uint32_t)et_stats_min(&g_s));
    ET_CHECK_U32_EQ((uint32_t)INT32_MAX, (uint32_t)et_stats_max(&g_s));
    ET_CHECK_U32_EQ((uint32_t)-1, (uint32_t)et_stats_mean(&g_s));   /* -0.5 -> -1 */
    ET_CHECK_U32_EQ((uint32_t)INT32_MAX, (uint32_t)et_stats_var_q10(&g_s)); /* 饱和 */
}

static void stats_large_step_saturation(void)
{
    ET_CHECK(et_stats_init(&g_s));
    et_stats_push(&g_s, INT32_MIN);
    et_stats_push(&g_s, INT32_MAX);
    /* 顺序无关: 饱和行为一致 */
    ET_CHECK_U32_EQ((uint32_t)INT32_MAX, (uint32_t)et_stats_var_q10(&g_s));

    ET_CHECK(et_stats_init(&g_s));
    et_stats_push(&g_s, INT32_MIN);
    (void)et_stats_min(&g_s);
    ET_CHECK_U32_EQ((uint32_t)INT32_MIN, (uint32_t)et_stats_mean(&g_s));
    ET_CHECK_U32_EQ(0, (uint32_t)et_stats_var_q10(&g_s));
}

const et_test_case_t *test_stats_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"stats.init_and_empty",        stats_init_and_empty},
        {"stats.single_sample",         stats_single_sample},
        {"stats.constant_input",        stats_constant_input},
        {"stats.two_samples_exact",     stats_two_samples_exact},
        {"stats.negative_values",       stats_negative_values},
        {"stats.order_independent",     stats_order_independent},
        {"stats.vs_double_reference",   stats_vs_double_reference},
        {"stats.reset_semantics",       stats_reset_semantics},
        {"stats.multi_instance",        stats_multi_instance},
        {"stats.extreme_saturation",    stats_extreme_saturation},
        {"stats.large_step_saturation", stats_large_step_saturation},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

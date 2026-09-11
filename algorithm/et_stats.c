/**
 * @file    et_stats.c
 * @brief   流式运行统计实现 (Welford 整数增量, Q10 均值, 饱和中间量)
 *
 * 单样本更新 (n = 更新后样本数, d 均为 Q10):
 *   d1 = v*1024 - mean_q10
 *   mean_q10 += d1 / n
 *   d2 = v*1024 - mean_q10
 *   m2_q10 += round(d1*d2 / 1024)      (d1,d2 同号, 乘积非负)
 * 方差(总体) = m2_q10 / n, 输出为 Q10 (×1024), 饱和到 INT32_MAX。
 */
#include "et_stats.h"

#if ET_MODULE_STATS

#define STATS_Q            1024LL
#define STATS_MEAN_SHIFT   10

/* 饱和乘法/加法: 不产生有符号溢出 UB */
static int64_t stats_mul_sat(int64_t a, int64_t b)
{
    if ((a == 0) || (b == 0)) {
        return 0;
    }
    if (a > 0) {
        if (b > 0) {
            if (a > (INT64_MAX / b)) { return INT64_MAX; }
        } else {
            if (b < (INT64_MIN / a)) { return INT64_MIN; }
        }
    } else {
        if (b > 0) {
            if (a < (INT64_MIN / b)) { return INT64_MIN; }
        } else {
            if (a < (INT64_MAX / b)) { return INT64_MAX; }
        }
    }
    return a * b;
}

static int64_t stats_add_sat(int64_t a, int64_t b)
{
    if ((b > 0) && (a > (INT64_MAX - b))) {
        return INT64_MAX;
    }
    if ((b < 0) && (a < (INT64_MIN - b))) {
        return INT64_MIN;
    }
    return a + b;
}

/* 非负被除数四舍五入除, 溢出钳到 INT64_MAX */
static int64_t stats_div_round_nonneg(int64_t num, int64_t den)
{
    int64_t half = den / 2;

    if (num < 0) {
        num = 0;                            /* 理论上不可达(乘积非负) */
    }
    if (num > (INT64_MAX - half)) {
        return INT64_MAX;
    }
    return (num + half) / den;
}

/* 有符号四舍五入除(远离零), 分母 > 0; 输入量级远小于 int64 溢出门限 */
static int32_t stats_div_round_s32(int64_t num, int64_t den)
{
    int64_t half = den / 2;

    if (num >= 0) {
        return (int32_t)((num + half) / den);
    }
    return -(int32_t)(((-num) + half) / den);
}

bool et_stats_init(et_stats_t *s)
{
    ET_ASSERT(s != NULL);
    if (s == NULL) {
        return false;
    }
    s->count    = 0u;
    s->mean_q10 = 0;
    s->m2_q10   = 0;
    s->vmin     = 0;
    s->vmax     = 0;
    return true;
}

void et_stats_push(et_stats_t *s, int32_t v)
{
    int64_t n;
    int64_t d1;
    int64_t d2;
    int64_t prod;

    ET_ASSERT(s != NULL);
    if (s == NULL) {
        return;
    }
    if (s->count == UINT32_MAX) {
        return;                             /* 计数饱和: 保持统计值不变 */
    }
    n = (int64_t)s->count + 1LL;
    s->count = (uint32_t)n;

    d1 = ((int64_t)v << STATS_MEAN_SHIFT) - s->mean_q10;
    s->mean_q10 += d1 / n;                  /* 截断除法, 误差 ≤ 1/1024 */
    d2 = ((int64_t)v << STATS_MEAN_SHIFT) - s->mean_q10;

    prod = stats_mul_sat(d1, d2);           /* Q20, 同号非负 */
    s->m2_q10 = stats_add_sat(s->m2_q10,
                              stats_div_round_nonneg(prod, STATS_Q));

    if (s->count == 1u) {
        s->vmin = v;
        s->vmax = v;
    } else {
        if (v < s->vmin) { s->vmin = v; }
        if (v > s->vmax) { s->vmax = v; }
    }
}

void et_stats_reset(et_stats_t *s)
{
    ET_ASSERT(s != NULL);
    if (s == NULL) {
        return;
    }
    s->count    = 0u;
    s->mean_q10 = 0;
    s->m2_q10   = 0;
    s->vmin     = 0;
    s->vmax     = 0;
}

uint32_t et_stats_count(const et_stats_t *s)
{
    ET_ASSERT(s != NULL);
    return (s == NULL) ? 0u : s->count;
}

int32_t et_stats_min(const et_stats_t *s)
{
    ET_ASSERT(s != NULL);
    return ((s == NULL) || (s->count == 0u)) ? 0 : s->vmin;
}

int32_t et_stats_max(const et_stats_t *s)
{
    ET_ASSERT(s != NULL);
    return ((s == NULL) || (s->count == 0u)) ? 0 : s->vmax;
}

int32_t et_stats_mean(const et_stats_t *s)
{
    ET_ASSERT(s != NULL);
    if ((s == NULL) || (s->count == 0u)) {
        return 0;
    }
    return stats_div_round_s32(s->mean_q10, STATS_Q);
}

int32_t et_stats_var_q10(const et_stats_t *s)
{
    int64_t v;

    ET_ASSERT(s != NULL);
    if ((s == NULL) || (s->count == 0u)) {
        return 0;
    }
    v = stats_div_round_nonneg(s->m2_q10, (int64_t)s->count);
    if (v > (int64_t)INT32_MAX) {
        return INT32_MAX;                   /* Q10 量程饱和(σ ≳ 1448) */
    }
    return (int32_t)v;
}

#endif /* ET_MODULE_STATS */

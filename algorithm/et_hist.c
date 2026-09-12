/**
 * @file    et_hist.c
 * @brief   定容直方图实现 (等宽分桶 + 桶内线性插值百分位)
 *
 * 分桶: v ∈ [lo,hi] → 桶 (v-lo)*bin_count/width, width = hi-lo+1 (闭区间)。
 * 百分位: rank = n*pct/100 (0 基, n = 区间内样本数, pct=100 钳到 n-1);
 * 定位桶后按 (2*offset+1)/(2*cnt) 在桶值域内插值 —— 桶内均匀假设下的
 * 中点映射, 截断取整(粗估语义, 见头注)。
 */
#include "et_hist.h"

#if ET_MODULE_HIST

bool et_hist_init(et_hist_t *h, uint32_t *bins, uint32_t bin_count,
                  int32_t lo, int32_t hi)
{
    uint32_t i;

    ET_ASSERT(h != NULL);
    ET_ASSERT(bins != NULL);
    ET_ASSERT(bin_count >= 1u);
    ET_ASSERT(bin_count <= (uint32_t)ET_HIST_BIN_MAX);
    ET_ASSERT(lo < hi);
    if ((h == NULL) || (bins == NULL)) {
        return false;
    }
    if ((bin_count == 0u) || (bin_count > (uint32_t)ET_HIST_BIN_MAX) ||
        (lo >= hi)) {
        return false;
    }
    for (i = 0u; i < bin_count; i++) {
        bins[i] = 0u;
    }
    h->bins      = bins;
    h->bin_count = bin_count;
    h->lo        = lo;
    h->hi        = hi;
    h->count     = 0u;
    h->under     = 0u;
    h->over      = 0u;
    return true;
}

void et_hist_push(et_hist_t *h, int32_t v)
{
    int64_t off;
    int64_t width;

    ET_ASSERT(h != NULL);
    if (h == NULL) {
        return;
    }
    h->count++;
    if (v < h->lo) {
        h->under++;
        return;
    }
    if (v > h->hi) {
        h->over++;
        return;
    }
    /* 等宽映射: (v-lo)*n/width, int64 承载乘积不溢出 */
    off   = (int64_t)v - (int64_t)h->lo;
    width = (int64_t)h->hi - (int64_t)h->lo + 1LL;
    {
        uint32_t idx = (uint32_t)((off * (int64_t)h->bin_count) / width);

        h->bins[idx]++;
    }
}

void et_hist_clear(et_hist_t *h)
{
    uint32_t i;

    ET_ASSERT(h != NULL);
    if (h == NULL) {
        return;
    }
    for (i = 0u; i < h->bin_count; i++) {
        h->bins[i] = 0u;
    }
    h->count = 0u;
    h->under = 0u;
    h->over  = 0u;
}

uint32_t et_hist_count(const et_hist_t *h)
{
    ET_ASSERT(h != NULL);
    return (h == NULL) ? 0u : h->count;
}

uint32_t et_hist_bin(const et_hist_t *h, uint32_t i)
{
    ET_ASSERT(h != NULL);
    if ((h == NULL) || (i >= h->bin_count)) {
        return 0u;
    }
    return h->bins[i];
}

uint32_t et_hist_under(const et_hist_t *h)
{
    ET_ASSERT(h != NULL);
    return (h == NULL) ? 0u : h->under;
}

uint32_t et_hist_over(const et_hist_t *h)
{
    ET_ASSERT(h != NULL);
    return (h == NULL) ? 0u : h->over;
}

int32_t et_hist_percentile(const et_hist_t *h, uint8_t pct)
{
    uint32_t n;                 /* 区间内样本数 */
    uint32_t rank;
    uint32_t cum = 0u;
    uint32_t i;

    ET_ASSERT(h != NULL);
    if ((h == NULL) || (h->count == 0u)) {
        return 0;
    }
    n = h->count - h->under - h->over;
    if (n == 0u) {
        return 0;               /* 全部越界: 无分布可言 */
    }
    rank = ((uint64_t)n * (uint64_t)pct) / 100u;
    if (rank >= n) {
        rank = n - 1u;          /* pct=100 → 最大样本 */
    }
    /* 定位 rank 所在桶 */
    for (i = 0u; i < h->bin_count; i++) {
        uint32_t c = h->bins[i];

        if (rank < (cum + c)) {
            /* 桶 i 值域 [start, end] (闭区间, 等宽整数分桶) */
            int64_t span    = ((int64_t)h->hi - (int64_t)h->lo + 1LL);
            int64_t start   = (int64_t)h->lo + (((int64_t)i * span) /
                                                (int64_t)h->bin_count);
            int64_t end     = (int64_t)h->lo +
                              ((((int64_t)i + 1LL) * span) /
                               (int64_t)h->bin_count) - 1LL;
            int64_t offset  = (int64_t)(rank - cum);
            int64_t cnt     = (int64_t)c;
            int64_t val;

            if (end < start) {  /* 空值域桶(值域宽 < 桶数时的不可达桶) */
                end = start;
            }
            /* 桶内插值: offset ∈ [0,cnt) 映射到 [start,end] 的中点序列 */
            val = start + (((2LL * offset + 1LL) * (end - start)) /
                           (2LL * cnt));
            return (int32_t)val;
        }
        cum += c;
    }
    return (int32_t)h->hi;      /* 理论不可达(计数守恒); 兜底 */
}

#endif /* ET_MODULE_HIST */

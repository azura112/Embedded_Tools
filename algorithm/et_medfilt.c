/**
 * @file    et_medfilt.c
 * @brief   中值滤波器实现 (环形窗口 + 每步插入排序取中位)
 */
#include "et_medfilt.h"

#if ET_MODULE_MEDFILT

bool et_medfilt_init(et_medfilt_t *f, int32_t *buf, uint32_t win_len)
{
    ET_ASSERT(f != NULL);
    ET_ASSERT(buf != NULL);
    ET_ASSERT((win_len >= 3u) && ((win_len % 2u) == 1u));
    if ((f == NULL) || (buf == NULL)) {
        return false;
    }
    if ((win_len < 3u) || ((win_len % 2u) != 1u) ||
        (win_len > (uint32_t)ET_MEDFILT_WIN_MAX)) {
        return false;                       /* 偶数窗/超长窗: "取上/下中位"歧义, 拒绝 */
    }
    f->buf     = buf;
    f->win_len = win_len;
    f->idx     = 0u;
    f->cnt     = 0u;
    return true;
}

/* 取窗口当前 n 个样本升序排序后的第 (n-1)/2 位(下中位; 奇数 n 即中位数) */
static int32_t medfilt_median(const et_medfilt_t *f)
{
    int32_t tmp[ET_MEDFILT_WIN_MAX];
    uint32_t n = f->cnt;
    uint32_t i;
    uint32_t j;

    for (i = 0u; i < n; i++) {
        tmp[i] = f->buf[i];
    }
    for (i = 1u; i < n; i++) {              /* 插入排序(≤15 元素) */
        int32_t key = tmp[i];

        for (j = i; (j > 0u) && (tmp[j - 1u] > key); j--) {
            tmp[j] = tmp[j - 1u];
        }
        tmp[j] = key;
    }
    return tmp[(n - 1u) / 2u];
}

int32_t et_medfilt_push(et_medfilt_t *f, int32_t v)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return 0;
    }
    if (f->cnt < f->win_len) {
        f->cnt++;                           /* 窗未满: 追加 */
    }
    f->buf[f->idx] = v;                     /* 窗已满: 覆盖最旧 */
    f->idx++;
    if (f->idx >= f->win_len) {
        f->idx = 0u;
    }
    return medfilt_median(f);
}

void et_medfilt_reset(et_medfilt_t *f)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return;
    }
    f->idx = 0u;
    f->cnt = 0u;
}

uint32_t et_medfilt_count(const et_medfilt_t *f)
{
    ET_ASSERT(f != NULL);
    return (f == NULL) ? 0u : f->cnt;
}

uint32_t et_medfilt_window(const et_medfilt_t *f)
{
    ET_ASSERT(f != NULL);
    return (f == NULL) ? 0u : f->win_len;
}

#endif /* ET_MODULE_MEDFILT */

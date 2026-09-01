/**
 * @file    et_filter.c
 * @brief   定点数字滤波器组实现
 */
#include "et_filter.h"

#if ET_MODULE_FILTER

/* ===================== 滑动窗口均值 ===================== */

bool et_movavg_init(et_movavg_t *f, int32_t *storage, uint32_t window)
{
    ET_ASSERT(f != NULL);
    ET_ASSERT(storage != NULL);
    ET_ASSERT(window >= 1u);
    if ((f == NULL) || (storage == NULL) || (window == 0u)) {
        return false;
    }
    f->buf    = storage;
    f->window = window;
    f->idx    = 0u;
    f->cnt    = 0u;
    f->sum    = 0;
    return true;
}

/* 窗口均值, 四舍五入到远离零方向(与符号无关的对称舍入) */
static int32_t movavg_rounded(const et_movavg_t *f)
{
    int64_t half = (int64_t)(f->cnt / 2u);

    if (f->sum >= 0) {
        return (int32_t)((f->sum + half) / (int64_t)f->cnt);
    }
    return -(int32_t)(((-f->sum) + half) / (int64_t)f->cnt);
}

int32_t et_movavg_update(et_movavg_t *f, int32_t x)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return 0;
    }
    if (f->cnt < f->window) {
        f->cnt++;                       /* 窗口未满: 追加 */
    } else {
        f->sum -= f->buf[f->idx];       /* 窗口已满: 挤掉最旧样本 */
    }
    f->buf[f->idx] = x;
    f->sum += x;
    f->idx++;
    if (f->idx >= f->window) {
        f->idx = 0u;
    }
    return movavg_rounded(f);
}

void et_movavg_reset(et_movavg_t *f)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return;
    }
    f->idx = 0u;
    f->cnt = 0u;
    f->sum = 0;
}

uint32_t et_movavg_count(const et_movavg_t *f)
{
    ET_ASSERT(f != NULL);
    return (f == NULL) ? 0u : f->cnt;
}

uint32_t et_movavg_window(const et_movavg_t *f)
{
    ET_ASSERT(f != NULL);
    return (f == NULL) ? 0u : f->window;
}

/* ===================== 一阶 IIR 低通 ===================== */

/* floor(p / 32768): 可移植的地板除法(不依赖负数右移的实现定义行为) */
static int32_t lpf1_frac(int64_t p)
{
    if (p >= 0) {
        return (int32_t)(p >> 15);
    }
    return -(int32_t)(((-p) + ((1LL << 15) - 1LL)) >> 15);
}

bool et_lpf1_init(et_lpf1_t *f, uint16_t k_q15)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return false;
    }
    f->y      = 0;
    f->k_q15  = k_q15;                  /* k_q15 上限 32767 由类型天然保证 */
    f->primed = false;
    return true;
}

int32_t et_lpf1_update(et_lpf1_t *f, int32_t x)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return 0;
    }
    if (!f->primed) {
        f->y      = x;                  /* 首样本直通 */
        f->primed = true;
        return f->y;
    }
    f->y += lpf1_frac((int64_t)f->k_q15 * (int64_t)(x - f->y));
    return f->y;
}

void et_lpf1_set_k(et_lpf1_t *f, uint16_t k_q15)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return;
    }
    f->k_q15 = k_q15;
}

void et_lpf1_reset(et_lpf1_t *f)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return;
    }
    f->y      = 0;
    f->primed = false;
}

int32_t et_lpf1_output(const et_lpf1_t *f)
{
    ET_ASSERT(f != NULL);
    return (f == NULL) ? 0 : f->y;
}

/* ===================== 斜率限制 ===================== */

bool et_slew_init(et_slew_t *f, uint32_t max_step)
{
    ET_ASSERT(f != NULL);
    ET_ASSERT(max_step >= 1u);
    if ((f == NULL) || (max_step == 0u)) {
        return false;
    }
    f->y       = 0;
    f->max_step = max_step;
    f->primed  = false;
    return true;
}

int32_t et_slew_update(et_slew_t *f, int32_t x)
{
    int32_t e;

    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return 0;
    }
    if (!f->primed) {
        f->y      = x;                  /* 首样本直通 */
        f->primed = true;
        return f->y;
    }
    e = x - f->y;
    if (e > (int32_t)f->max_step) {
        e = (int32_t)f->max_step;
    } else if (e < -(int32_t)f->max_step) {
        e = -(int32_t)f->max_step;
    }
    f->y += e;
    return f->y;
}

void et_slew_reset(et_slew_t *f)
{
    ET_ASSERT(f != NULL);
    if (f == NULL) {
        return;
    }
    f->y      = 0;
    f->primed = false;
}

int32_t et_slew_output(const et_slew_t *f)
{
    ET_ASSERT(f != NULL);
    return (f == NULL) ? 0 : f->y;
}

#endif /* ET_MODULE_FILTER */

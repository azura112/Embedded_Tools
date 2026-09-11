/**
 * @file    et_pid.c
 * @brief   定点 PID 控制器实现 (位置式, Q15 增益, 饱和 int64 中间量)
 *
 * 单步公式 (Q15 = 32768):
 *   e     = sp - pv
 *   P     = round(kp * e / Q15)
 *   I     = round(i_acc / (Q15*1000)),  i_acc += ki * e * dt_ms, 钳位 [i_min,i_max]窗口
 *   D     = round(kd * d * 1000 / (Q15 * dt_ms))
 *           d_on_measure=1: d = -(pv - pv_prev);  =0: d = +(e - e_prev)
 *   out   = clamp(P + I + D, out_min, out_max)
 *
 * 所有乘算走饱和乘法(pid_mul_sat), 加法走饱和加法(pid_add_sat): 不依赖有符号
 * 溢出(UB), 极端增益/误差/dt 下输出仍被输出限幅钳定。
 */
#include "et_pid.h"

#if ET_MODULE_PID

#define PID_Q15         32768LL
#define PID_I_DEN       (PID_Q15 * 1000LL)      /* 积分累加 -> 输出的分母 */
#define PID_D_NUM_SCALE 1000LL                  /* 变化率按"每秒"标定 */

/* 饱和乘法: 结果超出 int64 时钳到端点(不产生 UB) */
static int64_t pid_mul_sat(int64_t a, int64_t b)
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

/* 饱和加法 */
static int64_t pid_add_sat(int64_t a, int64_t b)
{
    if ((b > 0) && (a > (INT64_MAX - b))) {
        return INT64_MAX;
    }
    if ((b < 0) && (a < (INT64_MIN - b))) {
        return INT64_MIN;
    }
    return a + b;
}

/* 四舍五入除(远离零方向), 分母 > 0; 溢出钳到端点 */
static int64_t pid_div_round(int64_t num, int64_t den)
{
    int64_t half = den / 2;

    if (num >= 0) {
        if (num > (INT64_MAX - half)) { return INT64_MAX; }
        return (num + half) / den;
    }
    if (num == INT64_MIN) {
        return INT64_MIN;
    }
    {
        int64_t mag = -num;

        if (mag > (INT64_MAX - half)) { return INT64_MIN; }
        return -((mag + half) / den);
    }
}

/* 积分累加窗口(输出标度 -> Q15·ms 标度); i_min/i_max 为 int32, 乘积不溢出 */
static int64_t pid_i_window(const et_pid_t *p, int up)
{
    int64_t lim = (up != 0) ? (int64_t)p->i_max : (int64_t)p->i_min;

    return lim * PID_I_DEN;
}

bool et_pid_init(et_pid_t *p, const et_pid_cfg_t *cfg)
{
    ET_ASSERT(p != NULL);
    ET_ASSERT(cfg != NULL);
    if ((p == NULL) || (cfg == NULL)) {
        return false;
    }
    if ((cfg->out_min > cfg->out_max) || (cfg->i_min > cfg->i_max)) {
        return false;
    }
    p->kp           = cfg->kp;
    p->ki           = cfg->ki;
    p->kd           = cfg->kd;
    p->out_min      = cfg->out_min;
    p->out_max      = cfg->out_max;
    p->i_min        = cfg->i_min;
    p->i_max        = cfg->i_max;
    p->d_on_measure = cfg->d_on_measure;
    p->i_acc        = 0;
    p->pv_prev      = 0;
    p->e_prev       = 0;
    p->out          = 0;
    p->primed       = false;
    return true;
}

void et_pid_set_gains(et_pid_t *p, int32_t kp, int32_t ki, int32_t kd)
{
    ET_ASSERT(p != NULL);
    if (p == NULL) {
        return;
    }
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
}

void et_pid_reset(et_pid_t *p)
{
    ET_ASSERT(p != NULL);
    if (p == NULL) {
        return;
    }
    p->i_acc   = 0;
    p->pv_prev = 0;
    p->e_prev  = 0;
    p->out     = 0;
    p->primed  = false;
}

int32_t et_pid_step(et_pid_t *p, int32_t sp, int32_t pv, uint32_t dt_ms)
{
    int64_t e;
    int64_t p_term;
    int64_t i_term;
    int64_t d_term;
    int64_t total;
    int64_t lo;
    int64_t hi;

    ET_ASSERT(p != NULL);
    if (p == NULL) {
        return 0;
    }
    if (dt_ms == 0u) {
        return p->out;                      /* 无效采样: 不改状态 */
    }

    e = (int64_t)sp - (int64_t)pv;

    /* ---- P ---- */
    p_term = pid_div_round(pid_mul_sat((int64_t)p->kp, e), PID_Q15);

    /* ---- I (增量 + 积分限幅钳位) ---- */
    if (p->ki != 0) {
        int64_t inc = pid_mul_sat(pid_mul_sat((int64_t)p->ki, e), (int64_t)dt_ms);

        p->i_acc = pid_add_sat(p->i_acc, inc);
    }
    lo = pid_i_window(p, 0);
    hi = pid_i_window(p, 1);
    if (p->i_acc > hi) {
        p->i_acc = hi;
    } else if (p->i_acc < lo) {
        p->i_acc = lo;
    }
    i_term = pid_div_round(p->i_acc, PID_I_DEN);

    /* ---- D (首步无历史: 0) ---- */
    d_term = 0;
    if (p->primed) {
        int64_t d = (p->d_on_measure != 0u)
                        ? ((int64_t)p->pv_prev - (int64_t)pv)   /* -(pv - pv_prev) */
                        : (e - (int64_t)p->e_prev);
        int64_t den = (int64_t)dt_ms * PID_Q15;

        d_term = pid_div_round(pid_mul_sat(pid_mul_sat((int64_t)p->kd, d),
                                           PID_D_NUM_SCALE), den);
    }

    /* ---- 求和 + 输出钳位 + 更新历史 ---- */
    total = pid_add_sat(pid_add_sat(p_term, i_term), d_term);
    if (total > (int64_t)p->out_max) {
        total = p->out_max;
    } else if (total < (int64_t)p->out_min) {
        total = p->out_min;
    }
    p->out     = (int32_t)total;
    p->pv_prev = pv;
    p->e_prev  = e;
    p->primed  = true;
    return p->out;
}

int32_t et_pid_output(const et_pid_t *p)
{
    ET_ASSERT(p != NULL);
    return (p == NULL) ? 0 : p->out;
}

#endif /* ET_MODULE_PID */

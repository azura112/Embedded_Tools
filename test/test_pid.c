/**
 * @file    test_pid.c
 * @brief   et_pid 单元测试 (期望值均为手算定点数值; Q15=32768)
 */
#include "et_test.h"
#include "et_pid.h"

/* 便捷配置: kp/ki/kd 用 Q15, 限幅给宽(除专门用例) */
static et_pid_cfg_t mk(int32_t kp, int32_t ki, int32_t kd, uint8_t dom)
{
    et_pid_cfg_t c;

    c.kp    = kp;
    c.ki    = ki;
    c.kd    = kd;
    c.out_min = -1000000;
    c.out_max = 1000000;
    c.i_min   = -1000000;
    c.i_max   = 1000000;
    c.d_on_measure = dom;
    return c;
}

static et_pid_t g_p;

/* ---------- 生命周期 / 参数校验 ---------- */

static void pid_init_validation(void)
{
    et_pid_cfg_t c = mk(32768, 0, 0, 1u);

    ET_CHECK(!et_pid_init(NULL, &c));
    ET_CHECK(!et_pid_init(&g_p, NULL));

    c.out_min = 10; c.out_max = -10;                /* 输出上下限倒置 */
    ET_CHECK(!et_pid_init(&g_p, &c));
    c = mk(32768, 0, 0, 1u);
    c.i_min = 10; c.i_max = -10;                    /* 积分上下限倒置 */
    ET_CHECK(!et_pid_init(&g_p, &c));

    c = mk(32768, 0, 0, 1u);
    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_output(&g_p));
}

/* ---------- P 项 ---------- */

static void pid_p_proportional(void)
{
    et_pid_cfg_t c = mk(32768, 0, 0, 1u);           /* kp=1.0 */

    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(100, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    ET_CHECK_U32_EQ(50,  (uint32_t)et_pid_step(&g_p, 100, 50, 100u));
    ET_CHECK_U32_EQ(0,   (uint32_t)et_pid_step(&g_p, 100, 100, 100u));   /* e=0 */
    ET_CHECK_U32_EQ(100, (uint32_t)et_pid_step(&g_p, 200, 100, 100u));
    ET_CHECK_U32_EQ((uint32_t)-100, (uint32_t)et_pid_step(&g_p, 0, 100, 100u));
}

static void pid_p_gain_half_and_rounding(void)
{
    et_pid_cfg_t c = mk(16384, 0, 0, 1u);           /* kp=0.5 */

    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(50, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    ET_CHECK_U32_EQ(2,  (uint32_t)et_pid_step(&g_p, 3, 0, 100u));   /* 1.5 -> 2 */
    ET_CHECK_U32_EQ((uint32_t)-2, (uint32_t)et_pid_step(&g_p, -3, 0, 100u));
}

static void pid_output_clamp(void)
{
    et_pid_cfg_t c = mk(32768, 0, 0, 1u);

    c.out_min = -50; c.out_max = 50;
    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(50,  (uint32_t)et_pid_step(&g_p, 100, 0, 100u));   /* P=100 -> 50 */
    ET_CHECK_U32_EQ((uint32_t)-50, (uint32_t)et_pid_step(&g_p, -100, 0, 100u));
}

/* ---------- I 项 ---------- */

static void pid_i_accumulates(void)
{
    et_pid_cfg_t c = mk(0, 32768, 0, 1u);           /* ki=1.0 (1/s) */
    uint32_t i;

    ET_CHECK(et_pid_init(&g_p, &c));
    /* e=100 恒持, dt=100ms: 每步 I = 100 * 0.1s = 10 */
    for (i = 1u; i <= 4u; i++) {
        ET_CHECK_U32_EQ(10u * i, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    }
}

static void pid_i_clamp_antiwindup(void)
{
    et_pid_cfg_t c = mk(0, 32768, 0, 1u);

    c.i_min = -40; c.i_max = 40;                    /* 积分权限 ±40 */
    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(10, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    ET_CHECK_U32_EQ(20, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    ET_CHECK_U32_EQ(30, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    ET_CHECK_U32_EQ(40, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    ET_CHECK_U32_EQ(40, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));   /* 钳位不失控 */
    ET_CHECK_U32_EQ(40, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
}

static void pid_output_clamp_with_integral(void)
{
    et_pid_cfg_t c = mk(0, 32768, 0, 1u);

    c.out_min = -1000; c.out_max = 15;              /* 输出权限 15, 积分权限大 */
    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(10, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    ET_CHECK_U32_EQ(15, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));   /* 15 钳位 */
    ET_CHECK_U32_EQ(15, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));   /* 无回灌 */
}

/* ---------- D 项 ---------- */

static void pid_d_on_measure_no_setpoint_kick(void)
{
    et_pid_cfg_t c = mk(0, 0, 32768, 1u);           /* kd=1.0, d 作用于测量 */

    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 0, 0, 100u));      /* 建立基准 */
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 10, 0, 100u));     /* sp 跳变: pv 未动 -> D=0 */
}

static void pid_d_on_error_has_setpoint_kick(void)
{
    et_pid_cfg_t c = mk(0, 0, 32768, 0u);           /* 经典式: d 作用于误差 */

    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 0, 0, 100u));
    /* e 0->10, dt=100ms: D = 1.0 * 10 * 1000 / (100) = +100 */
    ET_CHECK_U32_EQ(100, (uint32_t)et_pid_step(&g_p, 10, 0, 100u));
}

static void pid_d_measure_response(void)
{
    et_pid_cfg_t c = mk(0, 0, 32768, 1u);

    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 0, 0, 100u));
    /* pv 0->10, dt=100ms: D = -1.0 * 10 * 1000 / 100 = -100 */
    ET_CHECK_U32_EQ((uint32_t)-100, (uint32_t)et_pid_step(&g_p, 0, 10, 100u));
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 0, 10, 100u));     /* pv 不变 -> 0 */
    ET_CHECK_U32_EQ((uint32_t)-100, (uint32_t)et_pid_step(&g_p, 0, 20, 100u));
}

static void pid_dt_variation(void)
{
    et_pid_cfg_t c = mk(0, 32768, 0, 1u);           /* 积分: 相同总时长应等量 */
    uint32_t i;

    ET_CHECK(et_pid_init(&g_p, &c));
    for (i = 0u; i < 10u; i++) {                    /* dt=100ms x10 = 1s */
        (void)et_pid_step(&g_p, 100, 0, 100u);
    }
    ET_CHECK_U32_EQ(100, (uint32_t)et_pid_output(&g_p));

    ET_CHECK(et_pid_init(&g_p, &c));
    for (i = 0u; i < 4u; i++) {                     /* dt=250ms x4 = 1s */
        (void)et_pid_step(&g_p, 100, 0, 250u);
    }
    ET_CHECK_U32_EQ(100, (uint32_t)et_pid_output(&g_p));

    /* 微分: 相同 pv 步, dt 加倍 -> D 减半 */
    c = mk(0, 0, 32768, 1u);
    ET_CHECK(et_pid_init(&g_p, &c));
    (void)et_pid_step(&g_p, 0, 0, 100u);
    ET_CHECK_U32_EQ((uint32_t)-100, (uint32_t)et_pid_step(&g_p, 0, 10, 100u));

    ET_CHECK(et_pid_init(&g_p, &c));
    (void)et_pid_step(&g_p, 0, 0, 100u);
    ET_CHECK_U32_EQ((uint32_t)-50, (uint32_t)et_pid_step(&g_p, 0, 10, 200u));
}

static void pid_dt_zero_no_state_change(void)
{
    et_pid_cfg_t c = mk(0, 0, 32768, 1u);

    ET_CHECK(et_pid_init(&g_p, &c));
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 0, 0, 100u));      /* pv_prev=0 */
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 0, 10, 0u));       /* dt=0: 不改状态 */
    /* 若上一步更新了 pv_prev, 此步 D 应为 0; 未更新则 D=-100 */
    ET_CHECK_U32_EQ((uint32_t)-100, (uint32_t)et_pid_step(&g_p, 0, 10, 100u));
}

/* ---------- 语义 ---------- */

static void pid_reset_semantics(void)
{
    et_pid_cfg_t c = mk(0, 32768, 0, 1u);
    uint32_t i;

    ET_CHECK(et_pid_init(&g_p, &c));
    for (i = 0u; i < 5u; i++) {
        (void)et_pid_step(&g_p, 100, 0, 100u);
    }
    ET_CHECK_U32_EQ(50, (uint32_t)et_pid_output(&g_p));
    et_pid_reset(&g_p);
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_output(&g_p));
    ET_CHECK_U32_EQ(10, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));   /* 从零重新计 */
}

static void pid_set_gains_keeps_state(void)
{
    et_pid_cfg_t c = mk(32768, 32768, 0, 1u);       /* kp=1.0, ki=1.0 */

    ET_CHECK(et_pid_init(&g_p, &c));
    /* 第一步: P=100, I=10 -> 110 */
    ET_CHECK_U32_EQ(110, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));
    et_pid_set_gains(&g_p, 0, 32768, 0);            /* kp 归零, 保留积分历史 */
    ET_CHECK_U32_EQ(20, (uint32_t)et_pid_step(&g_p, 100, 0, 100u));   /* P=0, I=20 */
}

static void pid_steady_state_zero(void)
{
    et_pid_cfg_t c = mk(32768, 32768, 32768, 1u);

    ET_CHECK(et_pid_init(&g_p, &c));
    /* 起始即 sp==pv: e=0, 积分不增, 首步 D=0 -> 输出 0 */
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 500, 500, 100u));
    ET_CHECK_U32_EQ(0, (uint32_t)et_pid_step(&g_p, 500, 500, 100u));
}

static void pid_multi_instance(void)
{
    et_pid_t a;
    et_pid_t b;
    et_pid_cfg_t ca = mk(32768, 0, 0, 1u);
    et_pid_cfg_t cb = mk(16384, 0, 0, 1u);

    ET_CHECK(et_pid_init(&a, &ca));
    ET_CHECK(et_pid_init(&b, &cb));
    ET_CHECK_U32_EQ(100, (uint32_t)et_pid_step(&a, 100, 0, 100u));
    ET_CHECK_U32_EQ(50,  (uint32_t)et_pid_step(&b, 100, 0, 100u));
    ET_CHECK_U32_EQ(0,   (uint32_t)et_pid_step(&a, 100, 100, 100u));  /* a 独立 */
    ET_CHECK_U32_EQ(100, (uint32_t)et_pid_step(&b, 200, 0, 100u));    /* b 独立 */
}

/* ---------- 极值 / 溢出防护 ---------- */

static void pid_overflow_saturation(void)
{
    et_pid_cfg_t c;
    int32_t      r1;
    int32_t      r2;

    c.kp = INT32_MAX; c.ki = INT32_MAX; c.kd = INT32_MAX;
    c.out_min = INT32_MIN; c.out_max = INT32_MAX;
    c.i_min = INT32_MIN; c.i_max = INT32_MAX;
    c.d_on_measure = 1u;

    ET_CHECK(et_pid_init(&g_p, &c));
    /* 满量程正差: P 巨大 -> 饱和到 out_max */
    ET_CHECK_U32_EQ((uint32_t)INT32_MAX,
                    (uint32_t)et_pid_step(&g_p, INT32_MAX, INT32_MIN, UINT32_MAX));
    r1 = et_pid_output(&g_p);
    ET_CHECK_U32_EQ((uint32_t)INT32_MAX, (uint32_t)r1);

    et_pid_reset(&g_p);
    /* 满量程负差 -> 饱和到 out_min */
    ET_CHECK_U32_EQ((uint32_t)INT32_MIN,
                    (uint32_t)et_pid_step(&g_p, INT32_MIN, INT32_MAX, UINT32_MAX));
    r2 = et_pid_output(&g_p);
    ET_CHECK_U32_EQ((uint32_t)INT32_MIN, (uint32_t)r2);
    ET_CHECK(r1 != r2);                             /* 确定性: 非回绕的同一中间值 */
}

static void pid_integral_saturation_with_extreme_dt(void)
{
    et_pid_cfg_t c = mk(0, INT32_MAX, INT32_MAX, 1u);

    c.i_min = -10; c.i_max = 10;                    /* 积分权限被钳死 */
    ET_CHECK(et_pid_init(&g_p, &c));
    (void)et_pid_step(&g_p, INT32_MAX, INT32_MIN, UINT32_MAX);
    ET_CHECK_U32_EQ(10, (uint32_t)et_pid_step(&g_p, INT32_MAX, INT32_MIN, UINT32_MAX));
    (void)et_pid_step(&g_p, INT32_MAX, INT32_MIN, UINT32_MAX);
    ET_CHECK_U32_EQ(10, (uint32_t)et_pid_output(&g_p));   /* 不失控 */
}

const et_test_case_t *test_pid_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"pid.init_validation",              pid_init_validation},
        {"pid.p_proportional",               pid_p_proportional},
        {"pid.p_gain_rounding",              pid_p_gain_half_and_rounding},
        {"pid.output_clamp",                 pid_output_clamp},
        {"pid.i_accumulates",                pid_i_accumulates},
        {"pid.i_clamp_antiwindup",           pid_i_clamp_antiwindup},
        {"pid.output_clamp_with_integral",   pid_output_clamp_with_integral},
        {"pid.d_on_measure_no_sp_kick",      pid_d_on_measure_no_setpoint_kick},
        {"pid.d_on_error_sp_kick",           pid_d_on_error_has_setpoint_kick},
        {"pid.d_measure_response",           pid_d_measure_response},
        {"pid.dt_variation",                 pid_dt_variation},
        {"pid.dt_zero_no_state_change",      pid_dt_zero_no_state_change},
        {"pid.reset_semantics",              pid_reset_semantics},
        {"pid.set_gains_keeps_state",        pid_set_gains_keeps_state},
        {"pid.steady_state_zero",            pid_steady_state_zero},
        {"pid.multi_instance",               pid_multi_instance},
        {"pid.overflow_saturation",          pid_overflow_saturation},
        {"pid.integral_saturation_extreme",  pid_integral_saturation_with_extreme_dt},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

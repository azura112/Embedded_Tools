/**
 * @file    ex_pid_loop.c
 * @brief   配方可执行载体 #1: 定点闭环整定 (API_GUIDE 11.10 的 host 版)
 *
 * 链路 (11.10 三段式): 模拟被控对象 → et_lpf1(去噪,k≈1.0 直通演示装配)
 *                      → et_pid(控制) → 模拟对象; et_stats 判稳 + et_hist
 *                      记录控制量分布。
 *
 * 模拟被控对象: y += k*(u-y) (一阶离散, k=Q15 0.4/步, 10ms 节拍) —— 与
 * v2.2 G474 板上 demo 同款, 无真实硬件; 全程定点整数运算, 输出确定。
 *
 * 自检(对确定性数值, 期望值 = host 同款定点公式预计算):
 *   - 闭环 3s: 无超调(peak==sp), 稳定时间 settle==1090ms;
 *   - et_stats 判稳: 达到设定值后方差阈值判稳(var_q10 < 1024);
 *   - et_hist 控制量分布: u ∈ [0,1000] 全落桶, 主体集中在 300~600。
 *
 * 运行: make ex (或单独编译, 见 Makefile ex 目标); 退出码 0=PASS。
 * API_GUIDE 11.10 ←→ 本文件互为载体: 改配方须同步改本例并让 CI 裁决。
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "et_pid.h"
#include "et_stats.h"
#include "et_hist.h"
#include "et_filter.h"

static int g_fail;

#define EX_CHECK(cond)                                                      \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("  FAIL: %s (line %d)\n", #cond, __LINE__);              \
            g_fail++;                                                       \
        }                                                                   \
    } while (0)

#define Q15_ONE         32768L      /* Q15 的 1.0 */
#define DT_MS           10u
#define RUN_STEPS       300u        /* 3s @10ms */
#define SP              500
#define PLANT_K         13107L      /* 0.4/步 */

int main(void)
{
    static et_pid_t   pid;
    static et_stats_t st;
    static et_hist_t  hist;
    static uint32_t   hb[10];
    static et_lpf1_t  lpf;
    et_pid_cfg_t      cfg;
    int32_t  y    = 0;          /* 被控对象状态 */
    int32_t  peak = 0;
    uint32_t settle_ms = 0u;
    uint32_t i;

    printf("== ex_pid_loop: API_GUIDE 11.10 定点闭环整定 (host 版) ==\n");

    /* ---- 装配 (配方第 1~2 步: 滤波在前, 时基固定 10ms) ---- */
    cfg.kp           = 1L * Q15_ONE;            /* kp=1.0 */
    cfg.ki           = 4L * Q15_ONE;            /* ki=4.0 (1/s) */
    cfg.kd           = 0L;
    cfg.out_min      = -1000;
    cfg.out_max      = 1000;
    cfg.i_min        = -500;
    cfg.i_max        = 500;                     /* 积分权限(钳位抗饱和) */
    cfg.d_on_measure = 1u;
    EX_CHECK(et_pid_init(&pid, &cfg));
    EX_CHECK(et_lpf1_init(&lpf, 32767u));        /* k≈1.0: 演示装配, 近直通 */
    EX_CHECK(et_stats_init(&st));
    EX_CHECK(et_hist_init(&hist, hb, 10u, 0, 1000));

    /* ---- 闭环 3s ---- */
    for (i = 0u; i < RUN_STEPS; i++) {
        int32_t pv = et_lpf1_update(&lpf, y);   /* 测量→滤波 */
        int32_t u  = et_pid_step(&pid, SP, pv, DT_MS);

        y += (int32_t)(((int64_t)(u - y) * PLANT_K) >> 15);   /* 模拟对象 */
        (void)et_stats_push(&st, pv);
        et_hist_push(&hist, u);                 /* 控制量分布 */
        if (y > peak) {
            peak = y;
        }
        if (((y - SP) > (SP / 20)) || ((SP - y) > (SP / 20))) {
            settle_ms = (i + 1u) * DT_MS;       /* 最后一次越出 ±5% 带 */
        }
    }

    printf("  peak=%d settle=%ums final_y=%d\n", peak, (unsigned)settle_ms, y);
    printf("  stats: n=%u mean=%d max=%d var_q10=%d\n",
           (unsigned)et_stats_count(&st), et_stats_mean(&st),
           et_stats_max(&st), et_stats_var_q10(&st));
    printf("  hist(u): n=%u under=%u over=%u b3=%u b4=%u p50=%d\n",
           (unsigned)et_hist_count(&hist), (unsigned)et_hist_under(&hist),
           (unsigned)et_hist_over(&hist), (unsigned)et_hist_bin(&hist, 3u),
           (unsigned)et_hist_bin(&hist, 4u),
           et_hist_percentile(&hist, 50u));

    /* ---- 自检 (期望值 = host 同款定点公式预计算, 确定性) ---- */
    EX_CHECK(peak == SP);                       /* 保守增益: 无超调 */
    EX_CHECK(settle_ms == 1090u);               /* 与 v2.2 板上走单同数量级 */
    EX_CHECK(y == SP);                          /* 稳态归位 */
    EX_CHECK(et_stats_count(&st) == RUN_STEPS);
    EX_CHECK(et_stats_max(&st) == SP - 1);      /* lpf1 k≈1 直通的 1 LSB 残差 */
    /* 控制量分布: 全部落桶(无越界), 主体集中 300~600 (桶 3/4) */
    EX_CHECK(et_hist_under(&hist) == 0u);
    EX_CHECK(et_hist_over(&hist) == 0u);
    EX_CHECK(et_hist_bin(&hist, 3u) == 34u);
    EX_CHECK(et_hist_bin(&hist, 4u) == 196u);
    EX_CHECK(et_hist_percentile(&hist, 50u) == 455);

    /* ---- 判稳 (配方第 5 步): 稳态开新窗口, 方差阈值 < 1.0 ---- */
    et_stats_reset(&st);
    {
        uint32_t j;

        for (j = 0u; j < 50u; j++) {
            int32_t pv = et_lpf1_update(&lpf, y);
            int32_t u  = et_pid_step(&pid, SP, pv, DT_MS);

            y += (int32_t)(((int64_t)(u - y) * PLANT_K) >> 15);
            (void)et_stats_push(&st, pv);
        }
    }
    EX_CHECK(et_stats_count(&st) == 50u);
    EX_CHECK(et_stats_var_q10(&st) < 1024);     /* 稳态: 方差 < 1.0 */
    printf("  settle-check: var_q10=%d (50-sample window after settle)\n",
           et_stats_var_q10(&st));

    printf("[ex_pid_loop] %s\n", (g_fail == 0) ? "PASS" : "FAIL");
    return (g_fail == 0) ? 0 : 1;
}

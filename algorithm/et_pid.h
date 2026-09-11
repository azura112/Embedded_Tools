/**
 * @file    et_pid.h
 * @brief   定点 PID 控制器 (位置式, Q15 增益, int64 中间累加)
 *
 * 定位:
 *  - 补齐 "测量 → 滤波 → 控制 → 输出" 环路的控制环节 —— 与 et_filter(Q15 定点)
 *    和 et_spwm(执行输出) 走同一条定点路线, 无任何浮点依赖;
 *  - 纯算法层: 不含 port.h, 不触碰硬件/时基; dt_ms 由调用方给(复用 stimer/sched
 *    或固定采样周期), 库内不取时基;
 *  - 位置式(输出绝对量)而非增量式: 配合 et_spwm 这类"按目标值设占空比"的执行器。
 *
 * 标度约定 (Q15, 与 et_filter 一致):
 *  - 增益 kp/ki/kd 均为 Q15: 32768 = 1.0;
 *  - kp 无量纲 (P = kp·e); ki 单位 1/s (I = ki·∫e dt); kd 单位 s (D 见下);
 *  - sp/pv 与输出、限幅(out_min/out_max/i_min/i_max)同标度(通常 = 被控量标度,
 *    如 ADC 码值或已归一化的 Q15 值); "增益 1.0" 即 1:1 传递。
 *  - 微分作用对象可选: d_on_measure=1 时 D 作用于测量值(推荐默认, 免除设定值
 *    跳变冲击); =0 时作用于误差(经典式, 设定值阶跃会带来微分尖峰)。
 *
 * 抗饱和与溢出语义 (文档化决议):
 *  - 抗饱和 = 积分限幅(钳位法): 积分累加被钳在 [i_min,i_max] 对应的窗口内,
 *    **不做反算回灌**; i_min/i_max 是"积分项的最大权限"(输出标度)。
 *  - 积分/微分在 int64 中累加与乘算, 乘算带饱和(不产生有符号溢出 UB);
 *    任意 int32 输入 + 任意 dt_ms 下, 返回值始终被 out_min/out_max 钳定。
 *  - dt_ms == 0 视为无效采样: 本步不改状态、返回上次输出(首步为 0)。
 *
 * 并发约定 (单上下文模块, 与 et_mempool 同级标注):
 *  - 全部 API 仅限 🏠MAIN 单上下文(典型为固定周期控制任务/主循环);
 *  - 跨上下文共享时由调用方用 PORT_CRITICAL_ENTER/EXIT 包裹完整 step;
 *  - 【非 ISR-safe】: 内部含 int64 乘除, ISR 中调用可能超出中断预算。
 */
#ifndef ET_PID_H
#define ET_PID_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_PID

#ifdef __cplusplus
extern "C" {
#endif

/* PID 配置(初始化时拷入实例; 增益可运行中用 et_pid_set_gains 调整) */
typedef struct {
    int32_t kp, ki, kd;         /* Q15 增益 (32768 = 1.0)              */
    int32_t out_min, out_max;   /* 输出限幅(与被控量同标度)           */
    int32_t i_min,  i_max;      /* 积分限幅(抗饱和, 输出标度)          */
    uint8_t d_on_measure;       /* 1 = 微分作用于测量值(推荐, 默认开) */
} et_pid_cfg_t;

typedef struct {
    int32_t  kp, ki, kd;        /* Q15 增益, 勿动(用 set_gains 改)     */
    int32_t  out_min, out_max;  /* 输出限幅, 勿动                      */
    int32_t  i_min,  i_max;     /* 积分限幅, 勿动                      */
    int64_t  i_acc;             /* 积分累加(Q15·ms), 勿动              */
    int32_t  pv_prev;           /* 上次测量值(微分历史), 勿动         */
    int64_t  e_prev;            /* 上次误差(可达 int32 差, 存 int64)  */
    int32_t  out;               /* 上次输出, 勿动                      */
    uint8_t  d_on_measure;      /* 勿动                                */
    bool     primed;            /* 微分历史已建立, 勿动                */
} et_pid_t;

/* 初始化: cfg 非法(NULL / out_min>out_max / i_min>i_max)返回 false 且不改现场 */
bool     et_pid_init(et_pid_t *p, const et_pid_cfg_t *cfg);

/* 运行中调整增益(Q15); 不清积分/微分历史(需清零用 et_pid_reset) */
void     et_pid_set_gains(et_pid_t *p, int32_t kp, int32_t ki, int32_t kd);

/* 清积分与微分历史, 输出归零; 下步重新建立微分基准(D=0, 无首步冲击) */
void     et_pid_reset(et_pid_t *p);

/* 单步控制: e=sp-pv, dt_ms 为距上次调用的毫秒数(>0)。
 * 返回钳位后的绝对输出; 首步与 dt_ms==0 时微分项为 0 / 不更新。 */
int32_t  et_pid_step(et_pid_t *p, int32_t sp, int32_t pv, uint32_t dt_ms);

/* 上次输出(未调用 step 时为 0) */
int32_t  et_pid_output(const et_pid_t *p);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_PID */
#endif /* ET_PID_H */

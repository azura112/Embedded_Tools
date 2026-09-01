/**
 * @file    et_filter.h
 * @brief   定点数字滤波器组 (滑动窗口均值 / 一阶 IIR 低通 / 斜率限制)
 *
 * 定位:
 *  - ADC 采样等周期信号的处理标配, 全部定点实现, 无任何浮点依赖;
 *  - 纯算法层: 本模块不包含 port.h, 不触碰任何硬件/时基;
 *  - 三个滤波器相互独立, 句柄互不兼容, 可任意组合级联。
 *
 * 并发约定 (单上下文模块):
 *  - 所有 API 仅限单一上下文调用(典型为主循环/采样任务);
 *  - 跨上下文共享时由调用方用 PORT_CRITICAL_ENTER/EXIT 包裹完整操作。
 */
#ifndef ET_FILTER_H
#define ET_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_FILTER

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== 滑动窗口均值 ===================== */
/*
 * 环形覆盖历史样本, O(1) 增量更新(减旧加新)。
 * 窗口未填满时输出"已有样本的均值"(开机免长等待)。
 * 和值用 int64 累加, 任意 int32 样本不溢出。
 */
typedef struct {
    int32_t *buf;                   /* 外部提供的样本存储区(容量 = window) */
    uint32_t window;                /* 窗口容量(样本个数) */
    uint32_t idx;                   /* 下一个写入位置, 勿动 */
    uint32_t cnt;                   /* 当前样本数(≤window), 勿动 */
    int64_t  sum;                   /* 窗口内样本和, 勿动 */
} et_movavg_t;

/* 绑定样本存储区(须能容纳 window 个 int32), window ≥ 1 */
bool     et_movavg_init(et_movavg_t *f, int32_t *storage, uint32_t window);

/* 送入新样本, 返回当前滤波输出(窗口均值的四舍五入整数值) */
int32_t  et_movavg_update(et_movavg_t *f, int32_t x);

/* 清空历史(下一样本重新从空窗口开始) */
void     et_movavg_reset(et_movavg_t *f);

uint32_t et_movavg_count(const et_movavg_t *f);     /* 当前窗口内样本数 */
uint32_t et_movavg_window(const et_movavg_t *f);

/* ===================== 一阶 IIR 低通 ===================== */
/*
 * 递推式 y += k*(x - y), k 为 Q15 定点系数(0~32767, 即 0~0.99997)。
 * 首个样本直通(y = x), 避免从 0 缓慢爬升。
 * 定点特性: 输出与理想值存在 |残差| < 32768/k 的稳态死区(k 越大死区越小)。
 */
typedef struct {
    int32_t  y;                     /* 滤波输出(即状态), 勿动 */
    uint16_t k_q15;                 /* Q15 系数, 勿动 */
    bool     primed;                /* 已收到首样本, 勿动 */
} et_lpf1_t;

/* k_q15 取值 0~32767; 0 = 输出保持首样本不变 */
bool     et_lpf1_init(et_lpf1_t *f, uint16_t k_q15);

/* 送入新样本, 返回滤波输出 */
int32_t  et_lpf1_update(et_lpf1_t *f, int32_t x);

/* 运行中调整系数(下次 update 生效) */
void     et_lpf1_set_k(et_lpf1_t *f, uint16_t k_q15);

/* 清状态: 下一样本重新直通 */
void     et_lpf1_reset(et_lpf1_t *f);

int32_t  et_lpf1_output(const et_lpf1_t *f);

/* ===================== 斜率限制(限幅滤波) ===================== */
/*
 * 输出每次最多向目标移动 max_step, 抑制脉冲型毛刺;
 * 变化量在限幅内时输出与输入一致(无损耗直通)。
 * 首个样本直通。
 */
typedef struct {
    int32_t  y;                     /* 当前输出, 勿动 */
    uint32_t max_step;              /* 每次更新允许的最大变化量, 勿动 */
    bool     primed;                /* 勿动 */
} et_slew_t;

/* max_step ≥ 1(每样本至少允许变化 1, 否则输出永远冻结) */
bool     et_slew_init(et_slew_t *f, uint32_t max_step);

/* 送入新样本, 返回限幅后的输出 */
int32_t  et_slew_update(et_slew_t *f, int32_t x);

/* 清状态: 下一样本重新直通 */
void     et_slew_reset(et_slew_t *f);

int32_t  et_slew_output(const et_slew_t *f);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_FILTER */
#endif /* ET_FILTER_H */

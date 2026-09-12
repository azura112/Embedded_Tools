/**
 * @file    et_hist.h
 * @brief   定容直方图 (等宽分桶, 越界计数, 百分位粗估, 零分配)
 *
 * 定位:
 *  - 可观测性收尾: 分布视图 —— 与 et_stats(点估计: min/max/均值/方差)、
 *    et_sched_task_stats(单任务 last/max) 互补; "任务耗时集中还是长尾"
 *    这类问题由 percentile(99) 回答;
 *  - 典型链路: et_sched_task_stats 采样 → et_hist_push → 诊断命令读分布
 *    (API_GUIDE 11.12); 亦可用于 ADC 值/延迟/控制量分布;
 *  - 纯算法层: 不包含 port.h, 不触碰任何硬件/时基。
 *
 * 区间与分桶语义 (文档化决议, v2.3 P1):
 *  - 统计区间为**闭区间 [lo, hi]**(下含上含): v==hi 恰好落入末桶 —— 嵌入式
 *    "判稳带宽 [sp-δ, sp+δ]" 类直觉是闭区间;
 *  - 等宽分桶: 值域宽 width = hi-lo+1, v 落入桶 (v-lo)*bin_count/width
 *    (整数除法, 0 基); bin_count ≤ 255(uint8 索引) 且 ≥ 1;
 *  - **越界不丢弃**: v<lo 计入 under, v>hi 计入 over, 均独立计数;
 *  - percentile 为**桶内线性插值粗估**(非精确分位数): 精度受桶宽/桶计数
 *    限制, 文档声明仅供分布判断(长尾检测), 不做精确承诺;
 *  - 全部 uint32 计数, 桶数组由调用方持有(零分配)。
 *
 * 并发约定 (单上下文模块, 与 et_mempool 同级标注):
 *  - 全部 API 仅限 🏠MAIN 单上下文;
 *  - 跨上下文共享时由调用方用 PORT_CRITICAL_ENTER/EXIT 包裹完整操作。
 */
#ifndef ET_HIST_H
#define ET_HIST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_HIST

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t *bins;         /* 调用方持有的桶数组(容量 bin_count), 勿动 */
    uint32_t  bin_count;    /* 桶数 1~ET_HIST_BIN_MAX, 勿动 */
    int32_t   lo;           /* 闭区间下界, 勿动 */
    int32_t   hi;           /* 闭区间上界(> lo), 勿动 */
    uint32_t  count;        /* 总计数(含越界), 勿动 */
    uint32_t  under;        /* v < lo 计数, 勿动 */
    uint32_t  over;         /* v > hi 计数, 勿动 */
} et_hist_t;

/* 绑定桶数组并置空: bin_count 1~ET_HIST_BIN_MAX, 且 lo < hi, 否则拒绝 */
bool     et_hist_init(et_hist_t *h, uint32_t *bins, uint32_t bin_count,
                      int32_t lo, int32_t hi);

/* 喂入一个样本, O(1): 区间内入桶, 越界进 under/over */
void     et_hist_push(et_hist_t *h, int32_t v);

/* 清空(桶/计数全归零, 保留区间与桶配置) */
void     et_hist_clear(et_hist_t *h);

uint32_t et_hist_count(const et_hist_t *h);         /* 总计数(含越界) */
uint32_t et_hist_bin(const et_hist_t *h, uint32_t i); /* 第 i 桶计数(i 越界报 0) */
uint32_t et_hist_under(const et_hist_t *h);         /* v < lo 计数 */
uint32_t et_hist_over(const et_hist_t *h);          /* v > hi 计数 */

/* 百分位粗估(桶内线性插值): pct 0~100; 区间内无样本返回 0。
 * 语义: rank = 区间内样本数*pct/100 (0 基), pct=100 取最大;
 * 精度受桶宽/桶计数限制 —— 用于分布/长尾判断, 非精确分位数。 */
int32_t  et_hist_percentile(const et_hist_t *h, uint8_t pct);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_HIST */
#endif /* ET_HIST_H */

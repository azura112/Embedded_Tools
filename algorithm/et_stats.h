/**
 * @file    et_stats.h
 * @brief   流式运行统计 (定长窗口无关, 零分配, Welford 整数增量)
 *
 * 定位:
 *  - 调试与整定配套: 对被控量/传感器流做 min/max/均值/方差的在线统计;
 *  - 与 et_pid 联动配方: stats 采集被控量 → 方差阈值判稳 → 记录超调(见 API_GUIDE);
 *  - 纯算法层: 不含 port.h, 无浮点依赖, 单实例状态全部在调用方句柄内。
 *
 * 算法与标度:
 *  - Welford 增量式(不累加 Σx²), 避免大数相减失真与 Σx² 溢出; 均值为
 *    Q10 内部精度(误差 ≤ 1/1024), 方差以 Q10 定点输出(×1024);
 *  - 方差为**总体方差**(除以 n), 与"双精度参照: Σ(x-mean)²/n"对拍一致;
 *  - 方差量程: int32 的 Q10 只能表达 σ ≲ 1448 的分布; 超出时输出饱和到
 *    INT32_MAX(确定性行为, 非回绕)。
 *
 * 溢出语义 (文档化决议):
 *  - 均值/方差中间量在 int64 中累加, 乘算带饱和; INT32_MIN/INT32_MAX 输入
 *    不产生有符号溢出 UB, 结果为饱和值;
 *  - count 达 UINT32_MAX 后忽略后续样本(保持统计值不变, 不回绕)。
 *
 * 并发约定 (单上下文模块, 与 et_mempool 同级标注):
 *  - 全部 API 仅限 🏠MAIN 单上下文;
 *  - 跨上下文共享时由调用方用 PORT_CRITICAL_ENTER/EXIT 包裹完整操作。
 */
#ifndef ET_STATS_H
#define ET_STATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_STATS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t count;         /* 已喂入样本数, 勿动                  */
    int64_t  mean_q10;      /* 均值 × 1024 (Q10), 勿动             */
    int64_t  m2_q10;        /* Σ(x−mean)² × 1024 (饱和), 勿动      */
    int32_t  vmin;          /* 最小值(count==0 时未定义), 勿动     */
    int32_t  vmax;          /* 最大值(count==0 时未定义), 勿动     */
} et_stats_t;

/* 清零(空集: 均值/极值/方差均报 0) */
bool     et_stats_init(et_stats_t *s);

/* 喂入一个样本(流式, O(1)); count 饱和于 UINT32_MAX 后忽略 */
void     et_stats_push(et_stats_t *s, int32_t v);

/* 清空统计(等价于 init, 保留句柄) */
void     et_stats_reset(et_stats_t *s);

uint32_t et_stats_count(const et_stats_t *s);

/* count==0 时以下四项均返回 0 */
int32_t  et_stats_min(const et_stats_t *s);
int32_t  et_stats_max(const et_stats_t *s);
int32_t  et_stats_mean(const et_stats_t *s);      /* 四舍五入到整数 */
int32_t  et_stats_var_q10(const et_stats_t *s);   /* 总体方差 × 1024, 饱和 */

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_STATS */
#endif /* ET_STATS_H */

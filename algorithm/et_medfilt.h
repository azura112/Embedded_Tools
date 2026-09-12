/**
 * @file    et_medfilt.h
 * @brief   中值滤波器 (定长奇数窗口, 小窗插入排序, 零分配)
 *
 * 定位:
 *  - ADC 尖峰(脉冲干扰)去除标配, 与 et_movavg / et_lpf1 / et_slew 并列;
 *    尖峰场景标准两级链 = medfilt(去脉冲) → lpf1(平滑), 见 API_GUIDE;
 *  - 中值对脉冲不敏感: 单点大尖峰只污染窗口一个槽位, 不影响中位数 ——
 *    这是滑动均值(movavg)做不到的(尖峰被摊进均值);
 *  - 纯算法层: 不包含 port.h, 不触碰任何硬件/时基。
 *
 * 窗满前语义 (文档化决议, v2.2 P3):
 *  - 输出 = 当前窗口内已入样本升序排序后的第 ⌊n/2⌋ 位(0 基);
 *  - 窗满后 n = win_len(奇数) → 恰为中位数; 窗满前 n < win_len 时该式给出
 *    "下中位"(偶数个样本取较小者), 确定性好且与奇数窗全满语义同式;
 *  - 阶跃响应延迟 = (win_len-1)/2 个样本(窗口一半宽度)。
 *
 * 实现代价: 每次 push 对 ≤win_len 个样本做插入排序, O(win_len²);
 * win_len ≤ 15 时最坏 ~225 次比较, 10kHz 采样下 < 0.1% CPU。
 *
 * 并发约定 (单上下文模块):
 *  - 所有 API 仅限单一上下文调用(典型为主循环/采样任务);
 *  - 跨上下文共享时由调用方用 PORT_CRITICAL_ENTER/EXIT 包裹完整操作。
 */
#ifndef ET_MEDFILT_H
#define ET_MEDFILT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_MEDFILT

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t *buf;                   /* 外部提供的窗口存储区(容量 = win_len) */
    uint32_t win_len;               /* 窗口容量(奇数, 3~ET_MEDFILT_WIN_MAX) */
    uint32_t idx;                   /* 下一个写入位置, 勿动 */
    uint32_t cnt;                   /* 当前样本数(≤win_len), 勿动 */
} et_medfilt_t;

/* 绑定窗口存储区(须能容纳 win_len 个 int32);
 * win_len 必须为奇数且 3 ≤ win_len ≤ ET_MEDFILT_WIN_MAX, 否则拒绝 */
bool     et_medfilt_init(et_medfilt_t *f, int32_t *buf, uint32_t win_len);

/* 送入新样本(环形覆盖最旧), 返回当前窗口中值(窗满前为下中位, 见文件头) */
int32_t  et_medfilt_push(et_medfilt_t *f, int32_t v);

/* 清空历史(下一样本重新从空窗口开始) */
void     et_medfilt_reset(et_medfilt_t *f);

uint32_t et_medfilt_count(const et_medfilt_t *f);     /* 当前窗口内样本数 */
uint32_t et_medfilt_window(const et_medfilt_t *f);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_MEDFILT */
#endif /* ET_MEDFILT_H */

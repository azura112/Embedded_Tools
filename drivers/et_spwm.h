/**
 * @file    et_spwm.h
 * @brief   多通道软件 PWM (毫秒时基相位法)
 *
 * 设计要点:
 *  - 相位基于绝对时基计算: 电平 = ((now - t0) % period) < on_ms,
 *    轮询抖动不累积相位误差, 时基自然回绕由无符号减法消化;
 *  - duty 0/255 为精确边界: 恒低/恒高, 任何相位下都不会产生毛刺;
 *  - 输出带缓存, 电平不变时不重复调用 write;
 *  - 与 et_led 配套: et_led 的 write 回调内把亮度值转发给
 *    et_spwm_set() 即可在无硬件 PWM 的 GPIO 上实现呼吸灯。
 *
 * 分辨率假设 (务必阅读):
 *  - 时基粒度 1ms 时, 可用周期 >= 2ms (最高约 500Hz);
 *  - 实际占空比步进为 1/period, 高分辨率/高频需求请使用硬件 PWM;
 *  - 电平翻转发生在 poll 调用时刻, 主循环长阻塞会造成可见的边沿抖动,
 *    重载场景建议降低周期要求或改用硬件 PWM。
 *
 * 并发约定: 全部 API 仅限主循环上下文 (内部零临界区)。
 */
#ifndef ET_SPWM_H
#define ET_SPWM_H

#include <stdint.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_SPWM

#ifdef __cplusplus
extern "C" {
#endif

/* 电平输出回调: on 非 0 为高, 0 为低 */
typedef void (*et_spwm_write_fn)(void *user, uint8_t on);

/* 初始化通道(默认占空比 0, 恒低): period_ms >= 2; 重复 init 等效重新配置 */
bool et_spwm_init(uint8_t ch, et_spwm_write_fn fn, void *user,
                  uint16_t period_ms);

/* 解绑通道: 停止驱动(不触碰最后一次输出的电平) */
void et_spwm_deinit(uint8_t ch);

/* 设置占空比 duty 0~255 (生效于下一次 poll), 未初始化通道返回 false */
bool et_spwm_set(uint8_t ch, uint8_t duty);

uint8_t  et_spwm_get_duty(uint8_t ch);      /* 未初始化通道返回 0 */
uint16_t et_spwm_get_period(uint8_t ch);

/* 刷新全部通道电平: now 为当前毫秒, 建议挂进主循环既有 poll 链 */
void et_spwm_poll(uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_SPWM */
#endif /* ET_SPWM_H */

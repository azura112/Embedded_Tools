/**
 * @file    et_led.h
 * @brief   LED 模式管理器 (常亮/闪烁N次/无限闪烁/软件呼吸)
 *
 * 设计要点:
 *  - 亮度输出经 write 回调抽象(0~255): 普通 GPIO 可阈值化, 有 PWM 的平台直通;
 *  - 相位基于绝对时基计算, 采样抖动不影响闪烁精度;
 *  - poll 内部带输出缓存, 亮度不变时不重复调用 write。
 *
 * 使用方法:
 *   1. init 后按需调用 set_* 切换模式;
 *   2. 主循环/软定时器中周期调用 et_led_poll(l, now):
 *      - BLINK 类模式低频轮询即可(如每 tick);
 *      - BREATH 建议以 period/64 以上的频率轮询保证平滑。
 */
#ifndef ET_LED_H
#define ET_LED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_LED

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ET_LED_MODE_OFF = 0,
    ET_LED_MODE_ON,
    ET_LED_MODE_BLINK,                  /* 无限闪烁           */
    ET_LED_MODE_BLINK_N,                /* 闪 n 个周期后熄灭  */
    ET_LED_MODE_BREATH,                 /* 三角波呼吸         */
} et_led_mode_t;

typedef void (*et_led_write_fn)(void *user, uint8_t brightness);

typedef struct et_led {
    et_led_write_fn  write;             /* 亮度输出回调       */
    void            *user;

    et_led_mode_t    mode;
    uint16_t         period_ms;
    uint16_t         on_ms;             /* BLINK: 一个周期内点亮时长 */
    uint16_t         cycles_left;       /* BLINK_N 剩余周期数 */
    uint32_t         t0;                /* 当前模式起始时刻   */
    bool             pending_start;     /* 首次 poll 时锁定 t0 */
    uint8_t          last_out;          /* 输出缓存           */
    bool             has_written;
} et_led_t;

/* 初始化(默认熄灭) */
bool et_led_init(et_led_t *l, et_led_write_fn write, void *user);

void et_led_set_off(et_led_t *l);
void et_led_set_on(et_led_t *l);

/* 闪烁: period_ms>=10, duty_pct 1~99; times=0 表示无限 */
bool et_led_set_blink(et_led_t *l, uint32_t period_ms,
                      uint8_t duty_pct, uint16_t times);

/* 呼吸: 三角波, period_ms>=100 */
bool et_led_set_breath(et_led_t *l, uint32_t period_ms);

/* 刷新输出: now 为当前毫秒 */
void et_led_poll(et_led_t *l, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_LED */
#endif /* ET_LED_H */

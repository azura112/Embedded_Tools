/**
 * @file    et_led.c
 * @brief   LED 模式管理器实现
 */
#include "et_led.h"

#if ET_MODULE_LED

bool et_led_init(et_led_t *l, et_led_write_fn write, void *user)
{
    ET_ASSERT(l != NULL);
    ET_ASSERT(write != NULL);
    if ((l == NULL) || (write == NULL)) {
        return false;
    }
    l->write         = write;
    l->user          = user;
    l->mode          = ET_LED_MODE_OFF;
    l->period_ms     = 0u;
    l->on_ms         = 0u;
    l->cycles_left   = 0u;
    l->t0            = 0u;
    l->pending_start = false;
    l->last_out      = 0u;
    l->has_written   = false;
    return true;
}

static void led_switch(et_led_t *l, et_led_mode_t mode)
{
    l->mode          = mode;
    l->pending_start = true;                /* 下次 poll 锁定相位起点 */
}

void et_led_set_off(et_led_t *l)
{
    led_switch(l, ET_LED_MODE_OFF);
}

void et_led_set_on(et_led_t *l)
{
    led_switch(l, ET_LED_MODE_ON);
}

bool et_led_set_blink(et_led_t *l, uint32_t period_ms,
                      uint8_t duty_pct, uint16_t times)
{
    uint32_t on;

    if ((period_ms < 10u) || (duty_pct == 0u) || (duty_pct >= 100u)) {
        return false;
    }
    on = period_ms * (uint32_t)duty_pct / 100u;
    if (on == 0u) {
        return false;
    }
    l->period_ms   = (uint16_t)period_ms;
    l->on_ms       = (uint16_t)on;
    l->cycles_left = times;
    led_switch(l, times != 0u ? ET_LED_MODE_BLINK_N : ET_LED_MODE_BLINK);
    return true;
}

bool et_led_set_breath(et_led_t *l, uint32_t period_ms)
{
    if (period_ms < 100u) {
        return false;
    }
    l->period_ms = (uint16_t)period_ms;
    led_switch(l, ET_LED_MODE_BREATH);
    return true;
}

/* 三角波呼吸: 前半周期线性升到 255, 后半周期线性降回 0 */
static uint8_t led_breath_level(const et_led_t *l, uint32_t elapsed)
{
    uint32_t half  = (uint32_t)l->period_ms / 2u;
    uint32_t phase = elapsed % (uint32_t)l->period_ms;
    uint32_t v;

    if (half == 0u) {
        return 0u;
    }
    v = (phase < half) ?
        (phase * 255u / half) :
        (((uint32_t)l->period_ms - phase) * 255u / half);
    return (uint8_t)(v & 0xFFu);
}

void et_led_poll(et_led_t *l, uint32_t now)
{
    uint8_t target = 0u;

    if (l->pending_start) {
        l->t0            = now;
        l->pending_start = false;
    }

    switch (l->mode) {
    case ET_LED_MODE_OFF:
        target = 0u;
        break;

    case ET_LED_MODE_ON:
        target = 255u;
        break;

    case ET_LED_MODE_BLINK: {
        uint32_t phase = (now - l->t0) % (uint32_t)l->period_ms;

        target = (phase < (uint32_t)l->on_ms) ? 255u : 0u;
        break;
    }

    case ET_LED_MODE_BLINK_N: {
        uint32_t elapsed = now - l->t0;
        uint32_t cycle   = elapsed / (uint32_t)l->period_ms;

        if (cycle >= (uint32_t)l->cycles_left) {
            led_switch(l, ET_LED_MODE_OFF); /* 次数用尽自动熄灭 */
            target = 0u;
        } else {
            uint32_t phase = elapsed % (uint32_t)l->period_ms;

            target = (phase < (uint32_t)l->on_ms) ? 255u : 0u;
        }
        break;
    }

    case ET_LED_MODE_BREATH:
        target = led_breath_level(l, now - l->t0);
        break;

    default:
        led_switch(l, ET_LED_MODE_OFF);
        target = 0u;
        break;
    }

    if ((!l->has_written) || (target != l->last_out)) {
        l->write(l->user, target);          /* 仅在亮度变化时输出 */
        l->last_out    = target;
        l->has_written = true;
    }
}

#endif /* ET_MODULE_LED */

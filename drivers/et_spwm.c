/**
 * @file    et_spwm.c
 * @brief   多通道软件 PWM 实现
 */
#include "et_spwm.h"

#if ET_MODULE_SPWM

typedef struct {
    et_spwm_write_fn write;         /* 电平输出回调       */
    void            *user;
    uint16_t         period_ms;     /* 周期(毫秒)         */
    uint16_t         on_ms;         /* 一周期内高电平时长 */
    uint8_t          duty;          /* 当前占空比设定     */
    uint32_t         t0;            /* 相位起点(首次 poll 锁定) */
    bool             active;        /* 已初始化           */
    bool             pending_start; /* 尚未锁定 t0        */
    bool             last_level;
    bool             has_written;   /* 输出缓存是否有效   */
} spwm_ch_t;

static spwm_ch_t g_ch[ET_SPWM_CH_MAX];

/* duty 0~255 -> on_ms; 255 恰好映射为整周期(恒高), 0 为恒低 */
static uint16_t spwm_duty_to_on(uint16_t period_ms, uint8_t duty)
{
    return (uint16_t)(((uint32_t)duty * (uint32_t)period_ms + 127u) / 255u);
}

bool et_spwm_init(uint8_t ch, et_spwm_write_fn fn, void *user,
                  uint16_t period_ms)
{
    ET_ASSERT(ch < ET_SPWM_CH_MAX);
    ET_ASSERT(fn != NULL);
    ET_ASSERT(period_ms >= 2u);
    if ((ch >= ET_SPWM_CH_MAX) || (fn == NULL) || (period_ms < 2u)) {
        return false;
    }
    g_ch[ch].write         = fn;
    g_ch[ch].user          = user;
    g_ch[ch].period_ms     = period_ms;
    g_ch[ch].on_ms         = 0u;    /* 默认恒低, 电平在首次 poll 输出 */
    g_ch[ch].duty          = 0u;
    g_ch[ch].t0            = 0u;
    g_ch[ch].active        = true;
    g_ch[ch].pending_start = true;
    g_ch[ch].last_level    = false;
    g_ch[ch].has_written   = false;
    return true;
}

void et_spwm_deinit(uint8_t ch)
{
    ET_ASSERT(ch < ET_SPWM_CH_MAX);
    if (ch < ET_SPWM_CH_MAX) {
        g_ch[ch].active = false;
    }
}

bool et_spwm_set(uint8_t ch, uint8_t duty)
{
    ET_ASSERT(ch < ET_SPWM_CH_MAX);
    if ((ch >= ET_SPWM_CH_MAX) || (!g_ch[ch].active)) {
        return false;
    }
    g_ch[ch].duty  = duty;
    g_ch[ch].on_ms = spwm_duty_to_on(g_ch[ch].period_ms, duty);
    return true;                        /* 相位保持连续, 生效于下次 poll */
}

uint8_t et_spwm_get_duty(uint8_t ch)
{
    ET_ASSERT(ch < ET_SPWM_CH_MAX);
    if ((ch >= ET_SPWM_CH_MAX) || (!g_ch[ch].active)) {
        return 0u;
    }
    return g_ch[ch].duty;
}

uint16_t et_spwm_get_period(uint8_t ch)
{
    ET_ASSERT(ch < ET_SPWM_CH_MAX);
    if ((ch >= ET_SPWM_CH_MAX) || (!g_ch[ch].active)) {
        return 0u;
    }
    return g_ch[ch].period_ms;
}

void et_spwm_poll(uint32_t now)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)ET_SPWM_CH_MAX; i++) {
        spwm_ch_t *c = &g_ch[i];
        bool level;

        if (!c->active) {
            continue;
        }
        if (c->pending_start) {
            c->t0            = now;     /* 首次 poll 锁定相位起点 */
            c->pending_start = false;
        }
        level = ((now - c->t0) % (uint32_t)c->period_ms) < (uint32_t)c->on_ms;
        if ((!c->has_written) || (level != c->last_level)) {
            c->write(c->user, level ? 1u : 0u);
            c->last_level  = level;
            c->has_written = true;
        }
    }
}

#endif /* ET_MODULE_SPWM */

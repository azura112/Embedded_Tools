/**
 * @file    et_softclock.h
 * @brief   软时钟: 毫秒 tick → 日历时钟 (UTC, 年月日时分秒)
 *
 * 设计要点:
 *  - 纯软件换算, 不占用任何硬件 RTC; 毫秒 tick 由调用方注入(通常为
 *    port_tick_get_ms()), 无符号减法消化回绕;
 *  - 闰年/月长用整数算法(civil_from_days), 无循环无查表;
 *  - 时基为 32 位无符号秒计数(UNIX 时间), 覆盖 1970 ~ 2106 年,
 *    无 2038 问题(有符号 time_t 才有);
 *  - 长时间休眠导致的 tick 大步进按实际 delta 推进(不做追赶上限),
 *    断电后的时间恢复配合 et_kv 保存/恢复 et_softclock_unix() 值。
 *
 * 并发约定: 单上下文模块(🏠MAIN), 跨上下文由调用方加临界区。
 */
#ifndef ET_SOFTCLOCK_H
#define ET_SOFTCLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_SOFTCLOCK

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;          /* 1970~2106 */
    uint8_t  month;         /* 1~12 */
    uint8_t  day;           /* 1~31 */
    uint8_t  hour;          /* 0~23 */
    uint8_t  min;           /* 0~59 */
    uint8_t  sec;           /* 0~59 */
} et_datetime_t;

typedef struct et_softclock {
    uint32_t unix_sec;      /* 当前 UTC 秒, 勿动 */
    uint32_t last_tick;     /* 上次 poll 的毫秒值, 勿动 */
    uint32_t acc_ms;        /* 不足 1s 的毫秒累计, 勿动 */
    bool     has_last;      /* 首次 poll 锁定基准, 勿动 */
} et_softclock_t;

/* 初始化: unix_sec 为起始 UNIX 时间(如由 et_kv 恢复) */
bool et_softclock_init(et_softclock_t *sc, uint32_t unix_sec);

/* 运行中重设时间(kv 恢复/NTP 校时) */
void et_softclock_set_unix(et_softclock_t *sc, uint32_t unix_sec);

uint32_t et_softclock_unix(const et_softclock_t *sc);

/* 推进时钟: now 为当前毫秒 tick(允许回绕); 建议挂进主循环 poll 链 */
void et_softclock_poll(et_softclock_t *sc, uint32_t now_ms);

/* 拆解当前时间; 失败(参数非法)返回 false */
bool et_softclock_get_datetime(const et_softclock_t *sc, et_datetime_t *dt);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_SOFTCLOCK */
#endif /* ET_SOFTCLOCK_H */

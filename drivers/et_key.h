/**
 * @file    et_key.h
 * @brief   按键驱动 (状态机消抖, 支持短按/长按/重复触发)
 *
 * 设计要点:
 *  - 电平读取经 read 回调抽象: 返回"已按下"(高有效由调用方转换),
 *    因此独立 IO / 矩阵键盘 / IO 扩展器均可接入;
 *  - 四态 FSM: 稳定判定双向消抖, 采样周期即调用 scan 的间隔(建议 5~20ms);
 *  - 事件在 scan 调用上下文同步回调。
 *
 * 事件语义:
 *   PRESS      按下(消抖确认)
 *   RELEASE    释放(消抖确认)
 *   CLICK      完整短按(释放时若未达长按阈值)
 *   LONG_PRESS 达到长按阈值时触发一次
 *   REPEAT     长按后按 repeat_ms 间隔连续触发(repeat_ms=0 关闭)
 *
 * 时基: now 由调用方传入(通常 port_tick_get_ms()), 与 sys 层共享同一时基。
 */
#ifndef ET_KEY_H
#define ET_KEY_H

#include <stdint.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_KEY

#ifdef __cplusplus
extern "C" {
#endif

struct et_key;                          /* 前置声明: 供回调签名使用 */

typedef enum {
    ET_KEY_PRESS = 0,
    ET_KEY_RELEASE,
    ET_KEY_CLICK,
    ET_KEY_LONG_PRESS,
    ET_KEY_REPEAT,
} et_key_event_t;

typedef bool (*et_key_read_fn)(void *user);

typedef void (*et_key_event_fn)(struct et_key *k, et_key_event_t ev, void *user);

typedef struct {
    uint32_t debounce_ms;           /* 消抖时间, 典型 15~30   */
    uint32_t long_press_ms;         /* 长按阈值, 典型 400~800 */
    uint32_t repeat_ms;             /* 连发间隔, 0=关闭       */
} et_key_params_t;

typedef struct et_key {
    et_key_read_fn    read;         /* 电平采样回调           */
    et_key_event_fn   on_event;     /* 事件回调               */
    void             *user;

    uint32_t          debounce_ms;
    uint32_t          long_ms;
    uint32_t          repeat_ms;

    uint8_t           state;        /* 内部 FSM 状态          */
    bool              long_done;    /* 本次按压是否已触发长按 */
    uint32_t          t_mark;       /* 最近一次电平跳变时刻   */
    uint32_t          t_hold;       /* 按下确认时刻           */
    uint32_t          t_rep;        /* 上次连发时刻           */
} et_key_t;

/* 初始化: prm 为 NULL 时使用默认参数(20/600/0) */
bool et_key_init(et_key_t *k,
                 et_key_read_fn read,
                 et_key_event_fn on_event,
                 void *user,
                 const et_key_params_t *prm);

/* 周期扫描: 主循环或定时中断中调用, now 为当前毫秒 */
void et_key_scan(et_key_t *k, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_KEY */
#endif /* ET_KEY_H */

/**
 * @file    et_key.c
 * @brief   按键驱动实现 (四态消抖状态机)
 */
#include "et_key.h"

#if ET_MODULE_KEY

/* FSM 状态 */
enum {
    KS_IDLE = 0,        /* 已释放, 等待按下       */
    KS_GOING_DOWN,      /* 检测到按下, 消抖中     */
    KS_DOWN,            /* 按下确认               */
    KS_GOING_UP,        /* 检测到释放, 消抖中     */
};

#define KEY_DEBOUNCE_DEFAULT 20u
#define KEY_LONG_DEFAULT     600u

static void key_fire(et_key_t *k, et_key_event_t ev)
{
    if (k->on_event != NULL) {
        k->on_event(k, ev, k->user);
    }
}

bool et_key_init(et_key_t *k,
                 et_key_read_fn read,
                 et_key_event_fn on_event,
                 void *user,
                 const et_key_params_t *prm)
{
    ET_ASSERT(k != NULL);
    ET_ASSERT(read != NULL);
    ET_ASSERT(on_event != NULL);
    if ((k == NULL) || (read == NULL) || (on_event == NULL)) {
        return false;
    }

    k->read     = read;
    k->on_event = on_event;
    k->user     = user;
    if (prm != NULL) {
        k->debounce_ms = prm->debounce_ms;
        k->long_ms     = prm->long_press_ms;
        k->repeat_ms   = prm->repeat_ms;
    } else {
        k->debounce_ms = KEY_DEBOUNCE_DEFAULT;
        k->long_ms     = KEY_LONG_DEFAULT;
        k->repeat_ms   = 0u;
    }
    if (k->debounce_ms == 0u) {
        k->debounce_ms = 1u;                /* 至少一个采样周期语义 */
    }

    k->state     = KS_IDLE;
    k->long_done = false;
    k->t_mark    = 0u;
    k->t_hold    = 0u;
    k->t_rep     = 0u;
    return true;
}

void et_key_scan(et_key_t *k, uint32_t now)
{
    bool pressed = k->read(k->user);

    switch (k->state) {

    case KS_IDLE:
        if (pressed) {
            k->t_mark = now;                /* 记录跳变起点 */
            k->state  = KS_GOING_DOWN;
        }
        break;

    case KS_GOING_DOWN:
        if (!pressed) {
            k->state = KS_IDLE;             /* 抖动回落, 忽略 */
            break;
        }
        if ((now - k->t_mark) >= k->debounce_ms) {
            k->long_done = false;
            k->t_hold    = k->t_mark + k->debounce_ms;
            key_fire(k, ET_KEY_PRESS);
            k->state = KS_DOWN;
        }
        break;

    case KS_DOWN:
        if (!pressed) {
            k->t_mark = now;
            k->state  = KS_GOING_UP;
            break;
        }
        if (!k->long_done && ((now - k->t_hold) >= k->long_ms)) {
            k->long_done = true;
            k->t_rep     = now;
            key_fire(k, ET_KEY_LONG_PRESS);
        } else if (k->long_done &&
                   (k->repeat_ms != 0u) &&
                   ((now - k->t_rep) >= k->repeat_ms)) {
            k->t_rep = now;                 /* 重锚定: 连发不追赶 */
            key_fire(k, ET_KEY_REPEAT);
        }
        break;

    case KS_GOING_UP:
        if (pressed) {
            k->state = KS_DOWN;             /* 抖动回升, 继续按压判定 */
            break;
        }
        if ((now - k->t_mark) >= k->debounce_ms) {
            key_fire(k, ET_KEY_RELEASE);
            if (!k->long_done) {
                key_fire(k, ET_KEY_CLICK);  /* 完整短按 */
            }
            k->state = KS_IDLE;             /* 长按后仅 RELEASE 不发 CLICK */
        }
        break;

    default:
        k->state = KS_IDLE;
        break;
    }
}

#endif /* ET_MODULE_KEY */

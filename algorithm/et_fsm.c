/**
 * @file    et_fsm.c
 * @brief   表驱动状态机实现 (纯逻辑, 零分配)
 *
 * 实现即语义: 表序扫描 + guard 回退链, 见 et_fsm.h 头注与
 * v1.3 开发计划 §3-P2 的语义决议。
 */
#include "et_fsm.h"

#if ET_MODULE_FSM

bool et_fsm_init(et_fsm_t *f, et_fsm_state_t init,
                 const et_fsm_trans_t *table, uint32_t count, void *user)
{
    if ((f == NULL) || (table == NULL) || (count == 0u)) {
        return false;
    }
    if (f->inited) {
        return false;               /* 重复 init 防护: 不破坏运行现场 */
    }
    f->table  = table;
    f->count  = count;
    f->state  = init;
    f->user   = user;
    f->inited = true;
    return true;
}

bool et_fsm_dispatch(et_fsm_t *f, et_fsm_event_t ev)
{
    const et_fsm_trans_t *t;
    uint32_t i;

    if ((f == NULL) || !f->inited) {
        return false;
    }
    for (i = 0u; i < f->count; i++) {
        t = &f->table[i];
        if (t->event != ev) {
            continue;               /* 表序扫描: 事件不匹配看下一条 */
        }
        if ((t->guard != NULL) && !t->guard(f->user)) {
            continue;               /* guard 拒绝: 继续向后扫(回退链) */
        }
        f->state = t->next;         /* 自迁移(next==当前态)亦走此路径 */
        if (t->action != NULL) {
            t->action(f->user);
        }
        return true;
    }
    return false;                   /* 无匹配迁移: 事件被忽略 */
}

et_fsm_state_t et_fsm_state(const et_fsm_t *f)
{
    return (f != NULL) ? f->state : (et_fsm_state_t)0u;
}

#endif /* ET_MODULE_FSM */

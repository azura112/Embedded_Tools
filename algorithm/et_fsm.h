/**
 * @file    et_fsm.h
 * @brief   表驱动状态机 (扁平迁移表 + guard/action 回调, 纯逻辑层)
 *
 * 定位:
 *  - "扁平表驱动"轻量状态机: 无层次/无并行态/无内置事件队列,
 *    复杂状态编排由应用层组合多个实例或自行在 action 内派发;
 *  - 纯算法层: 不包含 port.h, 不触碰任何硬件/时基, PC 可全量单测;
 *  - 零动态内存: 迁移表由调用方以 const 提供(可驻 flash), 运行期不可变,
 *    API 不提供任何改表入口 —— 这是"表可放 flash"的关键约束。
 *
 * 迁移语义 (v1.3 决议, 见 v1.3 开发计划 §3-P2):
 *  1. 表序扫描: 取首个 event 匹配且 guard 通过(或无 guard)的迁移生效;
 *  2. guard 失败继续向后扫 —— 允许同一事件挂多条迁移构成回退链;
 *  3. 无匹配迁移: 事件被忽略, dispatch 返回 false (应用可据此做默认处理);
 *  4. 自迁移(next == 当前态)合法, action 照常执行;
 *  5. entry/exit 不设独立钩子, 由应用在 action 内自行组合;
 *  6. 重复 init 防护: 已初始化的实例再 init 返回 false 且不破坏现场
 *     (确需复位时调用方自行清零句柄 —— 句柄归调用方所有, 与全库惯例一致)。
 *
 * 状态图示例 (两态 LED 开关, 含 guard 回退链):
 *
 *        +----------------------------------+
 *        |            (自迁移)              |
 *        v                                  |
 *     +------+  EV_TOGGLE  +------+         |
 *     | OFF  | ---------->| ON   |         |
 *     +------+ <---------- +------+         |
 *        ^      EV_TOGGLE (guard: 允许关)   |
 *        |                                 |
 *        +---- EV_FORCE_OFF (任意态) ------+
 *
 * 并发约定 (单上下文模块):
 *  - 所有 API 仅限单一上下文调用(典型为主循环);
 *  - guard/action 在 dispatch 内同步执行, 应用自担其上下文约束。
 */
#ifndef ET_FSM_H
#define ET_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_FSM

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t et_fsm_state_t;        /* 状态编码由应用自定义(0 亦合法) */
typedef uint16_t et_fsm_event_t;        /* 事件编码由应用自定义(0 亦合法) */

/* 一条迁移: event 匹配 + guard 通过(或 NULL 恒真) → 切到 next 并执行 action */
typedef struct {
    et_fsm_event_t  event;                      /* 触发事件 */
    et_fsm_state_t  next;                       /* 目标状态(可为当前态=自迁移) */
    bool          (*guard)(void *user);         /* 可为 NULL = 恒真 */
    void          (*action)(void *user);        /* 可为 NULL = 无动作 */
} et_fsm_trans_t;

typedef struct et_fsm {
    const et_fsm_trans_t *table;    /* 迁移表(const, 可驻 flash), 勿动 */
    uint32_t              count;    /* 表项数, 勿动 */
    et_fsm_state_t        state;    /* 当前状态, 勿动 */
    void                 *user;     /* 透传给 guard/action, 勿动 */
    bool                  inited;   /* 重复 init 防护位, 勿动 */
} et_fsm_t;

/* 初始化: 绑定迁移表与初始状态。
 * 参数非法(空表/空指针)或实例已初始化时返回 false 且不改变现场。🏠MAIN */
bool et_fsm_init(et_fsm_t *f, et_fsm_state_t init,
                 const et_fsm_trans_t *table, uint32_t count, void *user);

/* 派发事件: 迁移生效返回 true(含自迁移); 事件被忽略返回 false。
 * guard/action 同步执行, 本调用返回后状态机已处于新状态。🏠MAIN */
bool et_fsm_dispatch(et_fsm_t *f, et_fsm_event_t ev);

/* 查询当前状态(只读, 不做任何副作用) */
et_fsm_state_t et_fsm_state(const et_fsm_t *f);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_FSM */
#endif /* ET_FSM_H */

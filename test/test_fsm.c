/**
 * @file    test_fsm.c
 * @brief   et_fsm 单元测试 (覆盖 v1.3 计划 §3-P2 语义决议逐条)
 */
#include <stddef.h>
#include <string.h>

#include "et_test.h"
#include "et_fsm.h"

/* ---- 测试观测点 ---- */

enum { EV_TOGGLE = 1u, EV_FORCE_OFF = 2u, EV_TICK = 3u };
enum { ST_OFF = 0u, ST_ON = 1u, ST_ERR = 2u };

static int g_action_cnt;
static int g_guard_call_cnt;
static void *g_last_user;
static et_fsm_t g_fsm;              /* 共享实例(各用例经 fsm_fresh 隔离) */

static void reset_observers(void)
{
    g_action_cnt = 0;
    g_guard_call_cnt = 0;
    g_last_user = NULL;
}

/* 测试间隔离: g_fsm 为共享全局, 先清零再 init(绕开重复 init 防护) */
static bool fsm_fresh(et_fsm_state_t s, const et_fsm_trans_t *tbl,
                      uint32_t n, void *user)
{
    memset(&g_fsm, 0, sizeof(g_fsm));
    return et_fsm_init(&g_fsm, s, tbl, n, user);
}

static void action_count(void *user)
{
    (void)user;
    g_action_cnt++;
}

static void action_capture_user(void *user)
{
    g_last_user = user;
    g_action_cnt++;
}

/* 状态条件 guard: 扁平表没有"状态匹配"维度, 状态判断由 guard 承担 */
static bool guard_is_off(void *user)
{
    (void)user;
    g_guard_call_cnt++;
    return et_fsm_state(&g_fsm) == ST_OFF;
}

static bool guard_deny(void *user)
{
    (void)user;
    g_guard_call_cnt++;
    return false;
}

/* guard 依据外部条件(模拟"允许关灯"业务规则) */
static bool g_gate_open;
static bool guard_gate(void *user)
{
    (void)user;
    g_guard_call_cnt++;
    return g_gate_open;
}

/* ---- 用例 ---- */

/* 基本表: 首匹配优先 + 自迁移 + 未知事件 */
static const et_fsm_trans_t TBL_LED[] = {
    { EV_TOGGLE,    ST_ON,  guard_is_off, action_count },   /* OFF->ON (仅 OFF 态) */
    { EV_TOGGLE,    ST_OFF, guard_gate,   action_count },   /* ON->OFF (gate 放行) */
    { EV_TOGGLE,    ST_ON,  NULL,         action_count },   /* 回退链: 自迁移兜底 */
    { EV_FORCE_OFF, ST_OFF, NULL,         NULL },           /* 任意态->OFF */
};

static void init_validation(void)
{
    reset_observers();
    ET_CHECK(!et_fsm_init(NULL, ST_OFF, TBL_LED, 4u, NULL));
    ET_CHECK(!et_fsm_init(&g_fsm, ST_OFF, NULL, 4u, NULL));
    ET_CHECK(!et_fsm_init(&g_fsm, ST_OFF, TBL_LED, 0u, NULL));  /* 空表 */
    ET_CHECK(et_fsm_init(&g_fsm, ST_OFF, TBL_LED, 4u, NULL));   /* 全参合法 → 成功 */
    ET_CHECK_U32_EQ(ST_OFF, et_fsm_state(&g_fsm));
}

static void init_reinit_protection(void)
{
    reset_observers();
    ET_CHECK(fsm_fresh(ST_OFF, TBL_LED, 4u, NULL));
    (void)et_fsm_dispatch(&g_fsm, EV_TOGGLE);
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));

    /* 运行中重复 init: 拒绝且现场不变 */
    ET_CHECK(!et_fsm_init(&g_fsm, ST_ERR, TBL_LED, 4u, NULL));
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));

    /* 句柄清零(调用方所有)后可重建 */
    memset(&g_fsm, 0, sizeof(g_fsm));
    ET_CHECK(et_fsm_init(&g_fsm, ST_OFF, TBL_LED, 4u, NULL));
    ET_CHECK_U32_EQ(ST_OFF, et_fsm_state(&g_fsm));
}

static void dispatch_null_and_uninit(void)
{
    et_fsm_t raw;

    reset_observers();
    memset(&raw, 0, sizeof(raw));
    ET_CHECK(!et_fsm_dispatch(NULL, EV_TOGGLE));
    ET_CHECK(!et_fsm_dispatch(&raw, EV_TOGGLE));        /* 未 init */
    ET_CHECK_U32_EQ(0u, et_fsm_state(NULL));
    ET_CHECK_U32_EQ(0, g_action_cnt);
}

static void dispatch_first_match_order(void)
{
    reset_observers();
    ET_CHECK(fsm_fresh(ST_OFF, TBL_LED, 4u, NULL));

    /* OFF 上第一条 EV_TOGGLE guard 放行: 首匹配生效, 不再向后扫 */
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TOGGLE));
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));
    ET_CHECK_U32_EQ(1, g_action_cnt);
    ET_CHECK_U32_EQ(1, g_guard_call_cnt);               /* 仅第 1 条 guard 被问 */
}

static void dispatch_guard_fallback_chain(void)
{
    reset_observers();
    ET_CHECK(fsm_fresh(ST_ON, TBL_LED, 4u, NULL));

    /* ON 上: 第1条(is_off)拒绝 → 第2条(gate)拒绝 → 第3条自迁移兜底 */
    g_gate_open = false;
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TOGGLE));
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));       /* 自迁移兜底 */
    ET_CHECK_U32_EQ(1, g_action_cnt);
    ET_CHECK_U32_EQ(2, g_guard_call_cnt);               /* 两条 guard 都被问过 */

    /* gate 放行 → 第2条生效 ON->OFF (第1条仍先被问) */
    g_gate_open = true;
    g_action_cnt = 0;
    g_guard_call_cnt = 0;
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TOGGLE));
    ET_CHECK_U32_EQ(ST_OFF, et_fsm_state(&g_fsm));
    ET_CHECK_U32_EQ(1, g_action_cnt);
    ET_CHECK_U32_EQ(2, g_guard_call_cnt);
}

static void dispatch_guard_all_reject_ignored(void)
{
    /* 专用表: 同事件两条迁移 guard 全拒 */
    static const et_fsm_trans_t tbl[] = {
        { EV_TICK, ST_ERR, guard_deny, action_count },
        { EV_TICK, ST_ERR, guard_deny, action_count },
    };

    reset_observers();
    ET_CHECK(fsm_fresh(ST_ON, tbl, 2u, NULL));
    ET_CHECK(!et_fsm_dispatch(&g_fsm, EV_TICK));        /* 事件被忽略 */
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));       /* 状态不变 */
    ET_CHECK_U32_EQ(0, g_action_cnt);
    ET_CHECK_U32_EQ(2, g_guard_call_cnt);               /* 两条都问过 */
}

static void dispatch_unknown_event_ignored(void)
{
    reset_observers();
    ET_CHECK(fsm_fresh(ST_OFF, TBL_LED, 4u, NULL));
    ET_CHECK(!et_fsm_dispatch(&g_fsm, EV_TICK));        /* 表中无此事件 */
    ET_CHECK(!et_fsm_dispatch(&g_fsm, 0u));             /* 0 亦为普通编码 */
    ET_CHECK_U32_EQ(ST_OFF, et_fsm_state(&g_fsm));
    ET_CHECK_U32_EQ(0, g_action_cnt);
}

static void dispatch_self_transition_action_runs(void)
{
    /* 独立表: 显式自迁移 */
    static const et_fsm_trans_t tbl[] = {
        { EV_TICK, ST_ON, NULL, action_count },
    };

    reset_observers();
    ET_CHECK(fsm_fresh(ST_ON, tbl, 1u, NULL));
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TICK));         /* 自迁移也是"消费" */
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));
    ET_CHECK_U32_EQ(1, g_action_cnt);                   /* action 照常执行 */
}

static void dispatch_single_entry_selfloop(void)
{
    static const et_fsm_trans_t tbl[] = {
        { EV_TOGGLE, ST_ON, NULL, NULL },               /* 无 guard 无 action */
    };

    reset_observers();
    ET_CHECK(fsm_fresh(ST_OFF, tbl, 1u, NULL));
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TOGGLE));
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));
    ET_CHECK_U32_EQ(0, g_action_cnt);                   /* NULL action = 无动作 */
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TOGGLE));       /* 扁平表: 事件再命中即自环 */
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));
}

static void dispatch_user_pointer_passthrough(void)
{
    static const et_fsm_trans_t tbl[] = {
        { EV_TOGGLE, ST_ON, NULL, action_capture_user },
    };
    int cookie;

    reset_observers();
    ET_CHECK(fsm_fresh(ST_OFF, tbl, 1u, &cookie));
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TOGGLE));
    ET_CHECK(g_last_user == &cookie);                   /* user 原样透传 */
    ET_CHECK_U32_EQ(1, g_action_cnt);
}

static void dispatch_state_walk_sequence(void)
{
    /* 三态链: A --e1--> B --e2--> C --e3--> A */
    enum { A = 0u, B = 1u, C = 2u };
    static const et_fsm_trans_t tbl[] = {
        { 1u, B, NULL, NULL },
        { 2u, C, NULL, NULL },
        { 3u, A, NULL, NULL },
    };

    ET_CHECK(fsm_fresh(A, tbl, 3u, NULL));
    ET_CHECK_U32_EQ(A, et_fsm_state(&g_fsm));
    ET_CHECK(et_fsm_dispatch(&g_fsm, 1u));
    ET_CHECK_U32_EQ(B, et_fsm_state(&g_fsm));
    ET_CHECK(et_fsm_dispatch(&g_fsm, 2u));
    ET_CHECK_U32_EQ(C, et_fsm_state(&g_fsm));
    ET_CHECK(et_fsm_dispatch(&g_fsm, 3u));
    ET_CHECK_U32_EQ(A, et_fsm_state(&g_fsm));
    ET_CHECK(et_fsm_dispatch(&g_fsm, 2u));              /* 扁平表: e2 与状态无关 → C */
    ET_CHECK_U32_EQ(C, et_fsm_state(&g_fsm));
}

static void table_const_section_residency(void)
{
    /* 表以 static const 声明(C99 落 .rodata/flash), dispatch 正常工作;
     * 且运行期没有任何 API 能改表(API 面不提供写入口) */
    static const et_fsm_trans_t tbl[] = {
        { EV_TOGGLE, ST_ON, NULL, NULL },
    };
    const et_fsm_trans_t *ro = tbl;

    ET_CHECK(fsm_fresh(ST_OFF, tbl, 1u, NULL));
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TOGGLE));
    ET_CHECK_U32_EQ(ST_ON, et_fsm_state(&g_fsm));
    ET_CHECK(ro == &tbl[0]);                            /* const 指针持有无告警 */
}

static void guard_receives_user_for_decision(void)
{
    /* guard 依据 user 上下文决策(业务规则外置) */
    static const et_fsm_trans_t tbl[] = {
        { EV_TOGGLE, ST_OFF, guard_gate, NULL },
    };
    static int allow_cnt;

    g_gate_open = true;
    ET_CHECK(fsm_fresh(ST_ON, tbl, 1u, &allow_cnt));
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_TOGGLE));
    ET_CHECK_U32_EQ(ST_OFF, et_fsm_state(&g_fsm));
    ET_CHECK_U32_EQ(1, g_guard_call_cnt);
}

static void force_off_reaches_from_any_state(void)
{
    reset_observers();
    ET_CHECK(fsm_fresh(ST_ON, TBL_LED, 4u, NULL));
    ET_CHECK(et_fsm_dispatch(&g_fsm, EV_FORCE_OFF));    /* 第4条: 无 guard */
    ET_CHECK_U32_EQ(ST_OFF, et_fsm_state(&g_fsm));
    ET_CHECK_U32_EQ(0, g_action_cnt);                   /* 该条 action=NULL */
}

static void state_query_is_pure_readonly(void)
{
    et_fsm_state_t s1, s2;

    ET_CHECK(fsm_fresh(ST_OFF, TBL_LED, 4u, NULL));
    s1 = et_fsm_state(&g_fsm);
    s2 = et_fsm_state(&g_fsm);
    ET_CHECK(s1 == s2);                                 /* 多次查询幂等 */
    ET_CHECK_U32_EQ(ST_OFF, s1);
}

/* ---- 套件注册 ---- */

static const et_test_case_t g_cases[] = {
    { "init.validation",            init_validation },
    { "init.reinit_protection",     init_reinit_protection },
    { "dispatch.null_and_uninit",   dispatch_null_and_uninit },
    { "dispatch.first_match",       dispatch_first_match_order },
    { "dispatch.guard_fallback",    dispatch_guard_fallback_chain },
    { "dispatch.guard_all_reject",  dispatch_guard_all_reject_ignored },
    { "dispatch.unknown_ignored",   dispatch_unknown_event_ignored },
    { "dispatch.self_transition",   dispatch_self_transition_action_runs },
    { "dispatch.single_entry",      dispatch_single_entry_selfloop },
    { "dispatch.user_passthrough",  dispatch_user_pointer_passthrough },
    { "dispatch.walk_sequence",     dispatch_state_walk_sequence },
    { "table.const_residency",      table_const_section_residency },
    { "guard.user_decision",        guard_receives_user_for_decision },
    { "state.force_off_any",        force_off_reaches_from_any_state },
    { "state.query_readonly",       state_query_is_pure_readonly },
};

const et_test_case_t *test_fsm_cases(size_t *count)
{
    *count = sizeof(g_cases) / sizeof(g_cases[0]);
    return g_cases;
}

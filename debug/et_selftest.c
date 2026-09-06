/**
 * @file    et_selftest.c
 * @brief   板上自测组件实现 (v1.7)
 *
 * 套件来源: G474 工程 AT+SELFTEST 已验证实现移植(13) + v1.4~v1.6 新模块补齐(4):
 *   ringbuf/queue/mempool/list/filter/fsm/sched/event/stimer/crc/frame/softclock/wdt
 *   + atcmd+shell 行解析 / xmodem 短传输(RAM 环回) / kv 冒烟(存储门控) /
 *   bootctl 状态机(存储门控)。
 *
 * 与 G474 工程私有版差异:
 *   - 输出经结构化事件回调(不在组件内格式化), 接 et_log 或 shell 由应用决定;
 *   - sched/stimer 套件改为【自洽性断言】(arm 1000ms → next_due ∈ [990,1000]),
 *     去掉真实时基忙等 —— host 注入式时基与真机均可确定性通过;
 *   - kv/bootctl 为破坏性套件(擦写所配扇区), 默认未配置即 SKIP。
 *
 * 框架自测(test/test_selftest.c)覆盖: 注册表/失败计数/报告回调/动态注册/
 * 未知套件/存储门控跳过。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "et_selftest.h"
#include "et_config.h"
#include "port.h"

#if ET_MODULE_SELFTEST

#if ET_MODULE_RINGBUF
#include "et_ringbuf.h"
#endif
#if ET_MODULE_QUEUE
#include "et_queue.h"
#endif
#if ET_MODULE_MEMPOOL
#include "et_mempool.h"
#endif
#if ET_MODULE_LIST
#include "et_list.h"
#endif
#if ET_MODULE_FILTER
#include "et_filter.h"
#endif
#if ET_MODULE_FSM
#include "et_fsm.h"
#endif
#if ET_MODULE_STIMER
#include "et_stimer.h"
#endif
#if ET_MODULE_SCHED
#include "et_sched.h"
#endif
#if ET_MODULE_EVENT
#include "et_event.h"
#endif
#if ET_MODULE_CRC
#include "et_crc.h"
#endif
#if ET_MODULE_FRAME
#include "et_frame.h"
#endif
#if ET_MODULE_SOFTCLOCK
#include "et_softclock.h"
#endif
#if ET_MODULE_WDT
#include "et_wdt.h"
#endif
#if ET_MODULE_XMODEM
#include "et_xmodem.h"
#endif
#if ET_MODULE_ATCMD
#include "et_atcmd.h"
#endif
#if ET_MODULE_KV
#include "et_kv.h"
#endif
#if ET_MODULE_BOOTCTL
#include "et_bootctl.h"
#endif

/* ===================== 框架: 上下文与断言 ===================== */

typedef struct {
    et_selftest_report_fn report;
    void                 *user;
    const char           *name;
    int                   fails;
} st_ctx_t;

#define ST_CHECK(cond)                                                      \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ctx->fails++;                                                   \
            if (ctx->report != NULL) {                                      \
                ctx->report(ctx->user, ET_SELFTEST_CHECK_FAIL,              \
                            ctx->name, (uint32_t)__LINE__);                 \
            }                                                               \
        }                                                                   \
    } while (0)

typedef bool (*st_impl_fn)(st_ctx_t *ctx);

/* 动态注册槽 (零动态内存) */
#ifndef ET_SELFTEST_MAX_EXTRA
#define ET_SELFTEST_MAX_EXTRA   4u
#endif

typedef struct {
    const char       *name;
    et_selftest_suite_fn fn;
    bool              used;
} st_extra_t;

#if ET_MODULE_KV
static const et_kv_layout_t  *g_st_kv;
#endif
#if ET_MODULE_BOOTCTL
static const et_bootctl_cfg_t *g_st_bc;
#endif

#if ET_MODULE_KV
void et_selftest_set_kv_layout(const et_kv_layout_t *lay)
{
    g_st_kv = lay;
}
#endif

#if ET_MODULE_BOOTCTL
void et_selftest_set_bootctl_cfg(const et_bootctl_cfg_t *cfg)
{
    g_st_bc = cfg;
}
#endif

void et_selftest_note_fail(et_selftest_report_fn report, void *user,
                           const char *suite, uint32_t line)
{
    if (report != NULL) {
        report(user, ET_SELFTEST_CHECK_FAIL, suite, line);
    }
}

/* ===================== 1. ringbuf ===================== */
#if ET_MODULE_RINGBUF
static bool st_ringbuf(st_ctx_t *ctx)
{
    static et_ringbuf_t rb;
    static uint8_t      mem[16];
    uint8_t             buf[16];
    uint32_t            free_before;
    uint32_t            wrote;
    uint32_t            i;

    memset(&rb, 0, sizeof(rb));
    ST_CHECK(et_ringbuf_init(&rb, mem, sizeof(mem)));
    ST_CHECK(et_ringbuf_is_empty(&rb) && (et_ringbuf_used(&rb) == 0u));

    for (i = 0u; i < 5u; i++) {
        buf[i] = (uint8_t)(i * 7u + 3u);
    }
    ST_CHECK(et_ringbuf_write(&rb, buf, 5u) == 5u);
    memset(buf, 0, sizeof(buf));
    ST_CHECK(et_ringbuf_read(&rb, buf, 5u) == 5u);
    for (i = 0u; i < 5u; i++) {
        ST_CHECK(buf[i] == (uint8_t)(i * 7u + 3u));
    }

    for (i = 0u; i < 6u; i++) {             /* 回绕: 部分读写跨索引回绕点 */
        uint8_t  w[10];
        uint8_t  r[10];
        uint32_t j;

        for (j = 0u; j < 10u; j++) {
            w[j] = (uint8_t)(i * 10u + j);
        }
        ST_CHECK(et_ringbuf_write(&rb, w, 10u) == 10u);
        ST_CHECK(et_ringbuf_read(&rb, r, 10u) == 10u);
        ST_CHECK(memcmp(w, r, 10u) == 0);
    }

    {
        uint8_t w[4] = { 0xA1u, 0xA2u, 0xA3u, 0xA4u };
        uint8_t r[2];

        ST_CHECK(et_ringbuf_write(&rb, w, 4u) == 4u);
        ST_CHECK(et_ringbuf_peek(&rb, r, 2u) == 2u);
        ST_CHECK((r[0] == 0xA1u) && (r[1] == 0xA2u));
        et_ringbuf_drop(&rb, 2u);
        ST_CHECK(et_ringbuf_read(&rb, r, 2u) == 2u);
        ST_CHECK((r[0] == 0xA3u) && (r[1] == 0xA4u));
    }

    free_before = et_ringbuf_free_space(&rb);
    wrote = et_ringbuf_write(&rb, buf, sizeof(buf));
    ST_CHECK(wrote == free_before);
    ST_CHECK(et_ringbuf_is_full(&rb));
    ST_CHECK(et_ringbuf_write(&rb, buf, 1u) == 0u);

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_RINGBUF */

/* ===================== 2. queue ===================== */
#if ET_MODULE_QUEUE
static bool st_queue(st_ctx_t *ctx)
{
    static et_queue_t q;
    static uint32_t   mem[4];
    uint32_t          v;
    uint32_t          i;

    memset(&q, 0, sizeof(q));
    ST_CHECK(et_queue_init(&q, mem, sizeof(mem), sizeof(uint32_t)));
    ST_CHECK(et_queue_is_empty(&q) && (et_queue_capacity(&q) == 4u));

    for (i = 1u; i <= 3u; i++) {
        ST_CHECK(et_queue_push(&q, &i));
    }
    for (i = 1u; i <= 3u; i++) {            /* FIFO 序 */
        v = 0u;
        ST_CHECK(et_queue_pop(&q, &v));
        ST_CHECK(v == i);
    }
    ST_CHECK(!et_queue_pop(&q, &v));

    for (i = 0u; i < 4u; i++) {
        ST_CHECK(et_queue_push(&q, &i));
    }
    ST_CHECK(et_queue_is_full(&q));
    ST_CHECK(!et_queue_push(&q, &i));

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_QUEUE */

/* ===================== 3. mempool ===================== */
#if ET_MODULE_MEMPOOL
static bool st_mempool(st_ctx_t *ctx)
{
    static et_mempool_t mp;
    static uint8_t      mem[64];            /* 4×8B + 位图/对齐开销 */
    void               *blk[5];
    uint32_t            i;

    memset(&mp, 0, sizeof(mp));
    ST_CHECK(et_mempool_init(&mp, mem, sizeof(mem), 8u, 4u));
    ST_CHECK(et_mempool_free_count(&mp) == 4u);

    for (i = 0u; i < 4u; i++) {
        blk[i] = et_mempool_alloc(&mp);
        ST_CHECK((blk[i] != NULL) && et_mempool_contains(&mp, blk[i]));
    }
    ST_CHECK(et_mempool_alloc(&mp) == NULL);        /* 耗尽 */
    ST_CHECK(!et_mempool_contains(&mp, ctx));       /* 池外指针不认 */

    ST_CHECK(et_mempool_free(&mp, blk[1]));
    blk[4] = et_mempool_alloc(&mp);
    ST_CHECK(blk[4] != NULL);
    for (i = 0u; i < 4u; i++) {
        ST_CHECK(et_mempool_free(&mp, blk[i]));
    }
    ST_CHECK(!et_mempool_free(&mp, blk[0]));        /* 双重释放拒绝 */
    ST_CHECK(et_mempool_free_count(&mp) == 4u);

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_MEMPOOL */

/* ===================== 4. list ===================== */
#if ET_MODULE_LIST
typedef struct {
    et_list_node_t node;
    uint8_t        val;
} st_item_t;

static uint8_t   g_v_visit[8];
static uint32_t  g_v_n;
static st_item_t *g_v_removed;

static void st_visit(et_list_node_t *n, void *user)
{
    st_item_t *it = (st_item_t *)n;

    if ((g_v_removed != NULL) && (it == g_v_removed)) {
        (void)et_list_remove((et_list_t *)user, &it->node);
        return;                             /* 遍历中自删: 不计入访问序 */
    }
    if (g_v_n < 8u) {
        g_v_visit[g_v_n++] = it->val;
    }
}

static bool st_list(st_ctx_t *ctx)
{
    static et_list_t l;
    static st_item_t it[4];

    et_list_init(&l);
    g_v_n = 0u;
    g_v_removed = NULL;
    it[0].val = 'a';
    it[1].val = 'b';
    it[2].val = 'c';
    it[3].val = 'z';
    ST_CHECK(et_list_push_back(&l, &it[0].node));
    ST_CHECK(et_list_push_back(&l, &it[1].node));
    ST_CHECK(et_list_push_back(&l, &it[2].node));
    ST_CHECK(et_list_push_front(&l, &it[3].node));
    ST_CHECK(et_list_count(&l) == 4u);

    et_list_foreach(&l, st_visit, &l);      /* 序: z a b c */
    ST_CHECK((g_v_n == 4u) && (g_v_visit[0] == 'z') &&
             (g_v_visit[1] == 'a') && (g_v_visit[2] == 'b') &&
             (g_v_visit[3] == 'c'));

    ST_CHECK(et_list_remove(&l, &it[1].node));
    g_v_n = 0u;
    g_v_removed = &it[0];
    et_list_foreach(&l, st_visit, &l);      /* 遍历中自删 a: c 仍被走到 */
    g_v_removed = NULL;
    ST_CHECK((et_list_count(&l) == 2u) &&
             (g_v_visit[0] == 'z') && (g_v_visit[1] == 'c'));

    ST_CHECK(et_list_remove(&l, &it[3].node));
    ST_CHECK(et_list_remove(&l, &it[2].node));
    ST_CHECK(et_list_is_empty(&l) &&
             (et_list_front(&l) == NULL) && (et_list_back(&l) == NULL));

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_LIST */

/* ===================== 5. filter ===================== */
#if ET_MODULE_FILTER
static bool st_filter(st_ctx_t *ctx)
{
    static et_movavg_t mv;
    static int32_t     mv_mem[4];
    static et_lpf1_t   lp;

    memset(&mv, 0, sizeof(mv));
    ST_CHECK(et_movavg_init(&mv, mv_mem, 4u));
    ST_CHECK(et_movavg_update(&mv, 100) == 100);
    (void)et_movavg_update(&mv, 100);
    (void)et_movavg_update(&mv, 100);
    ST_CHECK(et_movavg_update(&mv, 100) == 100);
    ST_CHECK(et_movavg_update(&mv, 0) == 75);
    ST_CHECK(et_movavg_update(&mv, 0) == 50);
    ST_CHECK(et_movavg_update(&mv, 0) == 25);
    ST_CHECK(et_movavg_update(&mv, 0) == 0);

    memset(&lp, 0, sizeof(lp));
    ST_CHECK(et_lpf1_init(&lp, 32768u));    /* k=1.0: 直通 */
    ST_CHECK(et_lpf1_update(&lp, 1234) == 1234);
    ST_CHECK(et_lpf1_output(&lp) == 1234);

    memset(&lp, 0, sizeof(lp));
    ST_CHECK(et_lpf1_init(&lp, 16384u));    /* k=0.5 */
    ST_CHECK(et_lpf1_update(&lp, 100) == 100);      /* 首样本直通 */
    ST_CHECK(et_lpf1_update(&lp, 200) == 150);
    ST_CHECK(et_lpf1_update(&lp, 200) == 175);

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_FILTER */

/* ===================== 6. fsm ===================== */
#if ET_MODULE_FSM
#define STS_EV_ON        1u
#define STS_EV_OFF       2u
#define STS_EV_FORCE_OFF 3u
#define STS_ST_OFF       0u
#define STS_ST_ON        1u

static uint32_t g_act_on;
static uint32_t g_act_off;
static bool     g_allow_off;

static bool sts_guard_allow_off(void *user)
{
    (void)user;
    return g_allow_off;
}

static void sts_action_on(void *user)
{
    (void)user;
    g_act_on++;
}

static void sts_action_off(void *user)
{
    (void)user;
    g_act_off++;
}

static bool st_fsm(st_ctx_t *ctx)
{
    /* 扁平表按 event 匹配(无源状态域): 首个 event 匹配且 guard 通过者生效 */
    static const et_fsm_trans_t table[] = {
        { STS_EV_ON,        STS_ST_ON,  NULL,               sts_action_on  },
        { STS_EV_OFF,       STS_ST_OFF, sts_guard_allow_off, sts_action_off },
        { STS_EV_FORCE_OFF, STS_ST_OFF, NULL,               NULL           },
    };
    static et_fsm_t fsm;

    g_act_on = 0u;
    g_act_off = 0u;
    g_allow_off = true;
    memset(&fsm, 0, sizeof(fsm));
    ST_CHECK(et_fsm_init(&fsm, STS_ST_OFF, table, 3u, NULL));

    ST_CHECK(et_fsm_dispatch(&fsm, STS_EV_ON));
    ST_CHECK(et_fsm_state(&fsm) == STS_ST_ON);

    g_allow_off = false;                    /* guard 拒绝 → 事件忽略 */
    ST_CHECK(!et_fsm_dispatch(&fsm, STS_EV_OFF));
    ST_CHECK(et_fsm_state(&fsm) == STS_ST_ON);

    g_allow_off = true;
    ST_CHECK(et_fsm_dispatch(&fsm, STS_EV_OFF));
    ST_CHECK(et_fsm_state(&fsm) == STS_ST_OFF);

    ST_CHECK(et_fsm_dispatch(&fsm, STS_EV_FORCE_OFF));  /* 自迁移 */
    ST_CHECK(!et_fsm_dispatch(&fsm, 99u));              /* 无匹配: 忽略 */
    ST_CHECK(!et_fsm_init(&fsm, STS_ST_OFF, table, 3u, NULL));  /* 重复 init */

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_FSM */

/* ===================== 7. sched (自洽性, 无忙等) ===================== */
#if ET_MODULE_SCHED
static uint32_t g_sched_cnt;

static void st_sched_cb(void *arg)
{
    (void)arg;
    g_sched_cnt++;
}

static bool st_sched(st_ctx_t *ctx)
{
    static et_task_t t;
    port_tick_ms_t   rem;

    memset(&t, 0, sizeof(t));
    g_sched_cnt = 0u;
    et_sched_reset();
    ST_CHECK(!et_sched_register(&t, st_sched_cb, NULL, 0u));    /* 非法周期 */
    ST_CHECK(et_sched_register(&t, st_sched_cb, NULL, 1000u));

    /* 自洽性: 注册后 1s 内应到期 (host 注入时基恒 1000, 真机允许 ≤10ms 漂移) */
    rem = et_sched_next_due();
    ST_CHECK((rem >= 990u) && (rem <= 1000u));

    et_sched_poll_once();
    ST_CHECK(g_sched_cnt == 0u);            /* 未到期不执行 */
    ST_CHECK(et_sched_unregister(&t));
    ST_CHECK(et_sched_next_due() == PORT_TICK_WAIT_FOREVER);    /* 注销重算 */

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_SCHED */

/* ===================== 8. event ===================== */
#if ET_MODULE_EVENT
static bool st_event(st_ctx_t *ctx)
{
    static et_event_group_t g;

    et_event_init(&g);
    ST_CHECK(et_event_peek(&g) == 0u);

    et_event_set(&g, 0x5u);                 /* 模拟 ISR 置位 */
    ST_CHECK(et_event_peek(&g) == 0x5u);
    ST_CHECK(et_event_wait_and_clear(&g, 0x1u) == 0x1u);    /* 取走即清 */
    ST_CHECK(et_event_peek(&g) == 0x4u);
    ST_CHECK(et_event_wait_and_clear(&g, 0x1u) == 0u);
    et_event_set(&g, 0x80u);
    ST_CHECK(et_event_wait_and_clear(&g, 0xF0u) == 0x80u);  /* 掩码相交 */
    et_event_set(&g, 0x4u);
    et_event_clear(&g, 0x4u);
    ST_CHECK(et_event_peek(&g) == 0u);

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_EVENT */

/* ===================== 9. stimer (自洽性, 无忙等) ===================== */
#if ET_MODULE_STIMER
static uint32_t g_tmr_cnt;

static void st_tmr_cb(void *arg)
{
    (void)arg;
    g_tmr_cnt++;
}

static bool st_stimer(st_ctx_t *ctx)
{
    static et_stimer_t t;
    port_tick_ms_t     rem;

    memset(&t, 0, sizeof(t));
    g_tmr_cnt = 0u;
    et_stimer_reset_all();
    ST_CHECK(et_stimer_next_due() == PORT_TICK_WAIT_FOREVER);   /* 空表 */
    ST_CHECK(et_stimer_init(&t, st_tmr_cb, NULL));
    ST_CHECK(et_stimer_start_oneshot(&t, 1000u));

    rem = et_stimer_next_due();
    ST_CHECK((rem >= 990u) && (rem <= 1000u));
    et_stimer_poll(port_tick_get_ms());
    ST_CHECK(g_tmr_cnt == 0u);              /* 未到期不触发 */

    ST_CHECK(et_stimer_stop(&t));
    ST_CHECK(!et_stimer_is_running(&t));
    ST_CHECK(et_stimer_next_due() == PORT_TICK_WAIT_FOREVER);   /* stop 重算 */

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_STIMER */

/* ===================== 10. crc ===================== */
#if ET_MODULE_CRC
static bool st_crc(st_ctx_t *ctx)
{
    static const char vec[] = "123456789";
    uint32_t          a;
    uint32_t          b;

    ST_CHECK(et_crc8(vec, 9u) == 0xF4u);
    ST_CHECK(et_crc16_modbus(vec, 9u) == 0x4B37u);
    ST_CHECK(et_crc16_ccitt(vec, 9u) == 0x29B1u);
    ST_CHECK(et_crc32(vec, 9u) == 0xCBF43926u);

    a = et_crc32(vec, 9u);                  /* 流式 == 一次性 */
    b = et_crc32_update(ET_CRC32_INIT, vec, 4u);
    b = et_crc32_update(b, vec + 4u, 5u) ^ ET_CRC32_INIT;
    ST_CHECK(a == b);

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_CRC */

/* ===================== 11. frame ===================== */
#if ET_MODULE_FRAME
static uint8_t  g_frm_pay[16];
static uint16_t g_frm_len;
static uint32_t g_frm_ok;

static void st_on_frame(struct et_frame_parser *p, uint16_t len, void *user)
{
    (void)user;
    if (len <= sizeof(g_frm_pay)) {
        memcpy(g_frm_pay, p->rx_buf, len);
        g_frm_len = len;
        g_frm_ok++;
    }
}

static bool st_frame(st_ctx_t *ctx)
{
    static const uint8_t     hdr[2] = { 0xAAu, 0x55u };
    static et_frame_cfg_t    cfg;
    static et_frame_parser_t p;
    uint8_t                  out[32];
    uint8_t                  pay[2] = { 'h', 'i' };
    uint16_t                 n;
    uint32_t                 i;

    g_frm_ok = 0u;
    g_frm_len = 0u;
    memset(&cfg, 0, sizeof(cfg));
    memset(&p, 0, sizeof(p));
    cfg.header = hdr;
    cfg.header_len = 2u;
    cfg.use_len = true;
    cfg.crc = ET_FRAME_CRC_CRC16_MODBUS;
    cfg.rx_buf = g_frm_pay;
    cfg.rx_cap = sizeof(g_frm_pay);
    cfg.on_frame = st_on_frame;

    ST_CHECK(et_frame_parser_init(&p, &cfg));
    n = et_frame_pack(&cfg, pay, 2u, out, sizeof(out));
    ST_CHECK(n == 7u);                      /* 头2+长1+载荷2+CRC2 */

    (void)et_frame_feed(&p, 0x00u);         /* 前导噪声后重同步 */
    (void)et_frame_feed(&p, 0x77u);
    for (i = 0u; i < n; i++) {
        (void)et_frame_feed(&p, out[i]);
    }
    ST_CHECK((g_frm_ok == 1u) && (g_frm_len == 2u) &&
             (g_frm_pay[0] == 'h') && (g_frm_pay[1] == 'i'));
    ST_CHECK((p.frame_count == 1u) && (p.err_count == 0u));

    out[6u] ^= 0xFFu;                       /* 篡改 CRC → 坏帧 */
    for (i = 0u; i < n; i++) {
        (void)et_frame_feed(&p, out[i]);
    }
    ST_CHECK((g_frm_ok == 1u) && (p.err_count == 1u));

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_FRAME */

/* ===================== 12. softclock ===================== */
#if ET_MODULE_SOFTCLOCK
static bool st_softclock(st_ctx_t *ctx)
{
    static et_softclock_t sc;
    et_datetime_t         dt;

    memset(&sc, 0, sizeof(sc));
    ST_CHECK(et_softclock_init(&sc, 1767225600u));      /* 2026-01-01 */
    ST_CHECK(et_softclock_get_datetime(&sc, &dt));
    ST_CHECK((dt.year == 2026u) && (dt.month == 1u) && (dt.day == 1u) &&
             (dt.hour == 0u) && (dt.min == 0u) && (dt.sec == 0u));

    et_softclock_set_unix(&sc, 951782400u);             /* 闰日 2000-02-29 */
    ST_CHECK(et_softclock_get_datetime(&sc, &dt));
    ST_CHECK((dt.year == 2000u) && (dt.month == 2u) && (dt.day == 29u));
    ST_CHECK(et_softclock_unix(&sc) == 951782400u);     /* 往返一致 */

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_SOFTCLOCK */

/* ===================== 13. wdt (契约负样本, 不启动看门狗) ===================== */
#if ET_MODULE_WDT
static bool st_wdt(st_ctx_t *ctx)
{
    /* 库层下限: < 2×ERASE_MS_MAX 一律拒绝 (按宏相对取值, 双几何成立) */
    ST_CHECK(!et_wdt_enable((PORT_FLASH_ERASE_MS_MAX * 2u) - 1u));
    /* disable 语义 = port 语义 (IWDG 类平台恒 false, host 虚拟 WDT 可停) */
    ST_CHECK(et_wdt_disable() == port_wdt_disable());

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_WDT */

/* ===================== 14. atcmd + shell 行解析 (RAM) ===================== */
#if defined(ET_MODULE_ATCMD) && ET_MODULE_ATCMD && defined(ET_MODULE_SHELL) && ET_MODULE_SHELL
#include "et_shell.h"
static et_atcmd_proc_t g_at;
static et_shell_t      g_sh;
static char            g_line[32];
static bool            g_pinged;
static uint32_t        g_echo_n;

static void st_shell_put(void *user, char ch)
{
    (void)user;
    (void)ch;
    g_echo_n++;
}

static void st_cmd_ping(char *args, void *user)
{
    (void)args;
    (void)user;
    g_pinged = true;
}

static bool st_atcmd_shell(st_ctx_t *ctx)
{
    static const et_atcmd_entry_t tbl[] = {
        { "PING", st_cmd_ping, "selftest" },
    };

    g_pinged = false;
    g_echo_n = 0u;
    memset(&g_at, 0, sizeof(g_at));
    memset(&g_sh, 0, sizeof(g_sh));
    ST_CHECK(et_atcmd_init(&g_at, tbl, 1u, g_line, sizeof(g_line), NULL));
    ST_CHECK(et_shell_init(&g_sh, &g_at, st_shell_put, NULL));

    {
        static const char line[] = { 'A', 'T', '+', 'P', 'I', 'N', 'G', '\r' };
        uint32_t i;

        for (i = 0u; i < (sizeof(line) - 1u); i++) {
            (void)et_shell_feed(&g_sh, line[i]);    /* 攒行(无返回语义) */
        }
        ST_CHECK(et_shell_feed(&g_sh, line[7]));    /* 行完成 -> true */
    }
    ST_CHECK(g_pinged);                     /* 行完成 → 命令分发 */
    ST_CHECK(g_echo_n >= 7u);               /* 回显发生 */

    return (ctx->fails == 0);
}
#endif /* ATCMD && SHELL */

/* ===================== 15. xmodem 短传输 (RAM 环回) ===================== */
#if ET_MODULE_XMODEM
static et_xmodem_t g_xm;
static uint8_t     g_xm_buf[132];
static uint8_t     g_xm_img[384];
static uint32_t    g_xm_len;

static bool st_xm_sink(void *user, uint32_t off, const uint8_t *d, uint32_t len)
{
    (void)user;
    if ((off != g_xm_len) || ((off + len) > sizeof(g_xm_img))) {
        return false;
    }
    memcpy(g_xm_img + off, d, len);
    g_xm_len += len;
    return true;
}

/* 最小编码器: SOH+块号+~块号+128B+CRC16(高字节在前), 与 et_xmodem 对齐 */
static void st_xm_put_block(et_xmodem_t *xm, uint8_t blk,
                            const uint8_t *payload, bool corrupt, uint32_t *now)
{
    uint8_t  f[133];
    uint16_t crc = et_crc16_ccitt_update(0x0000u, payload, 128u);
    uint32_t i;

    f[0] = ET_XM_SOH;
    f[1] = blk;
    f[2] = (uint8_t)(~(uint8_t)blk);
    memcpy(&f[3], payload, 128u);
    if (corrupt) {
        crc ^= 0x5A5Au;
    }
    f[131] = (uint8_t)(crc >> 8);
    f[132] = (uint8_t)(crc & 0xFFu);
    for (i = 0u; i < sizeof(f); i++) {
        (void)et_xmodem_rx(xm, f[i], (*now)++);
    }
}

static bool st_xmodem(st_ctx_t *ctx)
{
    static uint8_t payload[384];
    uint32_t       now = 0u;
    uint32_t       i;
    et_xm_act_t    act;

    g_xm_len = 0u;
    memset(&g_xm, 0, sizeof(g_xm));
    for (i = 0u; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i * 5u + 1u);
    }
    et_xmodem_rx_init(&g_xm, g_xm_buf, sizeof(g_xm_buf), st_xm_sink, NULL);

    st_xm_put_block(&g_xm, 1u, payload, false, &now);
    st_xm_put_block(&g_xm, 2u, payload + 128u, true, &now);     /* CRC 坏 → NAK */
    st_xm_put_block(&g_xm, 2u, payload + 128u, false, &now);    /* 重发 → ACK */
    st_xm_put_block(&g_xm, 3u, payload + 256u, false, &now);
    ST_CHECK(g_xm.total == 384u);           /* EOT 握手前取值(DONE 会复位会话) */

    act = et_xmodem_rx(&g_xm, ET_XM_EOT, now++);                /* EOT#1 → NAK */
    ST_CHECK(act == ET_XM_NAK);
    act = et_xmodem_rx(&g_xm, ET_XM_EOT, now++);                /* EOT#2 → DONE */
    ST_CHECK(act == ET_XM_DONE);

    ST_CHECK(g_xm_len == 384u);
    ST_CHECK(memcmp(g_xm_img, payload, 384u) == 0);

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_XMODEM */

/* ===================== 16. kv 冒烟 (存储门控: 擦写所配扇区) ===================== */
#if ET_MODULE_KV
static bool st_kv(st_ctx_t *ctx)
{
    static et_kv_t kv;
    const et_kv_layout_t *lay = g_st_kv;
    uint32_t v = 0x12345678u;
    uint32_t r = 0u;
    et_kv_stats_t st;

    memset(&kv, 0, sizeof(kv));
    ST_CHECK(et_kv_format(&kv, lay));
    ST_CHECK(et_kv_set(&kv, 1u, &v, sizeof(v)));
    ST_CHECK(et_kv_get(&kv, 1u, &r, sizeof(r), NULL) && (r == v));
    et_kv_stats(&kv, &st);
    ST_CHECK(st.key_count == 1u);

    v = 0x0BADF00Du;
    ST_CHECK(et_kv_set(&kv, 1u, &v, sizeof(v)));        /* 覆盖写 */
    r = 0u;
    ST_CHECK(et_kv_get(&kv, 1u, &r, sizeof(r), NULL) && (r == 0x0BADF00Du));

    ST_CHECK(et_kv_del(&kv, 1u));                       /* tombstone */
    ST_CHECK(!et_kv_get(&kv, 1u, &r, sizeof(r), NULL));
    ST_CHECK(et_kv_size(&kv, 1u) == 0u);

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_KV */

/* ===================== 17. bootctl 状态机 (存储门控) ===================== */
#if ET_MODULE_BOOTCTL
static bool st_bootctl(st_ctx_t *ctx)
{
    static et_bootctl_t bc;
    const et_bootctl_cfg_t *cfg = g_st_bc;
    et_bootctl_state_t st;
    uint8_t            hdr[32];
    uint8_t            img[64];
    uint32_t           crc;
    uint32_t           i;

    memset(&bc, 0, sizeof(bc));
    ST_CHECK(et_bootctl_init(&bc, cfg));
    et_bootctl_state(&bc, &st);
    ST_CHECK((st.staged_slot < 0) && (st.confirmed_slot < 0));

    /* 合成 64B 镜像 + 32B 头(与 demo SIMUPGRADE 同构造): 'ETBI' */
    for (i = 0u; i < sizeof(img); i++) {
        img[i] = (uint8_t)(0xC0u + (uint8_t)i);
    }
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 0x45u; hdr[1] = 0x54u; hdr[2] = 0x42u; hdr[3] = 0x49u;
    hdr[4] = (uint8_t)ET_BOOT_HDR_VER;
    hdr[6] = (uint8_t)ET_BOOT_HDR_SIZE;
    hdr[8] = (uint8_t)sizeof(img);
    crc = et_crc32_update(ET_CRC32_INIT, img, sizeof(img)) ^ ET_CRC32_INIT;
    hdr[12] = (uint8_t)crc; hdr[13] = (uint8_t)(crc >> 8);
    hdr[14] = (uint8_t)(crc >> 16); hdr[15] = (uint8_t)(crc >> 24);
    hdr[16] = 0x03u; hdr[17] = 0x00u;                   /* ver=3 (奇数) */
    crc = et_crc32(hdr, 28u);
    hdr[28] = (uint8_t)crc; hdr[29] = (uint8_t)(crc >> 8);
    hdr[30] = (uint8_t)(crc >> 16); hdr[31] = (uint8_t)(crc >> 24);

    /* 写 B 槽: 先擦后写 (头 8B 对齐 + 体 8B 对齐, 满足 G4 双字约束) */
    ST_CHECK(port_flash_erase_sector(cfg->slot_sector[1]));
    ST_CHECK(port_flash_write(cfg->slot_sector[1] * PORT_FLASH_SECTOR_SIZE,
                              hdr, sizeof(hdr)) == sizeof(hdr));
    ST_CHECK(port_flash_write(cfg->slot_sector[1] * PORT_FLASH_SECTOR_SIZE + 32u,
                              img, sizeof(img)) == sizeof(img));

    ST_CHECK(et_bootctl_verify_image(&bc, 1u));
    ST_CHECK(et_bootctl_stage(&bc, 1u));
    et_bootctl_state(&bc, &st);
    ST_CHECK(st.staged_slot == 1);

    ST_CHECK(et_bootctl_boot_attempt(&bc, 1u) == 1u);
    ST_CHECK(!et_bootctl_should_rollback(&bc, 1u));     /* 1 < max_attempts */
    ST_CHECK(et_bootctl_boot_attempt(&bc, 1u) == 2u);
    ST_CHECK(et_bootctl_should_rollback(&bc, 1u));      /* 达阈值 (≥2) */
    ST_CHECK(et_bootctl_confirm(&bc, 1u));
    et_bootctl_state(&bc, &st);
    ST_CHECK(st.confirmed_slot == 1);

    return (ctx->fails == 0);
}
#endif /* ET_MODULE_BOOTCTL */

/* ===================== 框架: 套件表 / 门控 / 运行器 ===================== */

typedef struct {
    const char *name;
    st_impl_fn  impl;
    uint8_t     needs;      /* 0=无 1=kv 布局 2=bootctl 配置 */
} st_entry_t;

static const st_entry_t g_suites[] = {
#if defined(ET_MODULE_RINGBUF) && ET_MODULE_RINGBUF
    { "ringbuf",   st_ringbuf,   0u },
#endif
#if defined(ET_MODULE_QUEUE) && ET_MODULE_QUEUE
    { "queue",     st_queue,     0u },
#endif
#if defined(ET_MODULE_MEMPOOL) && ET_MODULE_MEMPOOL
    { "mempool",   st_mempool,   0u },
#endif
#if defined(ET_MODULE_LIST) && ET_MODULE_LIST
    { "list",      st_list,      0u },
#endif
#if defined(ET_MODULE_FILTER) && ET_MODULE_FILTER
    { "filter",    st_filter,    0u },
#endif
#if defined(ET_MODULE_FSM) && ET_MODULE_FSM
    { "fsm",       st_fsm,       0u },
#endif
#if defined(ET_MODULE_SCHED) && ET_MODULE_SCHED
    { "sched",     st_sched,     0u },
#endif
#if defined(ET_MODULE_EVENT) && ET_MODULE_EVENT
    { "event",     st_event,     0u },
#endif
#if defined(ET_MODULE_STIMER) && ET_MODULE_STIMER
    { "stimer",    st_stimer,    0u },
#endif
#if defined(ET_MODULE_CRC) && ET_MODULE_CRC
    { "crc",       st_crc,       0u },
#endif
#if defined(ET_MODULE_FRAME) && ET_MODULE_FRAME
    { "frame",     st_frame,     0u },
#endif
#if defined(ET_MODULE_SOFTCLOCK) && ET_MODULE_SOFTCLOCK
    { "softclock", st_softclock, 0u },
#endif
#if defined(ET_MODULE_WDT) && ET_MODULE_WDT
    { "wdt",       st_wdt,       0u },
#endif
#if defined(ET_MODULE_ATCMD) && ET_MODULE_ATCMD && defined(ET_MODULE_SHELL) && ET_MODULE_SHELL
    { "atcmd",     st_atcmd_shell, 0u },
#endif
#if defined(ET_MODULE_XMODEM) && ET_MODULE_XMODEM
    { "xmodem",    st_xmodem,    0u },
#endif
#if defined(ET_MODULE_KV) && ET_MODULE_KV
    { "kv",        st_kv,        1u },
#endif
#if defined(ET_MODULE_BOOTCTL) && ET_MODULE_BOOTCTL
    { "bootctl",   st_bootctl,   2u },
#endif
};

#define ST_BUILTIN_N        (sizeof(g_suites) / sizeof(g_suites[0]))

static st_extra_t g_extras[ET_SELFTEST_MAX_EXTRA];

bool et_selftest_register(const char *name, et_selftest_suite_fn fn)
{
    uint32_t i;

    if ((name == NULL) || (fn == NULL)) {
        return false;
    }
    for (i = 0u; i < ET_SELFTEST_MAX_EXTRA; i++) {
        if (g_extras[i].used && (strcmp(g_extras[i].name, name) == 0)) {
            return false;                   /* 重名 */
        }
    }
    for (i = 0u; i < ET_SELFTEST_MAX_EXTRA; i++) {
        if (!g_extras[i].used) {
            g_extras[i].name = name;
            g_extras[i].fn   = fn;
            g_extras[i].used = true;
            return true;
        }
    }
    return false;                           /* 满 */
}

static bool st_run_one(const char *name, st_impl_fn impl, uint8_t needs,
                       et_selftest_report_fn report, void *user)
{
    st_ctx_t ctx;
    bool     ok;

    if ((needs == 1u) &&
#if ET_MODULE_KV
        (g_st_kv == NULL)
#else
        true
#endif
        ) {
        if (report != NULL) {
            report(user, ET_SELFTEST_SUITE_SKIP, name, 0u);
        }
        return true;                        /* SKIP 视为通过(不计失败) */
    }
    if ((needs == 2u) &&
#if ET_MODULE_BOOTCTL
        (g_st_bc == NULL)
#else
        true
#endif
        ) {
        if (report != NULL) {
            report(user, ET_SELFTEST_SUITE_SKIP, name, 0u);
        }
        return true;
    }

    ctx.report = report;
    ctx.user   = user;
    ctx.name   = name;
    ctx.fails  = 0;
    ok = impl(&ctx);
    if (report != NULL) {
        report(user, ok ? ET_SELFTEST_SUITE_PASS : ET_SELFTEST_SUITE_FAIL,
               name, (uint32_t)ctx.fails);
    }
    return ok;
}

bool et_selftest_run_all(et_selftest_report_fn report, void *user)
{
    uint32_t total = (uint32_t)ST_BUILTIN_N;
    uint32_t pass  = 0u;
    bool     all   = true;
    uint32_t i;

    for (i = 0u; i < ET_SELFTEST_MAX_EXTRA; i++) {
        if (g_extras[i].used) {
            total++;
        }
    }
    if (report != NULL) {
        report(user, ET_SELFTEST_BEGIN, "selftest", total);
    }

    for (i = 0u; i < (uint32_t)ST_BUILTIN_N; i++) {
        if (st_run_one(g_suites[i].name, g_suites[i].impl,
                       g_suites[i].needs, report, user)) {
            pass++;
        } else {
            all = false;
        }
    }
    for (i = 0u; i < ET_SELFTEST_MAX_EXTRA; i++) {
        if (g_extras[i].used) {
            bool ok = g_extras[i].fn(report, user);

            if (report != NULL) {       /* 与内建套件一致: 运行器统一上报 */
                report(user, ok ? ET_SELFTEST_SUITE_PASS : ET_SELFTEST_SUITE_FAIL,
                       g_extras[i].name, 0u);
            }
            if (ok) {
                pass++;
            } else {
                all = false;
            }
        }
    }
    if (report != NULL) {
        report(user, ET_SELFTEST_DONE, "selftest", pass);
    }
    return all;
}

bool et_selftest_run_suite(const char *name, et_selftest_report_fn report,
                           void *user)
{
    uint32_t i;

    if (name == NULL) {
        return false;
    }
    for (i = 0u; i < (uint32_t)ST_BUILTIN_N; i++) {
        if (strcmp(g_suites[i].name, name) == 0) {
            return st_run_one(g_suites[i].name, g_suites[i].impl,
                              g_suites[i].needs, report, user);
        }
    }
    for (i = 0u; i < ET_SELFTEST_MAX_EXTRA; i++) {
        if (g_extras[i].used && (strcmp(g_extras[i].name, name) == 0)) {
            bool ok = g_extras[i].fn(report, user);

            if (report != NULL) {
                report(user, ok ? ET_SELFTEST_SUITE_PASS : ET_SELFTEST_SUITE_FAIL,
                       g_extras[i].name, 0u);
            }
            return ok;
        }
    }
    return false;                           /* 未知套件 */
}

uint16_t et_selftest_suite_count(void)
{
    uint32_t n = (uint32_t)ST_BUILTIN_N;
    uint32_t i;

    for (i = 0u; i < ET_SELFTEST_MAX_EXTRA; i++) {
        if (g_extras[i].used) {
            n++;
        }
    }
    return (uint16_t)n;
}

#endif /* ET_MODULE_SELFTEST */



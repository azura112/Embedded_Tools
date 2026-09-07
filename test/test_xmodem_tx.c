/**
 * @file    test_xmodem_tx.c
 * @brief   et_xmodem_tx 单元测试 (对端行为矩阵 + tx→rx 内存回环)
 *
 * 线路模拟器: tx.putc 发出的字节直接喂库内接收器 et_xmodem_rx,
 * 接收器应答 (ACK/NAK/CAN) 回灌 tx.poll —— 纯内存端到端, 不依赖串口。
 * 接收侧 DONE 以线上 ACK 收尾 (与 demo xmodem_reply 集成约定一致)。
 */
#include <string.h>
#include "et_test.h"
#include "et_xmodem.h"
#include "et_xmodem_tx.h"
#include "et_config.h"

#define SRC_N       512u
#define RX_CAP      (132u + 1024u)

static et_xmodem_tx_t    g_tx;
static et_xmodem_tx_cfg_t g_cfg;
static et_xmodem_t       g_rx;
static uint8_t           g_rx_buf[RX_CAP];

static uint8_t  g_src[2048u];           /* 覆盖 1K 双块用例的源规模 */
static uint32_t g_src_n;                /* 有效源数据量 */
static uint32_t g_src_fail_at;          /* ≥ 此偏移 src 返回 0 (0=不模拟) */

static uint8_t  g_got[4096u];
static uint32_t g_got_n;
static uint8_t  g_reply[16];            /* rx → tx 应答字节队列 */
static uint32_t g_reply_n;
static uint32_t g_rx_now;
static uint32_t g_tx_now;
static bool     g_rx_done;

static bool sink_rec(void *user, uint32_t off, const uint8_t *d, uint32_t len)
{
    (void)user;
    if ((off != g_got_n) || ((off + len) > sizeof(g_got))) {
        return false;
    }
    memcpy(g_got + off, d, len);
    g_got_n += len;
    return true;
}

static void wire_putc(void *user, uint8_t b)
{
    et_xm_act_t a;

    (void)user;
    a = et_xmodem_rx(&g_rx, b, g_rx_now++);
    if (a == ET_XM_ACK) {
        if (g_reply_n < sizeof(g_reply)) {
            g_reply[g_reply_n++] = ET_XM_ACK_BYTE;
        }
    } else if (a == ET_XM_NAK) {
        if (g_reply_n < sizeof(g_reply)) {
            g_reply[g_reply_n++] = ET_XM_NAK_BYTE;
        }
    } else if (a == ET_XM_CAN) {
        g_reply[g_reply_n++] = ET_XM_CAN_BYTE;
    } else if (a == ET_XM_DONE) {
        g_rx_done = true;               /* 接收侧以 ACK 收尾 (同 demo 约定) */
        g_reply[g_reply_n++] = ET_XM_ACK_BYTE;
    }
}

static uint32_t wire_src(void *user, uint32_t off, uint8_t *dst, uint32_t want)
{
    uint32_t i;

    (void)user;
    if (g_src_fail_at != 0u) {
        if (off >= g_src_fail_at) {
            return 0u;
        }
        if ((off + want) > g_src_fail_at) {
            want = g_src_fail_at - off;
        }
    }
    for (i = 0u; i < want; i++) {
        dst[i] = ((off + i) < g_src_n) ? g_src[off + i] : ET_XM_PAD_BYTE;
    }
    return want;
}

/* rx/tx 配对初始化 (每个会话型用例开头调用) */
static void tx_arm(uint32_t total_n, uint32_t blk_size, uint32_t retry)
{
    uint32_t i;

    memset(&g_rx, 0, sizeof(g_rx));
    et_xmodem_rx_init(&g_rx, g_rx_buf, sizeof(g_rx_buf), sink_rec, NULL);
    memset(&g_tx, 0, sizeof(g_tx));
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.putc = wire_putc;
    g_cfg.src = wire_src;
    g_cfg.user = NULL;
    g_cfg.total = total_n;
    g_cfg.block_size = blk_size;
    g_cfg.retry_max = retry;
    g_cfg.ack_timeout_ms = 1000u;
    ET_CHECK(et_xmodem_tx_init(&g_tx, &g_cfg));

    /* 源数据填充避开 0x01/0x02 (帧观测不被载荷噪声污染) */
    for (i = 0u; i < SRC_N; i++) {
        g_src[i] = (uint8_t)(i * 7u + 3u);
    }
    g_src_n = (total_n < SRC_N) ? total_n : SRC_N;
    g_src_fail_at = 0u;
    g_got_n = 0u;
    g_reply_n = 0u;
    g_rx_now = 0u;
    g_tx_now = 0u;
    g_rx_done = false;
}

/* tx 推进: 应答队列优先, 否则 tick (100ms 步进); 返回最后动作 */
static et_xm_act_t drive(uint32_t max_steps)
{
    et_xm_act_t act = ET_XM_IDLE;
    uint32_t    steps = 0u;

    while (!et_xmodem_tx_done(&g_tx) && !et_xmodem_tx_aborted(&g_tx) &&
           (steps < max_steps)) {
        if (g_reply_n > 0u) {
            uint8_t b = g_reply[0];

            memmove(g_reply, g_reply + 1u, g_reply_n - 1u);
            g_reply_n--;
            act = et_xmodem_tx_poll(&g_tx, b, g_tx_now++);
        } else {
            g_tx_now += 100u;
            act = et_xmodem_tx_tick(&g_tx, g_tx_now);
        }
        steps++;
    }
    return act;
}

/* ---- 用例 ---- */

static void tx_init_rejects_bad(void)
{
    tx_arm(128u, 128u, 10u);                /* 基准配置 */
    ET_CHECK(!et_xmodem_tx_init(&g_tx, NULL));
    ET_CHECK(!et_xmodem_tx_init(NULL, &g_cfg));
    g_cfg.putc = NULL;
    ET_CHECK(!et_xmodem_tx_init(&g_tx, &g_cfg));
    g_cfg.putc = wire_putc;
    g_cfg.src = NULL;
    ET_CHECK(!et_xmodem_tx_init(&g_tx, &g_cfg));
    g_cfg.src = wire_src;
    g_cfg.total = 0u;
    ET_CHECK(!et_xmodem_tx_init(&g_tx, &g_cfg));
    g_cfg.total = 128u;
    g_cfg.retry_max = 0u;
    ET_CHECK(!et_xmodem_tx_init(&g_tx, &g_cfg));
    g_cfg.retry_max = 10u;
    g_cfg.ack_timeout_ms = 0u;
    ET_CHECK(!et_xmodem_tx_init(&g_tx, &g_cfg));
    g_cfg.ack_timeout_ms = 1000u;
    g_cfg.block_size = 256u;
    ET_CHECK(!et_xmodem_tx_init(&g_tx, &g_cfg));            /* 非法块型 */
#if !ET_XM_1K
    g_cfg.block_size = 1024u;
    ET_CHECK(!et_xmodem_tx_init(&g_tx, &g_cfg));            /* 1K 未使能 */
#endif
}

static void tx_start_on_nak(void)
{
    tx_arm(300u, 128u, 10u);
    ET_CHECK(et_xmodem_tx_tick(&g_tx, 100u) == ET_XM_IDLE); /* 起步前无动作 */
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 110u) == ET_XM_NAK);
    ET_CHECK_U32_EQ(1u, g_tx.blk);                        /* 块 1 已发 */
}

static void tx_start_on_c(void)
{
    tx_arm(300u, 128u, 10u);
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_CRC_CH_BYTE, 5u) == ET_XM_NAK);
    ET_CHECK_U32_EQ(1u, g_tx.blk);
}

static void tx_ack_advances(void)
{
    tx_arm(300u, 128u, 10u);
    (void)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 110u);
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_ACK_BYTE, 120u) == ET_XM_ACK);
    ET_CHECK_U32_EQ(2u, g_tx.blk);                        /* 块 2 发出 */
}

static void tx_nak_resends_same_block(void)
{
    tx_arm(300u, 128u, 10u);
    (void)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 110u);
    ET_CHECK_U32_EQ(1u, g_tx.blk);
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 120u) == ET_XM_NAK);
    ET_CHECK_U32_EQ(1u, g_tx.blk);                        /* 块号不变 = 重发 */
}

static void tx_timeout_resends(void)
{
    tx_arm(300u, 128u, 10u);
    (void)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 110u);
    ET_CHECK_U32_EQ(1u, g_tx.blk);
    g_tx_now = 110u;
    ET_CHECK(et_xmodem_tx_tick(&g_tx, g_tx_now + 500u) == ET_XM_IDLE);
    ET_CHECK_U32_EQ(1u, g_tx.blk);                        /* 未超时 */
    ET_CHECK(et_xmodem_tx_tick(&g_tx, g_tx_now + 1000u) == ET_XM_NAK);
    ET_CHECK_U32_EQ(1u, g_tx.blk);                        /* 超时重发同块 */
}

static void tx_retry_max_aborts(void)
{
    tx_arm(300u, 128u, 2u);
    ET_CHECK_U32_EQ(1u, (uint32_t)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 1u));
    ET_CHECK_U32_EQ(1u, (uint32_t)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 2u));
    ET_CHECK_U32_EQ(5u, (uint32_t)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 3u));
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_ACK_BYTE, 4u) == ET_XM_ERR);
    ET_CHECK(et_xmodem_tx_aborted(&g_tx));
}

static void tx_peer_can_aborts(void)
{
    tx_arm(300u, 128u, 10u);
    (void)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 110u);
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_CAN_BYTE, 120u) == ET_XM_CAN);
    ET_CHECK(et_xmodem_tx_aborted(&g_tx));
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_ACK_BYTE, 121u) == ET_XM_ERR);
}

static void tx_eot_negotiation(void)
{
    tx_arm(128u, 128u, 10u);                    /* 单块 */
    (void)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 1u);
    g_reply_n = 0u;
    g_rx_done = false;

    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_ACK_BYTE, 2u) == ET_XM_ACK);
    /* 最后块确认后 EOT#1 已发, 等 NAK */
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 3u) == ET_XM_NAK);
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_ACK_BYTE, 4u) == ET_XM_DONE);
    ET_CHECK(et_xmodem_tx_done(&g_tx));
    ET_CHECK(g_rx_done);                        /* rx 侧同时 DONE */
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_ACK_BYTE, 5u) == ET_XM_DONE);
}

static void tx_dirty_bytes_ignored(void)
{
    tx_arm(300u, 128u, 10u);
    (void)et_xmodem_tx_poll(&g_tx, ET_XM_NAK_BYTE, 110u);
    ET_CHECK_U32_EQ(1u, g_tx.blk);
    ET_CHECK(et_xmodem_tx_poll(&g_tx, 0x00u, 120u) == ET_XM_IDLE);
    ET_CHECK(et_xmodem_tx_poll(&g_tx, 0xFFu, 121u) == ET_XM_IDLE);
    ET_CHECK_U32_EQ(1u, g_tx.blk);            /* 会话不受扰 */
    ET_CHECK(et_xmodem_tx_poll(&g_tx, ET_XM_ACK_BYTE, 122u) == ET_XM_ACK);
    ET_CHECK_U32_EQ(2u, g_tx.blk);
}

static void tx_done_sticky(void)
{
    tx_eot_negotiation();
    ET_CHECK(et_xmodem_tx_tick(&g_tx, 9999u) == ET_XM_DONE);
}

static void tx_loopback_128(void)
{
    tx_arm(300u, 128u, 10u);
    g_reply[g_reply_n++] = ET_XM_NAK_BYTE;      /* 起步: 模拟 rx 催块 */

    ET_CHECK(drive(100u) == ET_XM_DONE);
    ET_CHECK(g_rx_done);
    ET_CHECK_U32_EQ(384u, g_got_n);             /* 3 块 (尾块含填充) */
    ET_CHECK(memcmp(g_got, g_src, 300u) == 0);  /* 载荷逐字节一致 */
}

static void tx_src_short_read_pads(void)
{
    tx_arm(128u, 128u, 10u);
    g_src_n = 10u;                              /* 数据尽 → 全 PAD */
    g_reply[g_reply_n++] = ET_XM_NAK_BYTE;
    ET_CHECK(drive(100u) == ET_XM_DONE);
    ET_CHECK_U32_EQ(128u, g_got_n);
    ET_CHECK(memcmp(g_got, g_src, 10u) == 0);
    {
        uint32_t i;

        for (i = 10u; i < 128u; i++) {
            ET_CHECK_U32_EQ(ET_XM_PAD_BYTE, g_got[i]);
        }
    }
}

#if ET_XM_1K
static void tx_loopback_1k(void)
{
    uint32_t i;

    tx_arm(2000u, 1024u, 10u);
    for (i = 512u; i < 2000u; i++) {            /* tx_arm 只填前 SRC_N */
        g_src[i] = (uint8_t)(i * 7u + 3u);
    }
    g_src_n = 2000u;
    g_reply[g_reply_n++] = ET_XM_NAK_BYTE;

    ET_CHECK(drive(100u) == ET_XM_DONE);
    ET_CHECK(g_rx_done);
    ET_CHECK_U32_EQ(2048u, g_got_n);            /* 2 块 1K */
    ET_CHECK(memcmp(g_got, g_src, 2000u) == 0);
}
#endif

const et_test_case_t *test_xmodem_tx_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"xmtx.init_rejects_bad",  tx_init_rejects_bad},
        {"xmtx.start_on_nak",      tx_start_on_nak},
        {"xmtx.start_on_c",        tx_start_on_c},
        {"xmtx.ack_advances",      tx_ack_advances},
        {"xmtx.nak_resends",       tx_nak_resends_same_block},
        {"xmtx.timeout_resends",   tx_timeout_resends},
        {"xmtx.retry_max_aborts",  tx_retry_max_aborts},
        {"xmtx.peer_can_aborts",   tx_peer_can_aborts},
        {"xmtx.eot_negotiation",   tx_eot_negotiation},
        {"xmtx.dirty_ignored",     tx_dirty_bytes_ignored},
        {"xmtx.done_sticky",       tx_done_sticky},
        {"xmtx.loopback_128",      tx_loopback_128},
        {"xmtx.src_short_pad",     tx_src_short_read_pads},
#if ET_XM_1K
        {"xmtx.loopback_1k",       tx_loopback_1k},
#endif
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

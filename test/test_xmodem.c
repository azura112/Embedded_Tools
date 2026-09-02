/**
 * @file    test_xmodem.c
 * @brief   et_xmodem 单元测试 —— 对端行为矩阵 (host 虚拟时间驱动, 无真实延时)
 *
 * 对端行为矩阵 (发送侧 test double 脚本化, 按状态机边覆盖):
 *   正常流 / CRC 坏块重传 / 补码错 / 重复块 / 序号回绕(255→0→1) /
 *   EOT 两段确认 / CAN×2 中止 / 单 CAN 抗噪 / 10s 静默(会话复位) /
 *   1s 催块节拍 / 1K 使能与否 × 缓冲容量 / sink 拒绝 / 失序致命 /
 *   脏字节抗噪 / 半块静默恢复 / 会话完成后重启
 */
#include <stddef.h>
#include <string.h>

#include "et_test.h"
#include "et_crc.h"
#include "et_xmodem.h"

static et_xmodem_t g_xm;
static uint8_t     g_buf[1100];

/* ---- sink 记录器 ---- */
static struct {
    int      cnt;
    uint32_t off[8];
    uint32_t len[8];
    uint8_t  first[8];
} g_sink;

static void sink_reset(void)
{
    memset(&g_sink, 0, sizeof(g_sink));
}

static bool sink_rec(void *user, uint32_t off, const uint8_t *data, uint32_t len)
{
    (void)user;
    if (g_sink.cnt < 8) {
        g_sink.off[g_sink.cnt]   = off;
        g_sink.len[g_sink.cnt]   = len;
        g_sink.first[g_sink.cnt] = (len > 0u) ? data[0] : 0u;
    }
    g_sink.cnt++;
    return true;
}

static bool sink_reject(void *user, uint32_t off, const uint8_t *data, uint32_t len)
{
    (void)user; (void)off; (void)data; (void)len;
    g_sink.cnt++;
    return false;                       /* 模拟缓冲满/写 flash 失败 */
}

/* ---- 发送侧 double: 逐字节喂入, 返回最后一个应答动作 ---- */

static et_xm_act_t feed_u8(et_xmodem_t *x, uint8_t b, uint32_t now)
{
    return et_xmodem_rx(x, b, now);
}

static et_xm_act_t feed_mem(et_xmodem_t *x, const uint8_t *p, uint32_t n, uint32_t now)
{
    et_xm_act_t a = ET_XM_IDLE;
    uint32_t    i;

    for (i = 0u; i < n; i++) {
        a = feed_u8(x, p[i], now);
    }
    return a;
}

/* 组一个标准块并喂入; corrupt_crc=1 时翻转 CRC 末字节 */
static et_xm_act_t send_block(et_xmodem_t *x, uint8_t blk,
                              const uint8_t *payload, uint16_t plen,
                              uint32_t now, int corrupt_crc)
{
    static uint8_t wire[1100];
    uint16_t crc = et_crc16_ccitt_update(0x0000u, payload, plen);
    uint8_t  head = (plen > ET_XM_BLK128) ? ET_XM_STX : ET_XM_SOH;
    uint32_t k = 0u;

    wire[k++] = head;
    wire[k++] = blk;
    wire[k++] = (uint8_t)~blk;
    memcpy(&wire[k], payload, plen);
    k += plen;
    wire[k++] = (uint8_t)(crc >> 8);
    wire[k++] = (uint8_t)(crc & 0xFFu);
    if (corrupt_crc) {
        wire[k - 1u] ^= 0xFFu;
    }
    return feed_mem(x, wire, k, now);
}

static void fill_payload(uint8_t *p, uint16_t n, uint8_t seed)
{
    uint16_t i;

    for (i = 0u; i < n; i++) {
        p[i] = (uint8_t)(seed + (uint8_t)i);
    }
}

static void xm_fresh(void)
{
    sink_reset();
    memset(&g_xm, 0, sizeof(g_xm));
    et_xmodem_rx_init(&g_xm, g_buf, sizeof(g_buf), sink_rec, NULL);
}

/* ---- 用例 ---- */

static void init_validation(void)
{
    uint8_t  small[100];
    et_xmodem_t bad;

    sink_reset();
    memset(&bad, 0, sizeof(bad));
    et_xmodem_rx_init(&bad, small, sizeof(small), sink_rec, NULL);
    ET_CHECK_U32_EQ(ET_XM_ERR, et_xmodem_rx(&bad, ET_XM_SOH, 0u));  /* 容量不足 → 终态 */
    ET_CHECK_U32_EQ(ET_XM_ERR, et_xmodem_rx_tick(&bad, 0u));

    memset(&bad, 0, sizeof(bad));
    et_xmodem_rx_init(&bad, NULL, 1000u, sink_rec, NULL);
    ET_CHECK_U32_EQ(ET_XM_ERR, et_xmodem_rx(&bad, ET_XM_SOH, 0u));  /* 空缓冲 → 终态 */

    xm_fresh();
    ET_CHECK_U32_EQ(ET_XM_IDLE, et_xmodem_rx_tick(&g_xm, 500u));    /* 正常实例待机 */
}

static void prompt_nak_cadence(void)
{
    xm_fresh();
    ET_CHECK_U32_EQ(ET_XM_IDLE, et_xmodem_rx_tick(&g_xm, 500u));    /* 1s 未到 */
    ET_CHECK_U32_EQ(ET_XM_NAK,  et_xmodem_rx_tick(&g_xm, 1000u));   /* 1s 催块 */
    ET_CHECK_U32_EQ(ET_XM_IDLE, et_xmodem_rx_tick(&g_xm, 1500u));   /* 周期内 */
    ET_CHECK_U32_EQ(ET_XM_NAK,  et_xmodem_rx_tick(&g_xm, 2000u));   /* 再催 */
}

static void normal_flow_128(void)
{
    static uint8_t p[ET_XM_BLK128];
    uint8_t blk;

    xm_fresh();
    (void)et_xmodem_rx_tick(&g_xm, 1000u);
    for (blk = 1u; blk <= 3u; blk++) {
        fill_payload(p, sizeof(p), (uint8_t)(blk * 11u));
        ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, blk, p, sizeof(p), 2000u, 0));
    }
    ET_CHECK_U32_EQ(3, g_sink.cnt);
    ET_CHECK_U32_EQ(0u,   g_sink.off[0]);
    ET_CHECK_U32_EQ(128u, g_sink.off[1]);
    ET_CHECK_U32_EQ(256u, g_sink.off[2]);
    ET_CHECK_U32_EQ((uint8_t)(1u * 11u), g_sink.first[0]);
    ET_CHECK_U32_EQ(384u, g_xm.total);
}

static void crc_corrupt_nak_resend(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    fill_payload(p, sizeof(p), 7u);
    ET_CHECK_U32_EQ(ET_XM_NAK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 1)); /* CRC 坏 */
    ET_CHECK_U32_EQ(0, g_sink.cnt);
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0)); /* 原块重发 */
    ET_CHECK_U32_EQ(1, g_sink.cnt);
    ET_CHECK_U32_EQ(7u, g_sink.first[0]);
}

static void complement_mismatch_nak(void)
{
    static uint8_t p[ET_XM_BLK128];
    static uint8_t wire[5u + ET_XM_BLK128];     /* head+块号对+载荷+CRC16 */

    xm_fresh();
    fill_payload(p, sizeof(p), 1u);
    wire[0] = ET_XM_SOH;
    wire[1] = 1u;
    wire[2] = 0x02u;                    /* 补码错: 正确应为 ~1 = 0xFE */
    {
        uint16_t crc = et_crc16_ccitt_update(0x0000u, p, sizeof(p));
        memcpy(&wire[3], p, sizeof(p));
        wire[3u + sizeof(p)]     = (uint8_t)(crc >> 8);
        wire[3u + sizeof(p) + 1] = (uint8_t)crc;
    }
    ET_CHECK_U32_EQ(ET_XM_NAK, feed_mem(&g_xm, wire, sizeof(wire), 0u));
    /* 修好后重发 → ACK */
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0));
}

static void duplicate_block_ack(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    fill_payload(p, sizeof(p), 3u);
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0));
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0)); /* ACK 丢失重发 */
    ET_CHECK_U32_EQ(1, g_sink.cnt);     /* 不重复 sink */
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 2u, p, sizeof(p), 0u, 0));
    ET_CHECK_U32_EQ(2, g_sink.cnt);
    ET_CHECK_U32_EQ(256u, g_xm.total);
}

static void block_num_wraparound(void)
{
    static uint8_t p[ET_XM_BLK128];
    uint16_t i;

    xm_fresh();
    fill_payload(p, sizeof(p), 0u);
    for (i = 1u; i <= 255u; i++) {      /* 1..255 走满一个周期 */
        ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, (uint8_t)i, p, sizeof(p), 0u, 0));
    }
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 0u, p, sizeof(p), 0u, 0)); /* 回绕到 0 */
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0)); /* 0→1 */
    ET_CHECK_U32_EQ(257, g_sink.cnt);
    ET_CHECK_U32_EQ(257u * 128u, g_xm.total);
}

static void eot_two_phase(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    fill_payload(p, sizeof(p), 5u);
    (void)send_block(&g_xm, 1u, p, sizeof(p), 0u, 0);
    ET_CHECK_U32_EQ(ET_XM_NAK,  feed_u8(&g_xm, ET_XM_EOT, 0u));     /* 一段: NAK */
    ET_CHECK_U32_EQ(ET_XM_DONE, feed_u8(&g_xm, ET_XM_EOT, 0u));     /* 二段: ACK+DONE */
    ET_CHECK_U32_EQ(1, g_sink.cnt);

    /* 会话自动复位: 可立即开始下一轮 */
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0));
    ET_CHECK_U32_EQ(2, g_sink.cnt);
    ET_CHECK_U32_EQ(128u, g_xm.total);  /* total 已随新会话从 0 重计 */
}

static void can_x2_abort(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    fill_payload(p, sizeof(p), 9u);
    ET_CHECK_U32_EQ(ET_XM_IDLE, feed_u8(&g_xm, ET_XM_CAN_BYTE, 0u));
    ET_CHECK_U32_EQ(ET_XM_CAN,  feed_u8(&g_xm, ET_XM_CAN_BYTE, 0u));     /* CAN×2 立即中止 */
    ET_CHECK_U32_EQ(ET_XM_ERR,  feed_u8(&g_xm, ET_XM_SOH, 0u));     /* 终态 */
    ET_CHECK_U32_EQ(ET_XM_ERR,  et_xmodem_rx_tick(&g_xm, 999999u));
}

static void single_can_ignored(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    fill_payload(p, sizeof(p), 2u);
    ET_CHECK_U32_EQ(ET_XM_IDLE, feed_u8(&g_xm, ET_XM_CAN_BYTE, 0u));     /* 单 CAN: 抗噪 */
    ET_CHECK_U32_EQ(ET_XM_ACK,  send_block(&g_xm, 1u, p, sizeof(p), 0u, 0));
}

static void silence_10s_err_rearm(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    ET_CHECK_U32_EQ(ET_XM_NAK,  et_xmodem_rx_tick(&g_xm, 9999u));   /* 先触发了 1s 催块 */
    ET_CHECK_U32_EQ(ET_XM_ERR,  et_xmodem_rx_tick(&g_xm, 10000u));  /* 10s 静默放弃(静默判定优先) */
    ET_CHECK_U32_EQ(ET_XM_IDLE, et_xmodem_rx_tick(&g_xm, 10001u));  /* 会话自动复位 */
    ET_CHECK_U32_EQ(ET_XM_NAK,  et_xmodem_rx_tick(&g_xm, 11001u));  /* 新周期催块 */
    fill_payload(p, sizeof(p), 4u);
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 11002u, 0));
    ET_CHECK_U32_EQ(128u, g_xm.total);  /* total 随会话复位重新累计 */
}

static void dirty_noise_ignored(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    fill_payload(p, sizeof(p), 6u);
    ET_CHECK_U32_EQ(ET_XM_IDLE, feed_u8(&g_xm, 0x00u, 0u));
    ET_CHECK_U32_EQ(ET_XM_IDLE, feed_u8(&g_xm, 'A', 0u));
    ET_CHECK_U32_EQ(ET_XM_IDLE, feed_u8(&g_xm, 0xFFu, 0u));
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0));
}

static void sequence_fatal_can(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    fill_payload(p, sizeof(p), 8u);
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0));
    ET_CHECK_U32_EQ(ET_XM_CAN, send_block(&g_xm, 3u, p, sizeof(p), 0u, 0)); /* 跳块: 致命 */
    ET_CHECK_U32_EQ(ET_XM_ERR, et_xmodem_rx_tick(&g_xm, 1u));
}

static void sink_reject_abort(void)
{
    static uint8_t p[ET_XM_BLK128];

    sink_reset();
    memset(&g_xm, 0, sizeof(g_xm));
    et_xmodem_rx_init(&g_xm, g_buf, sizeof(g_buf), sink_reject, NULL);
    fill_payload(p, sizeof(p), 1u);
    ET_CHECK_U32_EQ(ET_XM_CAN, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0));
    ET_CHECK_U32_EQ(1, g_sink.cnt);
    ET_CHECK_U32_EQ(ET_XM_ERR, et_xmodem_rx_tick(&g_xm, 1u));       /* 中止后恒 ERR */
}

static void mid_block_silence_recover(void)
{
    static uint8_t p[ET_XM_BLK128];

    xm_fresh();
    fill_payload(p, sizeof(p), 5u);
    ET_CHECK_U32_EQ(ET_XM_IDLE, feed_u8(&g_xm, ET_XM_SOH, 0u));
    ET_CHECK_U32_EQ(ET_XM_IDLE, feed_u8(&g_xm, 1u, 0u));            /* 半块中断 */
    ET_CHECK_U32_EQ(ET_XM_ERR,  et_xmodem_rx_tick(&g_xm, 10000u));  /* 半块 + 10s 静默 */
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 10001u, 0)); /* 会话已复位 */
}

#if ET_XM_1K
static void stx_1k_mode(void)
{
    static uint8_t p[1024];

    xm_fresh();
    fill_payload(p, sizeof(p), 0xABu);
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0));
    ET_CHECK_U32_EQ(1, g_sink.cnt);
    ET_CHECK_U32_EQ(1024u, g_sink.len[0]);
    ET_CHECK_U32_EQ(0xABu, g_sink.first[0]);
    ET_CHECK_U32_EQ(1024u, g_xm.total);

    /* 1K 块 CRC 坏 → NAK 重发 */
    ET_CHECK_U32_EQ(ET_XM_NAK, send_block(&g_xm, 2u, p, sizeof(p), 0u, 1));
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 2u, p, sizeof(p), 0u, 0));
}
#else
static void stx_disabled_nak(void)
{
    static uint8_t p[ET_XM_BLK128];
    static uint8_t wire[4u + 1024u];

    xm_fresh();
    memset(wire, 0x55, sizeof(wire));
    wire[0] = ET_XM_STX;
    wire[1] = 1u;
    wire[2] = 0xFEu;
    /* 1K 未使能: 首字节 STX 即 NAK */
    ET_CHECK_U32_EQ(ET_XM_NAK, feed_u8(&g_xm, ET_XM_STX, 0u));
    ET_CHECK_U32_EQ(0, g_sink.cnt);
    fill_payload(p, sizeof(p), 1u);
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0)); /* SOH 不受影响 */
}
#endif

/* 1K 使能但容量只有基本块: STX → NAK, SOH 正常 (两变体都可跑) */
static void stx_cap_too_small(void)
{
#if !ET_XM_1K
    static uint8_t p[ET_XM_BLK128];
#endif
    uint8_t small[132];
    et_xmodem_t xm;

    sink_reset();
    memset(&xm, 0, sizeof(xm));
    et_xmodem_rx_init(&xm, small, sizeof(small), sink_rec, NULL);
#if ET_XM_1K
    /* 1K 使能时容量契约 = >=1028: cap=132 的实例 init 即终态(见头注) */
    ET_CHECK_U32_EQ(ET_XM_ERR, feed_u8(&xm, ET_XM_STX, 0u));
    ET_CHECK_U32_EQ(ET_XM_ERR, feed_u8(&xm, ET_XM_SOH, 0u));
    ET_CHECK_U32_EQ(ET_XM_ERR, et_xmodem_rx_tick(&xm, 0u));
#else
    memset(&g_xm, 0, sizeof(g_xm));
    et_xmodem_rx_init(&g_xm, g_buf, 132u, sink_rec, NULL);
    /* 1K 未使能: STX 直接 NAK(与容量无关) */
    ET_CHECK_U32_EQ(ET_XM_NAK, feed_u8(&g_xm, ET_XM_STX, 0u));
    fill_payload(p, sizeof(p), 1u);
    ET_CHECK_U32_EQ(ET_XM_ACK, send_block(&g_xm, 1u, p, sizeof(p), 0u, 0)); /* 128B 正常 */
#endif
}

static const et_test_case_t g_cases[] = {
    { "init.validation",        init_validation },
    { "tick.prompt_cadence",    prompt_nak_cadence },
    { "rx.normal_flow_128",     normal_flow_128 },
    { "rx.crc_nak_resend",      crc_corrupt_nak_resend },
    { "rx.complement_mismatch", complement_mismatch_nak },
    { "rx.duplicate_ack",       duplicate_block_ack },
    { "rx.block_num_wrap",      block_num_wraparound },
    { "rx.eot_two_phase",       eot_two_phase },
    { "rx.can_x2_abort",        can_x2_abort },
    { "rx.single_can_noise",    single_can_ignored },
    { "tick.silence_10s",       silence_10s_err_rearm },
    { "rx.dirty_noise",         dirty_noise_ignored },
    { "rx.sequence_fatal",      sequence_fatal_can },
    { "sink.reject_abort",      sink_reject_abort },
    { "rx.mid_block_silence",   mid_block_silence_recover },
#if ET_XM_1K
    { "rx.stx_1k_mode",         stx_1k_mode },
#else
    { "rx.stx_disabled",        stx_disabled_nak },
#endif
    { "rx.stx_cap_small",       stx_cap_too_small },
};

const et_test_case_t *test_xmodem_cases(size_t *count)
{
    *count = sizeof(g_cases) / sizeof(g_cases[0]);
    return g_cases;
}

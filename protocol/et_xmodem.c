/**
 * @file    et_xmodem.c
 * @brief   XMODEM-CRC 接收器实现 (状态机 + 流式 CRC16-CCITT)
 *
 * 状态机: WAIT(等 SOH/STX/EOT/CAN) / DATA(收块) / EOT1(等二段 EOT)。
 * 会话时序语义见 et_xmodem.h 头注; 测试矩阵见 test/test_xmodem.c 头注。
 */
#include "et_xmodem.h"
#include "et_crc.h"

#if ET_MODULE_XMODEM

/* 内部状态编码 */
#define XM_S_WAIT           0u
#define XM_S_DATA           1u
#define XM_S_EOT1           2u

static void session_reset(et_xmodem_t *x, uint32_t now)
{
    x->state    = XM_S_WAIT;
    x->pos      = 0u;
    x->expect   = 1u;
    x->last_blk = 0u;
    x->has_last = false;
    x->total    = 0u;
    x->can_cnt  = 0u;
    x->t_act    = now;
    x->t_prompt = now;
    x->has_act  = true;
}

void et_xmodem_rx_init(et_xmodem_t *x, uint8_t *buf, uint32_t cap,
                       et_xm_sink_fn sink, void *user)
{
    if (x == NULL) {
        return;
    }
    x->buf     = buf;
    x->cap     = cap;
    x->sink    = sink;
    x->user    = user;
    x->aborted = false;
    session_reset(x, 0u);

    /* 容量下限: 基本块 132 = 块号对 2 + 载荷 128 + CRC 2; 1K 需 1028 */
    if ((buf == NULL) || (sink == NULL) ||
#if ET_XM_1K
        (cap < (uint32_t)(ET_XM_HDR_RAW + 1024u))
#else
        (cap < (uint32_t)(ET_XM_HDR_RAW + ET_XM_BLK128))
#endif
    ) {
        x->aborted = true;              /* 配置非法: 终态, 见头注 */
    }
}

/* 收满一块: 校验并分派; 返回应答 */
static et_xm_act_t block_done(et_xmodem_t *x)
{
    uint8_t  blk  = x->buf[0];
    uint8_t  comp = x->buf[1];
    uint16_t plen = (uint16_t)(x->blk_len - ET_XM_HDR_RAW);
    uint16_t crc_rx = (uint16_t)((uint16_t)(x->buf[2u + plen] << 8) |
                                 (uint16_t)x->buf[3u + plen]);
    uint16_t crc = et_crc16_ccitt_update(0x0000u, &x->buf[2], plen);

    x->state = XM_S_WAIT;
    x->pos   = 0u;

    if (((uint8_t)~blk) != comp) {
        return ET_XM_NAK;               /* 块号补码错 */
    }
    if (crc != crc_rx) {
        return ET_XM_NAK;               /* CRC 坏: 原块重发 */
    }
    if (blk == x->expect) {
        if (x->sink != NULL) {
            if (!x->sink(x->user, x->total, &x->buf[2], plen)) {
                x->aborted = true;      /* sink 拒绝(缓冲满/写失败) */
                return ET_XM_CAN;
            }
        }
        x->last_blk = blk;
        x->has_last = true;
        x->expect   = (uint8_t)(x->expect + 1u);    /* 255→0, 0→1 回绕 */
        x->total   += plen;
        return ET_XM_ACK;
    }
    if (x->has_last && (blk == x->last_blk)) {
        return ET_XM_ACK;               /* ACK 丢失导致的重复块: 不重复 sink */
    }
    x->aborted = true;                  /* 失序: 致命 */
    return ET_XM_CAN;
}

et_xm_act_t et_xmodem_rx(et_xmodem_t *x, uint8_t ch, uint32_t now)
{
    if (x == NULL) {
        return ET_XM_ERR;
    }
    if (x->aborted) {
        return ET_XM_ERR;
    }
    x->t_act = now;                     /* 任何字节都是活动 */

    if (x->state == XM_S_DATA) {
        /* 块内一切字节皆数据(单个 CAN 亦然, 由块 CRC 兜底); 仅连续
         * CAN×2 才视为对端取消 */
        x->buf[x->pos++] = ch;
        if (ch == ET_XM_CAN_BYTE) {
            if (++x->can_cnt >= 2u) {
                x->aborted = true;
                return ET_XM_CAN;
            }
        } else {
            x->can_cnt = 0u;
        }
        if (x->pos < x->blk_len) {
            return ET_XM_IDLE;
        }
        x->can_cnt = 0u;                /* 块边界清零, 防跨块误连击 */
        return block_done(x);
    }

    /* ---- WAIT / EOT1 共用的起始字符处理 ---- */
    if (ch == ET_XM_CAN_BYTE) {
        if (++x->can_cnt >= 2u) {
            x->aborted = true;
            return ET_XM_CAN;
        }
        return ET_XM_IDLE;
    }
    x->can_cnt = 0u;

    if ((x->state == XM_S_EOT1) && (ch == ET_XM_EOT)) {
        session_reset(x, now);          /* 二段确认完成, 会话复位可再来 */
        return ET_XM_DONE;
    }
    if (x->state == XM_S_EOT1) {
        x->state = XM_S_WAIT;           /* 对端跳过二段确认继续发块: 按 WAIT 处理 */
    }

    switch (ch) {
    case ET_XM_SOH:
#if ET_XM_1K
    case ET_XM_STX:
#endif
        /* SOH=132 / STX=1028: 头 2 + 载荷 + CRC 2 */
        x->blk_len = (uint16_t)((ch == ET_XM_SOH) ? (ET_XM_BLK128 + ET_XM_HDR_RAW)
                                                  : (1024u + ET_XM_HDR_RAW));
        if ((uint32_t)x->blk_len > x->cap) {
            return ET_XM_NAK;           /* 缓冲装不下该块型 */
        }
        x->pos   = 0u;
        x->state = XM_S_DATA;
        return ET_XM_IDLE;

    case ET_XM_EOT:
        x->state = XM_S_EOT1;           /* 两段确认: 第一段回 NAK */
        return ET_XM_NAK;

#if !ET_XM_1K
    case ET_XM_STX:
        return ET_XM_NAK;               /* 1K 未使能 */
#endif

    default:
        return ET_XM_IDLE;              /* 脏字节抗噪: 忽略 */
    }
}

et_xm_act_t et_xmodem_rx_tick(et_xmodem_t *x, uint32_t now)
{
    if (x == NULL) {
        return ET_XM_ERR;
    }
    if (x->aborted) {
        return ET_XM_ERR;
    }
    if (x->has_act && ((now - x->t_act) >= ET_XM_SILENCE_MS)) {
        session_reset(x, now);          /* 静默放弃: 会话复位, 下一轮可重来 */
        return ET_XM_ERR;
    }
    if ((x->state == XM_S_WAIT) && ((now - x->t_prompt) >= ET_XM_PROMPT_MS)) {
        x->t_prompt = now;
        return ET_XM_NAK;               /* 催块(1s 无 SOH 重发 NAK) */
    }
    return ET_XM_IDLE;
}

#endif /* ET_MODULE_XMODEM */

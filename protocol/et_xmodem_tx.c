/**
 * @file    et_xmodem_tx.c
 * @brief   XMODEM-CRC 发送器实现 (与 et_xmodem.c 接收器对称)
 *
 * 状态机: WAIT_START(等 NAK/'C') → DATA(块流) → EOT1 → EOT2 → done。
 * 帧输出经 cfg->putc 逐字节; 载荷经 cfg->src 流式读取 (64B 分段), 尾块
 * 以 ET_XM_PAD_BYTE 补齐块型; CRC 用共享助手 et_xmodem_crc16(线上载荷,
 * 高字节在前) —— 与接收器同一实现, 防两侧漂移。
 */
#include "et_xmodem_tx.h"
#include "et_crc.h"

#if ET_MODULE_XMODEM

#include <string.h>

#define TX_ST_WAIT_START    0u
#define TX_ST_DATA          1u
#define TX_ST_EOT1          2u
#define TX_ST_EOT2          3u

#define TX_READ_CHUNK       64u     /* src 分段读取粒度 */

static void tx_send_block(et_xmodem_tx_t *x, uint32_t now)
{
    uint8_t  chunk[TX_READ_CHUNK];
    uint32_t remaining = x->block_size;
    uint32_t pos = x->sent;                 /* 块起始载荷偏移 */
    uint16_t crc = 0x0000u;                 /* 仅覆盖线上载荷 (与 rx 一致) */

    x->putc(x->user, (x->block_size == 1024u) ? ET_XM_STX : ET_XM_SOH);
    x->putc(x->user, x->blk);
    x->putc(x->user, (uint8_t)(~(uint8_t)x->blk));

    while (remaining > 0u) {
        uint32_t want = (remaining < TX_READ_CHUNK) ? remaining
                                                    : TX_READ_CHUNK;
        uint32_t got = x->src(x->user, pos, chunk, want);
        uint32_t k;

        for (k = got; k < want; k++) {      /* 短读/数据尽: PAD 补齐本段 */
            chunk[k] = ET_XM_PAD_BYTE;
        }
        crc = et_crc16_ccitt_update(crc, chunk, want);
        for (k = 0u; k < want; k++) {
            x->putc(x->user, chunk[k]);
        }
        pos += got;                         /* src 返回 0 时原位重询 */
        remaining -= want;                  /* 块长恒定, 填充语义不变 */
    }
    x->putc(x->user, (uint8_t)(crc >> 8));  /* CRC 高字节在前 */
    x->putc(x->user, (uint8_t)(crc & 0xFFu));

    x->state  = TX_ST_DATA;
    x->t_mark = now;
    /* retries 不在此清零: 重发路径也经本函数, 计数归 tx_on_ack 清 */
}

static et_xm_act_t tx_send_current(et_xmodem_tx_t *x, uint32_t now)
{
    if (x->retries >= x->retry_max) {
        x->aborted = true;
        return ET_XM_ERR;                   /* 重传超限 */
    }
    x->retries++;
    tx_send_block(x, now);
    return ET_XM_NAK;                       /* 语义: 重发/催发当前块 */
}

static et_xm_act_t tx_on_ack(et_xmodem_tx_t *x, uint32_t now)
{
    x->sent += x->block_size;               /* 确认一个块 (尾块按块型计) */
    if (x->sent >= x->total) {
        x->state  = TX_ST_EOT1;
        x->t_mark = now;
        x->retries = 0u;
        x->putc(x->user, ET_XM_EOT);        /* EOT#1 (预期 NAK) */
        return ET_XM_ACK;
    }
    x->blk = (x->blk == 255u) ? 1u : (uint8_t)(x->blk + 1u);
    tx_send_block(x, now);
    return ET_XM_ACK;
}

bool et_xmodem_tx_init(et_xmodem_tx_t *x, const et_xmodem_tx_cfg_t *cfg)
{
    if ((x == NULL) || (cfg == NULL) ||
        (cfg->putc == NULL) || (cfg->src == NULL) ||
        (cfg->total == 0u) ||
        (cfg->retry_max == 0u) || (cfg->ack_timeout_ms == 0u)) {
        return false;
    }
    if ((cfg->block_size != 128u) && (cfg->block_size != 1024u)) {
        return false;
    }
#if !ET_XM_1K
    if (cfg->block_size == 1024u) {
        return false;                       /* 接收器未使能 1K */
    }
#endif
    x->putc           = cfg->putc;
    x->src            = cfg->src;
    x->user           = cfg->user;
    x->total          = cfg->total;
    x->block_size     = cfg->block_size;
    x->retry_max      = cfg->retry_max;
    x->ack_timeout_ms = cfg->ack_timeout_ms;
    x->state          = TX_ST_WAIT_START;
    x->blk            = 1u;
    x->sent           = 0u;
    x->retries        = 0u;
    x->t_mark         = 0u;
    x->eot2           = false;
    x->done           = false;
    x->aborted        = false;
    return true;
}

et_xm_act_t et_xmodem_tx_poll(et_xmodem_tx_t *x, uint8_t ch, uint32_t now)
{
    if (x == NULL) {
        return ET_XM_ERR;
    }
    if (x->done) {
        return ET_XM_DONE;
    }
    if (x->aborted) {
        return ET_XM_ERR;
    }

    if (x->state == TX_ST_WAIT_START) {
        if ((ch == ET_XM_NAK_BYTE) || (ch == ET_XM_CRC_CH_BYTE)) {
            return tx_send_current(x, now); /* 首块: retries 0 → 发块 1 */
        }
        return ET_XM_IDLE;
    }

    if (ch == ET_XM_CAN_BYTE) {
        x->aborted = true;                  /* 单 CAN 即中止: 对端主动 */
        return ET_XM_CAN;
    }

    switch (x->state) {
    case TX_ST_DATA:
        if (ch == ET_XM_ACK_BYTE) {
            x->retries = 0u;
            return tx_on_ack(x, now);
        }
        if (ch == ET_XM_NAK_BYTE) {
            return tx_send_current(x, now); /* 重发当前块 */
        }
        return ET_XM_IDLE;

    case TX_ST_EOT1:
        if (ch == ET_XM_NAK_BYTE) {         /* EOT#1 被确认进入二段 */
            x->state  = TX_ST_EOT2;
            x->t_mark = now;
            x->retries = 0u;
            x->eot2    = true;
            x->putc(x->user, ET_XM_EOT);
            return ET_XM_NAK;
        }
        return ET_XM_IDLE;                  /* 其他字节: 继续等 NAK */

    case TX_ST_EOT2:
        if (ch == ET_XM_ACK_BYTE) {
            x->done = true;
            return ET_XM_DONE;
        }
        if (ch == ET_XM_NAK_BYTE) {         /* 防御: 重发 EOT#2 */
            x->t_mark = now;
            x->putc(x->user, ET_XM_EOT);
            return ET_XM_NAK;
        }
        return ET_XM_IDLE;

    default:
        x->aborted = true;
        return ET_XM_ERR;
    }
}

et_xm_act_t et_xmodem_tx_tick(et_xmodem_tx_t *x, uint32_t now)
{
    if (x == NULL) {
        return ET_XM_ERR;
    }
    if (x->done) {
        return ET_XM_DONE;
    }
    if (x->aborted) {
        return ET_XM_ERR;
    }
    if (x->state == TX_ST_WAIT_START) {
        return ET_XM_IDLE;                  /* 起步由对端 NAK/'C' 驱动 */
    }
    if ((uint32_t)(now - x->t_mark) < x->ack_timeout_ms) {
        return ET_XM_IDLE;                  /* 未超时 */
    }
    x->t_mark = now;                        /* 超时: 重发当前步骤 */
    return tx_send_current(x, now);
}

bool et_xmodem_tx_done(const et_xmodem_tx_t *x)
{
    return (x != NULL) && x->done;
}

bool et_xmodem_tx_aborted(const et_xmodem_tx_t *x)
{
    return (x != NULL) && x->aborted;
}

#endif /* ET_MODULE_XMODEM */

/**
 * @file    et_modbus.c
 * @brief   Modbus RTU 从站实现 (快路径长度域 + 静默路径 CRC 兜底)
 */
#include "et_modbus.h"
#include "et_crc.h"

#if ET_MODULE_MODBUS

#include <string.h>

/* 取 BE16(请求字段一律大端) */
static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/* 追加 CRC16-MODBUS(线上低字节在前), 返回帧总长 */
static uint32_t frame_seal(uint8_t *buf, uint32_t len)
{
    uint16_t crc = et_crc16_modbus(buf, len);

    buf[len]      = (uint8_t)(crc & 0xFFu);
    buf[len + 1u] = (uint8_t)(crc >> 8);
    return len + 2u;
}

/* 快路径: 已知功能码的期望帧长; 0 = 尚不可判定(收齐后再来/等静默) */
static uint32_t expected_len(const uint8_t *b, uint32_t n)
{
    if (n < 2u) {
        return 0u;
    }
    switch (b[1]) {
    case ET_MODBUS_FC_READ_HOLDING:
    case ET_MODBUS_FC_READ_INPUT:
    case ET_MODBUS_FC_WRITE_SINGLE:
        return 8u;
    case ET_MODBUS_FC_WRITE_MULTIPLE:
        return (n < 7u) ? 0u : ((uint32_t)b[6] + 9u);
    default:
        return 0u;              /* 未知功能码: 只能由静默路径界定 */
    }
}

/* 异常应答: [addr, func|0x80, exc, crc_lo, crc_hi] */
static void reply_exception(et_modbus_t *mb, uint8_t func, uint8_t exc)
{
    mb->tx[0] = mb->rx[0];
    mb->tx[1] = (uint8_t)(func | 0x80u);
    mb->tx[2] = exc;
    mb->resplen = frame_seal(mb->tx, 3u);
    mb->stats.responses++;
    mb->stats.exceptions++;
}

/* 读请求(0x03/0x04): 广播忽略; 成功时直接构造应答, 失败经 *exc 走异常应答 */
static void do_read(et_modbus_t *mb, uint8_t func, bool reply,
                    uint32_t len, uint8_t *exc)
{
    const uint8_t *r = mb->rx;
    uint16_t addr;
    uint16_t qty;

    if (len != 8u) {
        *exc = ET_MODBUS_EXC_ILLEGAL_VALUE;
        return;
    }
    if (!reply) {
        return;                             /* 广播读: 无应答无动作 */
    }
    addr = be16(r + 2u);
    qty  = be16(r + 4u);
    if ((qty == 0u) || (qty > ET_MODBUS_RD_QTY_MAX)) {
        *exc = ET_MODBUS_EXC_ILLEGAL_VALUE;
        return;
    }
    if ((5u + (2u * (uint32_t)qty)) > mb->txcap) {
        *exc = ET_MODBUS_EXC_ILLEGAL_VALUE; /* 应答放不下 → 值域错误 */
        return;
    }
    *exc = (mb->cfg.rd != NULL)
               ? mb->cfg.rd(mb->cfg.user, func, addr, qty, mb->tx + 3u)
               : ET_MODBUS_EXC_ILLEGAL_ADDR;
    if (*exc != 0u) {
        return;
    }
    mb->tx[0] = r[0];
    mb->tx[1] = func;
    mb->tx[2] = (uint8_t)(2u * (uint32_t)qty);
    mb->resplen = frame_seal(mb->tx, 3u + (2u * (uint32_t)qty));
    mb->stats.responses++;
}

/* 单寄存器写(0x06): 应答回显请求 */
static void do_write_single(et_modbus_t *mb, bool reply,
                            uint32_t len, uint8_t *exc)
{
    const uint8_t *r = mb->rx;

    if (len != 8u) {
        *exc = ET_MODBUS_EXC_ILLEGAL_VALUE;
        return;
    }
    *exc = (mb->cfg.wr != NULL)
               ? mb->cfg.wr(mb->cfg.user, ET_MODBUS_FC_WRITE_SINGLE,
                            be16(r + 2u), 1u, r + 4u)
               : ET_MODBUS_EXC_ILLEGAL_ADDR;
    if ((*exc == 0u) && reply) {
        memcpy(mb->tx, r, 8u);
        mb->resplen = 8u;
        mb->stats.responses++;
    }
}

/* 多寄存器写(0x10): 应答 = [addr, fc, start, qty, crc] */
static void do_write_multiple(et_modbus_t *mb, bool reply,
                              uint32_t len, uint8_t *exc)
{
    const uint8_t *r = mb->rx;
    uint16_t addr;
    uint16_t qty;
    uint8_t  bc;

    if (len < 9u) {
        *exc = ET_MODBUS_EXC_ILLEGAL_VALUE;
        return;
    }
    addr = be16(r + 2u);
    qty  = be16(r + 4u);
    bc   = r[6];
    if ((qty == 0u) || (qty > ET_MODBUS_WR_QTY_MAX) ||
        ((uint32_t)bc != (2u * (uint32_t)qty)) ||
        (len != (9u + (uint32_t)bc))) {
        *exc = ET_MODBUS_EXC_ILLEGAL_VALUE;
        return;
    }
    *exc = (mb->cfg.wr != NULL)
               ? mb->cfg.wr(mb->cfg.user, ET_MODBUS_FC_WRITE_MULTIPLE,
                            addr, qty, r + 7u)
               : ET_MODBUS_EXC_ILLEGAL_ADDR;
    if ((*exc == 0u) && reply) {
        mb->tx[0] = r[0];
        mb->tx[1] = ET_MODBUS_FC_WRITE_MULTIPLE;
        mb->tx[2] = r[2];
        mb->tx[3] = r[3];
        mb->tx[4] = r[4];
        mb->tx[5] = r[5];
        mb->resplen = frame_seal(mb->tx, 6u);
        mb->stats.responses++;
    }
}

/* 处理一帧(已通过 CRC 与地址判定); len = 帧长(含 CRC) */
static void execute(et_modbus_t *mb, uint32_t len)
{
    uint8_t func = mb->rx[1];
    bool    reply = (mb->rx[0] != 0u);      /* 广播(0): 不应答 */
    uint8_t exc = 0u;

    switch (func) {
    case ET_MODBUS_FC_READ_HOLDING:
    case ET_MODBUS_FC_READ_INPUT:
        do_read(mb, func, reply, len, &exc);
        break;
    case ET_MODBUS_FC_WRITE_SINGLE:
        do_write_single(mb, reply, len, &exc);
        break;
    case ET_MODBUS_FC_WRITE_MULTIPLE:
        do_write_multiple(mb, reply, len, &exc);
        break;
    default:
        if (!reply) {
            return;                         /* 广播非法功能: 静默 */
        }
        exc = ET_MODBUS_EXC_ILLEGAL_FUNC;
        break;
    }

    if ((exc != 0u) && reply) {
        reply_exception(mb, func, exc);
    }
}

/* CRC + 地址判定后转 execute */
static void process_frame(et_modbus_t *mb, uint32_t len)
{
    uint16_t crc_wire;
    uint16_t crc_calc;

    if (len < 4u) {
        mb->stats.discarded++;
        return;
    }
    /* 线上 CRC 低字节在前(MODBUS 规范) */
    crc_wire = (uint16_t)(((uint16_t)mb->rx[len - 1u] << 8) |
                          (uint16_t)mb->rx[len - 2u]);
    crc_calc = et_crc16_modbus(mb->rx, len - 2u);
    if (crc_wire != crc_calc) {
        mb->stats.crc_err++;
        return;
    }
    if ((mb->rx[0] != mb->cfg.slave_addr) && (mb->rx[0] != 0u)) {
        mb->stats.addr_mismatch++;
        return;
    }
    mb->stats.frames++;
    execute(mb, len);
}

bool et_modbus_init(et_modbus_t *mb, const et_modbus_cfg_t *cfg,
                    uint8_t *rxbuf, uint32_t rxcap,
                    uint8_t *txbuf, uint32_t txcap)
{
    ET_ASSERT(mb != NULL);
    ET_ASSERT(cfg != NULL);
    ET_ASSERT(rxbuf != NULL);
    ET_ASSERT(txbuf != NULL);
    if ((mb == NULL) || (cfg == NULL) || (rxbuf == NULL) || (txbuf == NULL)) {
        return false;
    }
    if ((cfg->slave_addr < 1u) || (cfg->slave_addr > 247u)) {
        return false;                       /* 0 保留给广播, 248~255 保留 */
    }
    if ((rxcap < 8u) || (txcap < 8u)) {
        return false;
    }
    mb->cfg       = *cfg;
    mb->rx        = rxbuf;
    mb->rxcap     = rxcap;
    mb->rxlen     = 0u;
    mb->tx        = txbuf;
    mb->txcap     = txcap;
    mb->resplen   = 0u;
    mb->last_ms   = 0u;
    mb->last_rxlen = 0u;
    mb->idle_ms   = 0u;
    memset(&mb->stats, 0, sizeof(mb->stats));
    return true;
}

uint32_t et_modbus_feed(et_modbus_t *mb, const uint8_t *data, uint32_t len)
{
    ET_ASSERT(mb != NULL);
    if (mb == NULL) {
        return 0u;
    }
    mb->resplen = 0u;                       /* 上一应答视为已被取走发送 */
    if ((data != NULL) && (len > 0u)) {
        if ((mb->rxlen + len) > mb->rxcap) {
            mb->stats.discarded++;          /* 溢出: 丢整批并重新同步 */
            mb->rxlen = 0u;
        } else {
            memcpy(mb->rx + mb->rxlen, data, len);
            mb->rxlen += len;
        }
        mb->idle_ms = 0u;                   /* 新字节到达: 静默计时清零 */
    }
    /* 处理循环: 产生应答即停(粘包后续帧留待下次 feed(NULL,0)) */
    while ((mb->resplen == 0u) && (mb->rxlen > 0u)) {
        uint32_t need = expected_len(mb->rx, mb->rxlen);

        if ((need == 0u) || (need > mb->rxlen)) {
            break;                          /* 帧未齐或需静默界定 */
        }
        if (need > mb->rxcap) {
            mb->stats.discarded++;
            mb->rxlen = 0u;
            break;
        }
        process_frame(mb, need);
        memmove(mb->rx, mb->rx + need, mb->rxlen - need);
        mb->rxlen -= need;
    }
    return (mb->resplen != 0u) ? 1u : 0u;
}

const uint8_t *et_modbus_response(const et_modbus_t *mb, uint32_t *len)
{
    ET_ASSERT(mb != NULL);
    if (len != NULL) {
        *len = 0u;
    }
    if ((mb == NULL) || (mb->resplen == 0u)) {
        return NULL;
    }
    if (len != NULL) {
        *len = mb->resplen;
    }
    return mb->tx;
}

void et_modbus_tick(et_modbus_t *mb, uint32_t now_ms)
{
    uint32_t dt;

    ET_ASSERT(mb != NULL);
    if (mb == NULL) {
        return;
    }
    dt = now_ms - mb->last_ms;              /* 无符号减法: 回绕安全 */
    mb->last_ms = now_ms;
    if (mb->rxlen == 0u) {
        mb->idle_ms    = 0u;
        mb->last_rxlen = 0u;
        return;
    }
    if (mb->rxlen == mb->last_rxlen) {
        mb->idle_ms += dt;                  /* 两次 tick 间无新字节 → 累积静默 */
    } else {
        mb->idle_ms    = 0u;                /* 仍在收: 重新计时 */
        mb->last_rxlen = mb->rxlen;
    }
    if (mb->idle_ms >= (uint32_t)mb->cfg.silence_ms) {
        process_frame(mb, mb->rxlen);       /* 残帧按完整帧 CRC 兜底 */
        mb->rxlen      = 0u;
        mb->idle_ms    = 0u;
        mb->last_rxlen = 0u;
    }
}

uint32_t et_modbus_rx_pending(const et_modbus_t *mb)
{
    ET_ASSERT(mb != NULL);
    return (mb == NULL) ? 0u : mb->rxlen;
}

void et_modbus_stats(const et_modbus_t *mb, et_modbus_stats_t *st)
{
    ET_ASSERT(mb != NULL);
    ET_ASSERT(st != NULL);
    if ((mb == NULL) || (st == NULL)) {
        return;
    }
    *st = mb->stats;
}

#endif /* ET_MODULE_MODBUS */

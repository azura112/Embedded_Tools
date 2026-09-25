/**
 * @file    et_modbus.c
 * @brief   Modbus RTU 从站实现 (快路径长度域校验 + 静默路径 CRC 兜底 + 逐字节重同步)
 *
 * v2.7 (CO-5) —— 把主站在 v2.6 确立的判据**对称移植**到从站:
 *   ① `expected_len()` 对 FC_WRITE_MULTIPLE **先校验字节数域**(b[6] == 2*qty)再返回帧长,
 *      不符返回 0("不可信") —— b[6] 不再先于 CRC 被信任;
 *   ② 头部"已不可能是一帧起点"时**逐字节重同步**(不得停等, 否则噪声永久阻塞其后真请求);
 *   ③ CRC 失败只用"与本站期望同形"的候选帧计 crc_err, 其余计 discarded 并重同步。
 *   语义口径与 `et_modbus_master.c` 的 v2.6 口径同一(见 HC-2/HC-3)。
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

/* 丢弃接收缓冲首字节(逐字节重同步: 头部已不可能为帧起点时的兜底) */
static void drop_head(et_modbus_t *mb)
{
    if (mb->rxlen == 0u) {
        return;
    }
    if (mb->rxlen > 1u) {
        memmove(mb->rx, mb->rx + 1u, mb->rxlen - 1u);
    }
    mb->rxlen--;
}

/* 快路径: 已知功能码的期望帧长; 0 = 尚不可判定(收齐后再来 / 等静默 / 长度域不可信) */
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
        if (n < 7u) {
            return 0u;                      /* 长度域未到齐: 尚不可判定 */
        }
        /* v2.7 HC-2 (CO-5(v2.6) 前半): **切片前**完成字节数域合理性校验 ——
         * b[6] 须等于 2*qty, 否则该帧长不可信(被判为噪声头部, 见 feed 的重同步)。
         * 旧实现直接 `return b[6] + 9` = 字节数域先于 CRC 被信任: 噪声只需伪造
         * 一个 b[6] 即可让解析器按该长度**整段前进**, 吞掉紧随其后的真请求
         * (评审 E10 实测形态)。 */
        if ((uint32_t)b[6] !=
            (2u * ((((uint32_t)b[4]) << 8) | (uint32_t)b[5]))) {
            return 0u;
        }
        return (uint32_t)b[6] + 9u;
    default:
        return 0u;              /* 未知功能码: 只能由静默路径界定 */
    }
}

/* 缓冲首部**是否可能**是一帧发往本站(或广播)的请求的起点 (v2.7 HC-2/HC-3)。
 *
 * 只用于"快路径无法定长"(need == 0)时决定**是否逐字节重同步** —— 返回 false 表示
 * "该首部已不可能是本站请求的起点", 丢弃 1 字节安全(不会切掉真帧); 返回 true 表示
 * 仍需等待(帧未齐, 或未知功能码须交静默路径界定)。
 *
 * 判据(从站侧, 与主站 is_expected_shape 同一语义方向):
 *   ① 长度域**未到齐**或字节不足 ⇒ true(无法否定 —— 必须等);
 *   ② 已知功能码 ⇒ 帧首须为本站地址或广播地址, 且 0x10 的长度域须自洽;
 *   ③ 未知功能码 ⇒ 只有**单播**才保留 —— 库承诺"未知功能码回异常 0x01"依赖静默
 *      路径, 该路径只对单播帧有可观测后果; 广播 + 未知功能码按库文档为"静默"
 *      (无应答无动作), 无任何可观测后果 ⇒ 视为噪声丢弃是安全的(且在噪声级联中
 *      必需 —— 否则广播字节会阻塞其后紧跟的真请求)。 */
static bool head_may_be_frame(const et_modbus_cfg_t *cfg, const uint8_t *b, uint32_t n)
{
    if (n < 2u) {
        return true;                        /* 字节不足: 无法否定 */
    }
    if (b[1] == ET_MODBUS_FC_WRITE_MULTIPLE) {
        if ((b[0] != cfg->slave_addr) && (b[0] != 0u)) {
            return false;
        }
        if (n < 7u) {
            return true;                    /* 长度域未到齐: 无法否定 */
        }
        return ((uint32_t)b[6] ==
                (2u * ((((uint32_t)b[4]) << 8) | (uint32_t)b[5])));
    }
    if ((b[1] == ET_MODBUS_FC_READ_HOLDING) ||
        (b[1] == ET_MODBUS_FC_READ_INPUT) ||
        (b[1] == ET_MODBUS_FC_WRITE_SINGLE)) {
        return ((b[0] == cfg->slave_addr) || (b[0] == 0u));
    }
    return (b[0] == cfg->slave_addr);       /* 未知功能码: 仅单播保留 */
}

/* 该序列是否"与本站期望同形"(CRC 失败时是否计入 crc_err 的门槛, HC-3):
 * 帧首 = **本站从站地址**(与主站 `is_expected_shape` 的 `rx[0] == cfg.addr` 严格对称
 * —— 广播帧可从宽成帧, 但它不是"本站期望的应答/请求", 且若把 0 也算作同形, 逐字节
 * 重同步的级联会把噪声里的 0x00 字节误计为 crc_err, 使该计数失去"链路质量"含义),
 * 且 功能码属已支持集合, 且长度与功能码自洽(0x03/0x04/0x06 定长 8;
 * 0x10 = 9 + b[6] 且 b[6] == 2*qty)。
 * 只有这种序列才可能是"本来发往本站的请求", 其 CRC 失败才计 crc_err 并按帧长
 * 整帧丢弃; 其余字节序列属"从未成帧", 计 discarded 并逐字节重同步。 */
static bool frame_is_expected_shape(const et_modbus_cfg_t *cfg,
                                    const uint8_t *b, uint32_t n)
{
    if (n < 2u) {
        return false;
    }
    if (b[0] != cfg->slave_addr) {
        return false;
    }
    switch (b[1]) {
    case ET_MODBUS_FC_READ_HOLDING:
    case ET_MODBUS_FC_READ_INPUT:
    case ET_MODBUS_FC_WRITE_SINGLE:
        return (n == 8u);
    case ET_MODBUS_FC_WRITE_MULTIPLE:
        if (n < 7u) {
            return false;
        }
        return (((uint32_t)b[6] ==
                 (2u * ((((uint32_t)b[4]) << 8) | (uint32_t)b[5])))) &&
               (n == ((uint32_t)b[6] + 9u));
    default:
        return false;                       /* 未知功能码: 不可能是本站期望的帧 */
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

/* CRC + 地址判定后转 execute; 返回**前进量**(字节):
 *   len  = 帧边界已确定(整帧丢弃 / 已处理) —— 按帧长前进;
 *   1    = "从未成帧"的字节序列 —— 逐字节重同步(HC-2)。 */
static uint32_t process_frame(et_modbus_t *mb, uint32_t len)
{
    uint16_t crc_wire;
    uint16_t crc_calc;

    if (len < 4u) {
        mb->stats.discarded++;
        return len;                         /* 残帧: 按已知长度丢弃 */
    }
    /* 线上 CRC 低字节在前(MODBUS 规范) */
    crc_wire = (uint16_t)(((uint16_t)mb->rx[len - 1u] << 8) |
                          (uint16_t)mb->rx[len - 2u]);
    crc_calc = et_crc16_modbus(mb->rx, len - 2u);
    if (crc_wire != crc_calc) {
        if (frame_is_expected_shape(&mb->cfg, mb->rx, len)) {
            mb->stats.crc_err++;            /* 与本站期望同形的候选帧 CRC 坏 */
            return len;                     /* 帧边界已确定: 整帧丢弃 */
        }
        mb->stats.discarded++;              /* 从未成帧: 逐字节重同步(HC-2/CO-5) */
        return 1u;
    }
    if ((mb->rx[0] != mb->cfg.slave_addr) && (mb->rx[0] != 0u)) {
        mb->stats.addr_mismatch++;
        return len;
    }
    mb->stats.frames++;
    execute(mb, len);
    return len;
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
        uint32_t adv;

        if (need == 0u) {
            /* 快路径无法定长 —— 三种可能: ① 帧未齐(等字节); ② 未知功能码(等静默);
             * ③ **噪声头部**。③ 必须当场清掉(逐字节重同步): 若在此停等, 噪声会
             * 永久占据缓冲头部, 其后紧跟的真请求再也无法成帧(评审 E10 的受害形态;
             * v2.7 HC-2 的"不符即逐字节重同步"即指此)。 */
            if (head_may_be_frame(&mb->cfg, mb->rx, mb->rxlen)) {
                break;
            }
            mb->stats.discarded++;
            drop_head(mb);
            continue;
        }
        if (need > mb->rxlen) {
            break;                          /* 帧未齐或需静默界定 */
        }
        if (need > mb->rxcap) {
            mb->stats.discarded++;
            mb->rxlen = 0u;
            break;
        }
        adv = process_frame(mb, need);      /* 前进量由判定结果给出(HC-2) */
        if (adv >= mb->rxlen) {
            mb->rxlen = 0u;
        } else {
            memmove(mb->rx, mb->rx + adv, mb->rxlen - adv);
            mb->rxlen -= adv;
        }
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
        /* 残帧按完整帧 CRC 兜底 —— **语义保持不变**(v2.7 计划: 静默路径只换口径
         * 不改兜底方式): 整个残段作为一次整帧尝试, 不逐字节重同步; 返回值(前进量)
         * 在此被有意忽略, 缓冲统一清零。 */
        (void)process_frame(mb, mb->rxlen);
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

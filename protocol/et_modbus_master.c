/**
 * @file    et_modbus_master.c
 * @brief   Modbus RTU 主站实现 (单事务状态机 + 超时重发 + 陈旧字节隔离)
 *
 * 协议常量全部经 et_modbus.h 的 ET_MODBUS_* 引用(HC-5 单一来源, 禁止复制数值)。
 */
#include "et_modbus_master.h"

#if ET_MODULE_MODBUS

#include <string.h>
#include "et_crc.h"

/* 追加 CRC16-MODBUS(线上低字节在前), 返回帧总长 */
static uint32_t seal(uint8_t *buf, uint32_t len)
{
    uint16_t crc = et_crc16_modbus(buf, len);

    buf[len]      = (uint8_t)(crc & 0xFFu);
    buf[len + 1u] = (uint8_t)(crc >> 8);
    return len + 2u;
}

/* 读类功能码(主站可发起) */
static bool fc_is_read(uint8_t fc)
{
    return (fc == ET_MODBUS_FC_READ_HOLDING) || (fc == ET_MODBUS_FC_READ_INPUT);
}

/* 写类功能码(主站可发起) */
static bool fc_is_write(uint8_t fc)
{
    return (fc == ET_MODBUS_FC_WRITE_SINGLE) || (fc == ET_MODBUS_FC_WRITE_MULTIPLE);
}

/* 缓冲首部是否与在途事务"严格同形"(v2.6 P1-1 / HC-2 的 crc_err 计数门槛):
 * 帧首 = 本站地址, 且功能码与在途期望一致(正常应答 fc, 或异常应答 fc|0x80) ——
 * 只有这种序列才可能是"本来发往本站的应答", 其 CRC 失败才计入 crc_err 并按帧长
 * 丢弃整帧; 其余字节序列属"从未成帧", 一律逐字节重同步(不污染 crc_err)。
 *
 * v2.7 P2-1(`CO-9(v2.6)`)清理: 原实现另有一条读分支子句
 * `fc_is_read(fc) → (rx[2] == 2*exp_qty)` —— 该子句在**到达点恒真**
 * (feed 的读分支已用该等式定长后才走到这里), 属死代码, 已删。
 * 本条判据的**有效判别力 = 地址 + 功能码**(评审探针 E12 已验证原分支命中 0 次)。 */
static bool is_expected_shape(const et_modbus_master_t *m, uint8_t fc)
{
    return (m->rx[0] == (uint8_t)m->cfg.addr) &&
           ((fc == (uint8_t)m->exp_fc) ||
            (fc == (uint8_t)(m->exp_fc | 0x80u)));
}

/* 丢弃接收缓冲首字节(逐字节重同步: 长度不可判定时的兜底) */
static void drop_head(et_modbus_master_t *m)
{
    if (m->rxlen == 0u) {
        return;
    }
    if (m->rxlen > 1u) {
        memmove(m->rx, m->rx + 1u, m->rxlen - 1u);
    }
    m->rxlen--;
}

/* 丢弃接收缓冲前 n 字节(帧边界已确定时整帧丢弃 —— 比逐字节重同步更干净:
 * 残字节不会在后续 feed 中被重新拼成假帧) */
static void drop_n(et_modbus_master_t *m, uint32_t n)
{
    if (n >= m->rxlen) {
        m->rxlen = 0u;
        return;
    }
    memmove(m->rx, m->rx + n, m->rxlen - n);
    m->rxlen -= n;
}

/* 进入终态(清除在途标记) */
static void to_final(et_modbus_master_t *m, uint8_t state)
{
    m->state   = state;
    m->pending = false;
    m->on_wire = false;
    m->rxlen   = 0u;
}

/* 组装读请求: [addr, fc, reg_hi, reg_lo, qty_hi, qty_lo, crc] */
static void build_read(et_modbus_master_t *m, uint8_t fc, uint16_t reg, uint16_t qty)
{
    m->tx[0] = (uint8_t)m->cfg.addr;
    m->tx[1] = fc;
    m->tx[2] = (uint8_t)(reg >> 8);
    m->tx[3] = (uint8_t)(reg & 0xFFu);
    m->tx[4] = (uint8_t)(qty >> 8);
    m->tx[5] = (uint8_t)(qty & 0xFFu);
    m->txlen = seal(m->tx, 6u);
}

/* 组装写请求 */
static bool build_write(et_modbus_master_t *m, uint8_t fc, uint16_t reg,
                        const uint16_t *vals, uint16_t qty)
{
    uint32_t i;

    if (fc == ET_MODBUS_FC_WRITE_SINGLE) {
        if ((qty != 1u) || (m->txcap < 8u)) {
            return false;
        }
        m->tx[0] = (uint8_t)m->cfg.addr;
        m->tx[1] = fc;
        m->tx[2] = (uint8_t)(reg >> 8);
        m->tx[3] = (uint8_t)(reg & 0xFFu);
        m->tx[4] = (uint8_t)(vals[0] >> 8);
        m->tx[5] = (uint8_t)(vals[0] & 0xFFu);
        m->txlen = seal(m->tx, 6u);
        return true;
    }
    if (fc == ET_MODBUS_FC_WRITE_MULTIPLE) {
        uint32_t need = 9u + (2u * (uint32_t)qty);

        if (need > m->txcap) {
            return false;
        }
        m->tx[0] = (uint8_t)m->cfg.addr;
        m->tx[1] = fc;
        m->tx[2] = (uint8_t)(reg >> 8);
        m->tx[3] = (uint8_t)(reg & 0xFFu);
        m->tx[4] = (uint8_t)(qty >> 8);
        m->tx[5] = (uint8_t)(qty & 0xFFu);
        m->tx[6] = (uint8_t)(2u * (uint32_t)qty);
        for (i = 0u; i < (uint32_t)qty; i++) {
            m->tx[7u + (2u * i)]      = (uint8_t)(vals[i] >> 8);
            m->tx[7u + (2u * i) + 1u] = (uint8_t)(vals[i] & 0xFFu);
        }
        m->txlen = seal(m->tx, 7u + (2u * (uint32_t)qty));
        return true;
    }
    return false;
}

/* 事务公共起始状态(组帧成功后) */
static void begin_tx(et_modbus_master_t *m, uint8_t fc, uint16_t qty)
{
    m->exp_fc  = fc;
    m->exp_qty = qty;
    m->reslen  = 0u;
    m->exc     = 0u;
    m->retries = 0u;
    m->rxlen   = 0u;            /* 新事务: 清空接收缓冲(防上一事务残字节污染) */
    m->pending = true;
    m->on_wire = false;
    m->state   = ET_MB_BUSY;
}

/* 处理一个已通过 CRC 与地址判定的完整应答帧; need = 帧长;
 * 返回 1 = 本帧终结事务。帧边界已确定, 不符即整帧丢弃(不逐字节重同步)。 */
static uint32_t handle_frame(et_modbus_master_t *m, uint32_t need)
{
    uint8_t fc = m->rx[1];

    if (fc == (uint8_t)(m->exp_fc | 0x80u)) {               /* 异常应答: 不重试 */
        m->exc = m->rx[2];
        m->stats.exceptions++;
        to_final(m, ET_MB_EXC);
        return 1u;
    }
    if (fc != m->exp_fc) {                                  /* 功能码不符 */
        m->stats.discarded++;
        drop_n(m, need);
        return 0u;
    }
    if (fc_is_read(m->exp_fc)) {
        /* 读应答: 字节数域与期望寄存器数**严格相等**是到达此处的前置条件 ——
         * feed() 的读分支已用 `rx[2] == 2*exp_qty` 定长(v2.6 P1-1 / HC-2):
         * 不满足者在进入本函数之前就被判为"从未成帧"并逐字节重同步。故此处只做
         * **不变式断言**, 不再保留 v2.5 那条"字节数不符 → discarded"分支
         * (v2.7 P2-1 / `CO-9(v2.6)`: 探针 E12 实测该分支在全量用例中命中 0 次)。
         * ET_ASSERT 默认展开为 `((void)0)`(见 et_config.h), 发布配置零开销。 */
        ET_ASSERT((uint32_t)m->rx[2] == (2u * (uint32_t)m->exp_qty));
        m->reslen = (uint32_t)m->rx[2];                     /* 数据在 rx[3 .. 3+bc) */
        m->stats.responses++;
        to_final(m, ET_MB_OK);
        return 1u;
    }
    /* 写应答: 回显 [reg_hi, reg_lo, qty/val_hi, qty/val_lo] 须与请求一致 */
    if ((m->rx[2] != m->tx[2]) || (m->rx[3] != m->tx[3]) ||
        (m->rx[4] != m->tx[4]) || (m->rx[5] != m->tx[5])) {
        m->stats.discarded++;
        drop_n(m, need);
        return 0u;
    }
    m->reslen = 0u;
    m->stats.responses++;
    to_final(m, ET_MB_OK);
    return 1u;
}

bool et_modbus_master_init(et_modbus_master_t *m, const et_modbus_master_cfg_t *cfg,
                           uint8_t *rxbuf, uint32_t rxcap,
                           uint8_t *txbuf, uint32_t txcap)
{
    ET_ASSERT(m != NULL);
    ET_ASSERT(cfg != NULL);
    ET_ASSERT(rxbuf != NULL);
    ET_ASSERT(txbuf != NULL);
    if ((m == NULL) || (cfg == NULL) || (rxbuf == NULL) || (txbuf == NULL)) {
        return false;
    }
    if (cfg->addr > 247u) {
        return false;                       /* 248~255 保留(0 = 广播) */
    }
    if (cfg->resp_timeout_ms == 0u) {
        return false;                       /* 超时必须由调用方给出(不内部取时基) */
    }
    if ((rxcap < ET_MODBUS_ADU_MAX) || (txcap < ET_MODBUS_ADU_MAX)) {
        return false;                       /* 硬底线: RTU ADU 上限 */
    }
    m->cfg     = *cfg;
    m->tx      = txbuf;
    m->txcap   = txcap;
    m->txlen   = 0u;
    m->rx      = rxbuf;
    m->rxcap   = rxcap;
    m->rxlen   = 0u;
    m->reslen  = 0u;
    m->exp_fc  = 0u;
    m->exp_qty = 0u;
    m->state   = ET_MB_IDLE;
    m->retries = 0u;
    m->exc     = 0u;
    m->pending = false;
    m->on_wire = false;
    m->inited  = true;
    m->sent_ms = 0u;
    memset(&m->stats, 0, sizeof(m->stats));
    return true;
}

bool et_modbus_master_read(et_modbus_master_t *m, uint8_t fc, uint16_t reg, uint16_t qty)
{
    ET_ASSERT(m != NULL);
    if ((m == NULL) || !m->inited) {
        return false;
    }
    if (!fc_is_read(fc)) {
        return false;                       /* 读类功能码白名单(经 ET_MODBUS_FC_* 引用) */
    }
    if ((qty == 0u) || (qty > ET_MODBUS_RD_QTY_MAX)) {
        return false;
    }
    if (m->cfg.addr == 0u) {
        return false;                       /* 广播读无应答, 主站不允许 */
    }
    if (m->state == ET_MB_BUSY) {
        return false;                       /* 单事务: 一次一个在途请求 */
    }
    if (m->txcap < 8u) {
        return false;
    }
    build_read(m, fc, reg, qty);
    begin_tx(m, fc, qty);
    return true;
}

bool et_modbus_master_write(et_modbus_master_t *m, uint8_t fc, uint16_t reg,
                            const uint16_t *vals, uint16_t qty)
{
    ET_ASSERT(m != NULL);
    if ((m == NULL) || !m->inited || (vals == NULL)) {
        return false;
    }
    if (!fc_is_write(fc)) {
        return false;                       /* 写类功能码白名单(经 ET_MODBUS_FC_* 引用) */
    }
    if ((qty == 0u) || (qty > ET_MODBUS_WR_QTY_MAX)) {
        return false;
    }
    if (m->state == ET_MB_BUSY) {
        return false;                       /* 单事务: 一次一个在途请求 */
    }
    if (!build_write(m, fc, reg, vals, qty)) {
        return false;                       /* 单写 qty≠1 / 缓冲放不下 */
    }
    begin_tx(m, fc, qty);
    return true;
}

const uint8_t *et_modbus_master_tx(const et_modbus_master_t *m, uint32_t *len)
{
    if (len != NULL) {
        *len = 0u;
    }
    if ((m == NULL) || !m->inited || !m->pending) {
        return NULL;
    }
    if (len != NULL) {
        *len = m->txlen;
    }
    return m->tx;
}

void et_modbus_master_sent(et_modbus_master_t *m, uint32_t now_ms)
{
    ET_ASSERT(m != NULL);
    if ((m == NULL) || !m->inited) {
        return;
    }
    if ((m->state != ET_MB_BUSY) || !m->pending) {
        return;                             /* 无待发请求: 空操作 */
    }
    m->pending = false;
    m->stats.requests++;                    /* 计数口径: 每次上线计一次(含重发) */
    if (m->cfg.addr == 0u) {                /* 广播写: 不等应答, 立即终态 */
        m->state = ET_MB_OK;
        return;
    }
    m->on_wire = true;
    m->sent_ms = now_ms;
}

uint32_t et_modbus_master_feed(et_modbus_master_t *m, const uint8_t *data, uint32_t len)
{
    ET_ASSERT(m != NULL);
    if ((m == NULL) || !m->inited) {
        return 0u;
    }
    if (m->state != ET_MB_BUSY) {
        /* 终态/空闲期喂入 = 迟到或陈旧字节: 丢弃并计数, **不得污染下一事务**
         * (下一事务发起时 begin_tx() 还会再清一次接收缓冲) */
        if ((data != NULL) && (len > 0u)) {
            m->stats.late++;
        }
        m->rxlen = 0u;
        return 0u;
    }
    if ((data != NULL) && (len > 0u)) {
        if ((m->rxlen + len) > m->rxcap) {
            m->stats.discarded++;           /* 溢出: 丢整批重新同步 */
            m->rxlen = 0u;
        } else {
            memcpy(m->rx + m->rxlen, data, len);
            m->rxlen += len;
        }
    }
    while (m->rxlen >= 2u) {
        uint8_t  fc = m->rx[1];
        uint32_t need;
        uint16_t crc_wire;
        uint16_t crc_calc;

        if ((fc & 0x80u) != 0u) {
            need = 5u;                      /* 异常应答定长 */
        } else if (fc_is_read(fc)) {
            if (m->rxlen < 3u) {
                break;                      /* 等字节数域 */
            }
            /* 单事务语义下主站已知本事务的期望寄存器数: 应答字节数域必须与之
             * **严格相符**, 否则该序列不可能是本事务的有效应答 —— 逐字节重同步
             * (HC-2, v2.6 P1-1)。判据取"== 2*exp_qty"而非"≤ 2*RD_QTY_MAX 的合法域":
             * 后者会把"恰为偶数的垃圾字节"(如噪声 04 46 ..)当成合法帧长, 解析器
             * 停等一个永不到齐的长度, 真应答随后到达也只能超时(本机实测的残留口子)。 */
            if ((uint32_t)m->rx[2] != (2u * (uint32_t)m->exp_qty)) {
                m->stats.discarded++;
                drop_head(m);
                continue;
            }
            need = 3u + (uint32_t)m->rx[2] + 2u;
        } else if (fc_is_write(fc)) {
            need = 8u;                      /* 写应答定长(回显) */
        } else {
            m->stats.discarded++;           /* 无法定长: 逐字节重同步 */
            drop_head(m);
            continue;
        }
        if (m->rxlen < need) {
            break;                          /* 帧未齐 */
        }
        crc_wire = (uint16_t)(((uint16_t)m->rx[need - 1u] << 8) |
                              (uint16_t)m->rx[need - 2u]);
        crc_calc = et_crc16_modbus(m->rx, need - 2u);
        if (crc_wire != crc_calc) {
            if (is_expected_shape(m, fc)) {
                m->stats.crc_err++;         /* 与在途事务严格同形的候选帧 CRC 坏 */
                drop_n(m, need);
            } else {
                m->stats.discarded++;       /* 未成帧的字节序列: 逐字节重同步(HC-2, P1-1) */
                drop_head(m);
            }
            continue;
        }
        if (m->rx[0] != (uint8_t)m->cfg.addr) {
            m->stats.addr_mismatch++;
            drop_n(m, need);
            continue;
        }
        return handle_frame(m, need);
    }
    return 0u;
}

et_mb_status_t et_modbus_master_poll(et_modbus_master_t *m, uint32_t now_ms)
{
    ET_ASSERT(m != NULL);
    if ((m == NULL) || !m->inited) {
        return ET_MB_REJECT;
    }
    if (m->state != ET_MB_BUSY) {
        return (et_mb_status_t)m->state;    /* 终态保持 */
    }
    if (!m->on_wire) {
        return ET_MB_BUSY;                  /* 已组帧但尚未 sent(): 等待上线 */
    }
    if ((uint32_t)(now_ms - m->sent_ms) < m->cfg.resp_timeout_ms) {
        return ET_MB_BUSY;                  /* 未到超时 */
    }
    if (m->retries < m->cfg.retry_max) {
        m->retries++;                       /* 超时: 置重发(tx() 再返回同一请求) */
        m->stats.retries++;
        m->pending = true;
        m->on_wire = false;                 /* 待调用方再次 sent() 重新计时 */
        return ET_MB_BUSY;
    }
    m->stats.timeouts++;
    to_final(m, ET_MB_TIMEOUT);             /* 重发耗尽 */
    return ET_MB_TIMEOUT;
}

uint16_t et_modbus_master_result(const et_modbus_master_t *m, uint16_t *qty)
{
    if ((m == NULL) || !m->inited || (m->reslen < 2u)) {
        if (qty != NULL) {
            *qty = 0u;
        }
        return 0u;
    }
    if (qty != NULL) {
        *qty = (uint16_t)(m->reslen / 2u);
    }
    return (uint16_t)(((uint16_t)m->rx[3] << 8) | (uint16_t)m->rx[4]);
}

const uint8_t *et_modbus_master_data(const et_modbus_master_t *m, uint32_t *len)
{
    if (len != NULL) {
        *len = 0u;
    }
    if ((m == NULL) || !m->inited || (m->reslen == 0u)) {
        return NULL;
    }
    if (len != NULL) {
        *len = m->reslen;
    }
    return m->rx + 3u;
}

uint8_t et_modbus_master_exc(const et_modbus_master_t *m)
{
    ET_ASSERT(m != NULL);
    return (m == NULL) ? 0u : m->exc;
}

void et_modbus_master_stats(const et_modbus_master_t *m, et_modbus_master_stats_t *st)
{
    ET_ASSERT(m != NULL);
    ET_ASSERT(st != NULL);
    if ((m == NULL) || (st == NULL)) {
        return;
    }
    *st = m->stats;
}

#endif /* ET_MODULE_MODBUS */

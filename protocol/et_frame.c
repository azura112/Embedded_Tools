/**
 * @file    et_frame.c
 * @brief   字节流帧解析状态机实现
 */
#include "et_frame.h"
#include "et_crc.h"

#if ET_MODULE_FRAME

#include <stddef.h>
#include <string.h>

/* 内部状态 */
enum {
    FR_ST_HEADER = 0,   /* 扫描帧头     */
    FR_ST_LEN,          /* 等待长度域   */
    FR_ST_PAYLOAD,      /* 接收载荷     */
    FR_ST_CHECK,        /* 接收校验字节 */
    FR_ST_TAIL,         /* 等待帧尾     */
};

static uint16_t fr_chk_size(et_frame_crc_t crc)
{
    switch (crc) {
    case ET_FRAME_CRC_XOR:
    case ET_FRAME_CRC_SUM8:
    case ET_FRAME_CRC_CRC8:
        return 1u;
    case ET_FRAME_CRC_CRC16_MODBUS:
    case ET_FRAME_CRC_CCITT:
        return 2u;
    case ET_FRAME_CRC_NONE:
    default:
        return 0u;
    }
}

/* 校验累加器初值(收发共用) */
static uint16_t fr_acc_init(et_frame_crc_t crc)
{
    switch (crc) {
    case ET_FRAME_CRC_CRC16_MODBUS:
        return ET_CRC16_MODBUS_INIT;
    case ET_FRAME_CRC_CCITT:
        return ET_CRC16_CCITT_INIT;
    default:
        return 0u;                      /* XOR/SUM8/CRC8 复用低 8 位语义 */
    }
}

/* 校验累加器推进一个字节(收发共用) */
static uint16_t fr_acc_add(et_frame_crc_t crc, uint16_t acc, uint8_t b)
{
    switch (crc) {
    case ET_FRAME_CRC_XOR:
        return (uint16_t)((uint8_t)acc ^ b);
    case ET_FRAME_CRC_SUM8:
        return (uint8_t)(acc + b);
    case ET_FRAME_CRC_CRC8:
        return et_crc8_update((uint8_t)acc, &b, 1u);
    case ET_FRAME_CRC_CRC16_MODBUS:
        return et_crc16_modbus_update(acc, &b, 1u);
    case ET_FRAME_CRC_CCITT:
        return et_crc16_ccitt_update(acc, &b, 1u);
    default:
        return acc;
    }
}

/* 计算最终校验字节序(写入 out, 返回写入字节数) */
static uint16_t fr_chk_store(et_frame_crc_t crc, uint16_t acc,
                             uint8_t *out)
{
    switch (crc) {
    case ET_FRAME_CRC_XOR:
    case ET_FRAME_CRC_SUM8:
    case ET_FRAME_CRC_CRC8:
        out[0] = (uint8_t)acc;
        return 1u;
    case ET_FRAME_CRC_CRC16_MODBUS:
        out[0] = (uint8_t)(acc & 0xFFu);        /* 低前高后 */
        out[1] = (uint8_t)(acc >> 8);
        return 2u;
    case ET_FRAME_CRC_CCITT:
        out[0] = (uint8_t)(acc >> 8);           /* 高前低后 */
        out[1] = (uint8_t)(acc & 0xFFu);
        return 2u;
    default:
        return 0u;
    }
}

static bool fr_check_ok(const et_frame_parser_t *p)
{
    uint8_t expect[2];
    uint8_t n;

    if (p->crc == ET_FRAME_CRC_NONE) {
        return true;
    }
    n = (uint8_t)fr_chk_store(p->crc, p->acc16, expect);
    return (n != 0u) &&
           (expect[0] == p->chk[0]) &&
           ((n < 2u) || (expect[1] == p->chk[1]));
}

static void fr_reset_scan(et_frame_parser_t *p)
{
    p->state   = FR_ST_HEADER;
    p->hdr_idx = 0u;
    p->expect  = 0u;
    p->got     = 0u;
    p->chk_got = 0u;
}

static bool fr_finish(et_frame_parser_t *p)
{
    uint16_t len = p->got;                  /* 先保存长度再复位 */

    p->frame_count++;
    fr_reset_scan(p);
    if (p->on_frame != NULL) {
        p->on_frame(p, len, p->user);
    }
    return true;
}

bool et_frame_parser_init(et_frame_parser_t *p, const et_frame_cfg_t *cfg)
{
    ET_ASSERT(p != NULL);
    ET_ASSERT(cfg != NULL);
    ET_ASSERT(cfg->header != NULL);
    if ((p == NULL) || (cfg == NULL) || (cfg->header == NULL)) {
        return false;
    }
    if ((cfg->header_len == 0u) || (cfg->header_len > 4u)) {
        return false;
    }

    (void)memcpy(p->header, cfg->header, cfg->header_len);
    p->header_len = cfg->header_len;
    p->use_len    = cfg->use_len;
    p->fixed_len  = cfg->fixed_len;
    p->tail       = cfg->tail;
    p->use_tail   = cfg->use_tail;
    p->crc        = cfg->crc;
    p->rx_buf     = cfg->rx_buf;
    p->rx_cap     = cfg->rx_cap;
    p->on_frame   = cfg->on_frame;
    p->user       = cfg->user;

    p->frame_count = 0u;
    p->err_count   = 0u;
    fr_reset_scan(p);
    p->acc16 = fr_acc_init(p->crc);
    return true;
}

void et_frame_reset(et_frame_parser_t *p)
{
    fr_reset_scan(p);
}

/* 进入校验阶段; 若无校验且无帧尾则当场完帧(返回 true 表示已完帧) */
static bool fr_enter_check(et_frame_parser_t *p)
{
    if (fr_chk_size(p->crc) == 0u) {
        if (p->use_tail) {
            p->state = FR_ST_TAIL;
        } else {
            return fr_finish(p);
        }
    } else {
        p->state   = FR_ST_CHECK;
        p->chk_got = 0u;
    }
    return false;
}

bool et_frame_feed(et_frame_parser_t *p, uint8_t byte)
{
    switch (p->state) {

    case FR_ST_HEADER:
        if (byte == p->header[p->hdr_idx]) {
            p->hdr_idx++;
            if (p->hdr_idx >= p->header_len) {  /* 帧头锁定 */
                if (p->use_len) {
                    p->state = FR_ST_LEN;
                    p->acc16 = fr_acc_init(p->crc); /* 校验从长度域起算 */
                } else {
                    p->expect = p->fixed_len;
                    p->got    = 0u;
                    p->acc16  = fr_acc_init(p->crc);
                    if (p->expect == 0u) {
                        if (fr_enter_check(p)) {
                            return true;
                        }
                    } else {
                        p->state = FR_ST_PAYLOAD;
                    }
                }
            }
        } else if ((p->hdr_idx != 0u) && (byte == p->header[0])) {
            p->hdr_idx = 1u;                    /* 部分匹配回退 */
        } else {
            p->hdr_idx = 0u;
        }
        break;

    case FR_ST_LEN:
        p->expect = byte;
        p->acc16  = fr_acc_add(p->crc, p->acc16, byte);  /* 长度域参与校验 */
        if ((p->rx_cap == 0u) || ((uint16_t)p->expect > p->rx_cap)) {
            p->err_count++;
            fr_reset_scan(p);                   /* 超长: 放弃本帧重新同步 */
        } else {
            p->got = 0u;
            if (p->expect == 0u) {
                if (fr_enter_check(p)) {
                    return true;
                }
            } else {
                p->state = FR_ST_PAYLOAD;
            }
        }
        break;

    case FR_ST_PAYLOAD:
        p->acc16 = fr_acc_add(p->crc, p->acc16, byte);
        if (p->got < p->rx_cap) {
            p->rx_buf[p->got] = byte;
        }
        p->got++;
        if (p->got >= p->expect) {
            if (fr_enter_check(p)) {
                return true;                /* 无校验配置在此完帧 */
            }
        }
        break;

    case FR_ST_CHECK:
        p->chk[p->chk_got++] = byte;
        if (p->chk_got >= fr_chk_size(p->crc)) {
            if (fr_check_ok(p)) {
                if (p->use_tail) {
                    p->state = FR_ST_TAIL;
                } else {
                    return fr_finish(p);
                }
            } else {
                p->err_count++;
                fr_reset_scan(p);
            }
        }
        break;

    case FR_ST_TAIL:
        if (byte == p->tail) {
            return fr_finish(p);
        }
        p->err_count++;
        fr_reset_scan(p);
        break;

    default:
        fr_reset_scan(p);
        break;
    }
    return false;
}

uint32_t et_frame_write(et_frame_parser_t *p, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t frames = 0u;

    for (i = 0u; i < len; i++) {
        if (et_frame_feed(p, data[i])) {
            frames++;
        }
    }
    return frames;
}

uint16_t et_frame_pack(const et_frame_cfg_t *cfg,
                       const uint8_t *payload, uint16_t len,
                       uint8_t *out, uint16_t out_cap)
{
    uint16_t total;
    uint16_t pos = 0u;
    uint16_t i;
    uint16_t acc;

    ET_ASSERT(cfg != NULL);
    ET_ASSERT(out != NULL);
    if ((cfg == NULL) || (out == NULL) ||
        (cfg->header == NULL) || (cfg->header_len == 0u) ||
        (cfg->header_len > 4u)) {
        return 0u;
    }
    if ((!cfg->use_len) && (len != cfg->fixed_len)) {
        return 0u;                              /* 定长模式载荷必须等长 */
    }
    if (len > 255u) {
        return 0u;
    }

    total = (uint16_t)(cfg->header_len + (cfg->use_len ? 1u : 0u) +
                       len + fr_chk_size(cfg->crc) +
                       (cfg->use_tail ? 1u : 0u));
    if (out_cap < total) {
        return 0u;
    }

    for (i = 0u; i < cfg->header_len; i++) {
        out[pos++] = cfg->header[i];
    }
    if (cfg->use_len) {
        out[pos++] = (uint8_t)len;
    }

    /* 校验覆盖: 长度域(若有) + 载荷, 与接收端一致 */
    acc = fr_acc_init(cfg->crc);
    if (cfg->use_len) {
        acc = fr_acc_add(cfg->crc, acc, (uint8_t)len);
    }
    for (i = 0u; i < len; i++) {
        out[pos++] = payload[i];
        acc = fr_acc_add(cfg->crc, acc, payload[i]);
    }
    pos += fr_chk_store(cfg->crc, acc, &out[pos]);

    if (cfg->use_tail) {
        out[pos++] = cfg->tail;
    }
    return pos;
}

#endif /* ET_MODULE_FRAME */

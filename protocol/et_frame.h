/**
 * @file    et_frame.h
 * @brief   字节流帧解析状态机 (逐字节喂入, ISR 友好; 配套组帧函数)
 *
 * 帧格式(全部可裁剪):
 *   [帧头 1~4B][长度域 1B?][载荷 nB][校验 0~2B?][帧尾 1B?]
 *
 * 约定:
 *   - 长度域 = 载荷字节数(0~255); 关闭长度域时使用配置中的固定载荷长度;
 *   - 校验覆盖范围: 长度域(若有) + 载荷, 不含帧头/帧尾;
 *   - CRC16-MODBUS 校验字节低前高后, CRC16-CCITT 高前低后;
 *   - 载荷直接写入调用方缓冲区, 解析器内部零拷贝零额外 RAM。
 *
 * 并发策略:
 *   - feed() 可在 UART 接收中断中逐字节调用(单写者);
 *   - 同一解析器实例禁止多处同时 feed。
 *
 * 容错:
 *   - 帧头扫描支持"部分匹配回退"(如帧头 AA 55 对流 AA AA 55 正确同步);
 *   - 校验错/超长/帧尾错 => err_count++, 状态复位重新扫帧头;
 *   - 回调在 feed() 调用上下文中同步执行(可能位于 ISR, 用户自行保证时效)。
 */
#ifndef ET_FRAME_H
#define ET_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_FRAME

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ET_FRAME_CRC_NONE = 0,
    ET_FRAME_CRC_XOR,               /* 异或 1 字节          */
    ET_FRAME_CRC_SUM8,              /* 累加和 1 字节        */
    ET_FRAME_CRC_CRC8,              /* CRC8 poly07, 1 字节  */
    ET_FRAME_CRC_CRC16_MODBUS,      /* 低字节在前           */
    ET_FRAME_CRC_CCITT,             /* 高字节在前           */
} et_frame_crc_t;

struct et_frame_parser;

typedef void (*et_frame_on_frame_fn)(struct et_frame_parser *p,
                                     uint16_t len, void *user);

typedef struct {
    /* ---- 协议描述(初始化时拷贝, 之后可释放) ---- */
    const uint8_t *header;          /* 帧头序列, 长度 1~4       */
    uint8_t        header_len;
    bool           use_len;         /* true: 帧内含 1B 长度域   */
    uint8_t        fixed_len;       /* use_len=false 时的定长载荷 */
    uint8_t        tail;            /* use_tail=true 时帧尾字节 */
    bool           use_tail;
    et_frame_crc_t crc;

    /* ---- 接收资源 ---- */
    uint8_t       *rx_buf;          /* 载荷缓冲(调用方提供)     */
    uint16_t       rx_cap;          /* 缓冲容量, >=1            */

    /* ---- 完帧回调 ---- */
    et_frame_on_frame_fn on_frame;  /* 可为 NULL(仅统计计数)    */
    void          *user;
} et_frame_cfg_t;

typedef struct et_frame_parser {
    /* 配置副本(只读) */
    uint8_t             header[4];
    uint8_t             header_len;
    bool                use_len;
    uint8_t             fixed_len;
    uint8_t             tail;
    bool                use_tail;
    et_frame_crc_t      crc;
    uint8_t            *rx_buf;
    uint16_t            rx_cap;
    et_frame_on_frame_fn on_frame;
    void               *user;

    /* 运行状态(内部维护) */
    uint8_t             state;      /* 内部状态机状态           */
    uint8_t             hdr_idx;
    uint16_t            expect;     /* 期望载荷长度             */
    uint16_t            got;        /* 已收载荷字节数           */
    uint16_t            acc16;      /* 校验累加器               */
    uint8_t             chk_got;    /* 已收校验字节数           */
    uint8_t             chk[2];     /* 收到的校验字节           */

    /* 统计 */
    uint32_t            frame_count;
    uint32_t            err_count;
} et_frame_parser_t;

/* 初始化(拷贝配置并复位状态), 配置非法返回 false */
bool et_frame_parser_init(et_frame_parser_t *p, const et_frame_cfg_t *cfg);

/* 复位到帧头扫描状态(不清计数) */
void et_frame_reset(et_frame_parser_t *p);

/* 喂入 1 字节; 完成一帧有效帧时返回 true(已回调 on_frame) */
bool et_frame_feed(et_frame_parser_t *p, uint8_t byte);

/* 批量喂入, 返回期间完成的帧数 */
uint32_t et_frame_write(et_frame_parser_t *p, const uint8_t *data, uint32_t len);

/* 组帧: 按同一协议格式打包 payload 到 out, 返回总字节数(容量不足返回 0) */
uint16_t et_frame_pack(const et_frame_cfg_t *cfg,
                       const uint8_t *payload, uint16_t len,
                       uint8_t *out, uint16_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_FRAME */
#endif /* ET_FRAME_H */

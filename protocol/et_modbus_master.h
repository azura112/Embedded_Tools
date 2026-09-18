/**
 * @file    et_modbus_master.h
 * @brief   Modbus RTU 主站 (单事务状态机, 零分配)
 *
 * 定位 (v2.5, 第 34 模块):
 *  - 与 `et_modbus`(从站)配对的主站侧: 发起 0x03/0x04 读、0x06/0x10 写,
 *    处理应答超时重发、异常码上报、迟到/陈旧字节隔离;
 *  - **复用从站协议常量**: 本头文件 `#include "et_modbus.h"`, 功能码/异常码/
 *    数量边界/ADU 上限一律用 `ET_MODBUS_*` 宏引用, **禁止复制数值**
 *    (沿 `et_xmodem_tx` 共享 `ET_MODULE_XMODEM` 先例, 模块开关同为
 *    `ET_MODULE_MODBUS`, 不新增开关);
 *  - 零动态内存: 请求缓冲与接收组装缓冲均由调用方持有。
 *
 * **职责边界 (HC-4, 务必先读)**: 本模块只做**单事务**状态机 ——
 *  一次一个在途请求。**不内置**多从站轮询表、**不内置**调度器、
 *  **不内部取时基**(`now_ms` 由调用方注入)、**不**按波特率自动换算超时
 *  (阈值由调用方算好后写入 `cfg.resp_timeout_ms`)。
 *  多从站轮询 = 本模块 × `et_sched` × 应用侧从站表 —— 见 API_GUIDE 11.14。
 *
 * 事务流程 (调用方视角, 四步):
 *  @code
 *    et_modbus_master_read(&m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u);  // ① 组帧
 *    // ② 取待发请求并上线, 然后 sent():
 *    const uint8_t *req = et_modbus_master_tx(&m, &len);
 *    if (req != NULL) { uart_write(req, len); et_modbus_master_sent(&m, now); }
 *    // ③ 收到字节就喂:
 *    (void)et_modbus_master_feed(&m, chunk, n);
 *    // ④ 周期 poll: 推进/超时重发/终态; 若 tx() 又返回非 NULL 说明要重发
 *    st = et_modbus_master_poll(&m, now);
 *    if (st == ET_MB_BUSY) { req = et_modbus_master_tx(&m, &len);
 *                            if (req != NULL) { uart_write(req, len);
 *                                               et_modbus_master_sent(&m, now); } }
 *  @endcode
 *  `et_modbus_master_tx()` 返回非 NULL 的**唯一含义 = "有请求待上线"**:
 *  首次组帧后, 以及超时重发时(重发字节与首帧逐字节相同, 含 CRC)。
 *
 * 状态语义:
 *  - `ET_MB_IDLE`    : 无在途事务(初始态);
 *  - `ET_MB_BUSY`    : 请求已组帧/已上线, 等应答(或待重发);
 *  - `ET_MB_OK`      : 收到正常应答(广播写在 `sent()` 后立即置此态);
 *  - `ET_MB_EXC`     : 收到异常应答(`fc|0x80`), **不重试**; 异常码见 `_exc()`;
 *  - `ET_MB_TIMEOUT` : 超时且重发耗尽;
 *  - `ET_MB_REJECT`  : 组帧参数非法/未初始化 —— `read`/`write` 直接返回 false
 *                      (该状态值供"状态查询"语义完备性保留, 不作为 poll 返回值)。
 *
 * 重发与终态后隔离:
 *  - 超时 → 重发同一请求 ≤ `cfg.retry_max` 次 → 仍无应答则 `ET_MB_TIMEOUT`;
 *  - 应答地址不符 / CRC 坏 / 功能码不符 → 丢弃该字节并计数, **继续等至超时**;
 *  - **终态后的迟到/陈旧字节 → 丢弃并计数(`late`), 不得污染下一事务**:
 *    非 BUSY 状态下 `feed()` 的字节一律不入缓冲; 且每次 `read`/`write`
 *    发起新事务时接收缓冲清零。
 *
 * 并发约定: 全部 API 仅限 🏠MAIN(与 et_modbus 同: ISR 只往 ringbuf 写)。
 */
#ifndef ET_MODBUS_MASTER_H
#define ET_MODBUS_MASTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_MODBUS

#include "et_modbus.h"      /* HC-5: 功能码/异常码/数量边界/ADU 上限单一来源 */

#ifdef __cplusplus
extern "C" {
#endif

/* 事务状态 (成员顺序即契约, 见文件头"状态语义") */
typedef enum {
    ET_MB_IDLE = 0,         /* 无在途事务 */
    ET_MB_BUSY,             /* 等应答(或待重发) */
    ET_MB_OK,               /* 正常应答 */
    ET_MB_EXC,              /* 异常应答(不重试) */
    ET_MB_TIMEOUT,          /* 超时且重发耗尽 */
    ET_MB_REJECT            /* 组帧参数非法/未初始化 */
} et_mb_status_t;

typedef struct {
    uint16_t addr;              /* 目标从站 1~247; 0 = 广播(仅写, 不等应答) */
    uint32_t resp_timeout_ms;   /* 应答超时(调用方按帧长+3.5字符换算; 必须 > 0) */
    uint8_t  retry_max;         /* 超时重发次数上限(0 = 不重发) */
} et_modbus_master_cfg_t;

/* 只读统计 (计数口径见各字段注释; 口径细节由 API_GUIDE 5.8 固定并测) */
typedef struct {
    uint32_t requests;          /* 已上线请求帧数(**含重发**: 每次 sent() 计一次) */
    uint32_t responses;         /* CRC 通过且地址/功能码/回显匹配的正常应答数 */
    uint32_t exceptions;        /* 异常应答数(终态 ET_MB_EXC 的成因) */
    uint32_t timeouts;          /* 终态为 ET_MB_TIMEOUT 的事务数(非超时次数) */
    uint32_t retries;           /* 重发次数(不含首帧) */
    uint32_t crc_err;           /* CRC 校验失败被丢弃的候选帧数 */
    uint32_t addr_mismatch;     /* 地址不符被丢弃的候选帧数 */
    uint32_t late;              /* 非 BUSY 状态下被丢弃的喂入突发数(陈旧字节) */
    uint32_t discarded;         /* 其它丢弃: 功能码不符/长度非法/缓冲溢出/回显不符 */
} et_modbus_master_stats_t;

typedef struct {
    et_modbus_master_cfg_t cfg;     /* 初始化副本, 勿动 */
    uint8_t  *tx;                   /* 调用方 TX 请求缓冲, 勿动 */
    uint32_t  txcap;                /* TX 容量, 勿动 */
    uint32_t  txlen;                /* 请求帧长(含 CRC), 勿动 */
    uint8_t  *rx;                   /* 调用方 RX 组装缓冲, 勿动 */
    uint32_t  rxcap;                /* RX 容量, 勿动 */
    uint32_t  rxlen;                /* 已组装字节数, 勿动 */
    uint32_t  reslen;               /* 读应答寄存器数据字节数(0=无), 勿动 */
    uint8_t   exp_fc;               /* 本事务请求功能码, 勿动 */
    uint16_t  exp_qty;              /* 本事务请求寄存器数, 勿动 */
    uint8_t   state;                /* et_mb_status_t, 勿动 */
    uint8_t   retries;              /* 已重发次数, 勿动 */
    uint8_t   exc;                  /* 应答异常码, 勿动 */
    bool      pending;              /* 有请求待上线(即 tx() 返回非 NULL), 勿动 */
    bool      on_wire;              /* 已上线且未超时(等应答计时中), 勿动 */
    bool      inited;               /* 已初始化, 勿动 */
    uint32_t  sent_ms;              /* 上次上线时刻, 勿动 */
    et_modbus_master_stats_t stats; /* 统计, 勿动 */
} et_modbus_master_t;

/* 初始化: addr ≤ 247; resp_timeout_ms > 0; rxcap 与 txcap 均须
 * ≥ ET_MODBUS_ADU_MAX(硬底线)。缓冲为调用方持有的普通字节数组(零分配)。 */
bool et_modbus_master_init(et_modbus_master_t *m, const et_modbus_master_cfg_t *cfg,
                           uint8_t *rxbuf, uint32_t rxcap,
                           uint8_t *txbuf, uint32_t txcap);

/* 发起读事务(功能码仅 ET_MODBUS_FC_READ_HOLDING / ET_MODBUS_FC_READ_INPUT;
 * qty 1~ET_MODBUS_RD_QTY_MAX; 广播地址 0 对读无效)。成功 = 请求已组帧待发。
 * 仅在上一次事务不在途时可用(单事务语义), 否则返回 false。 */
bool et_modbus_master_read(et_modbus_master_t *m, uint8_t fc, uint16_t reg, uint16_t qty);

/* 发起写事务(功能码仅 ET_MODBUS_FC_WRITE_SINGLE(须 qty==1) /
 * ET_MODBUS_FC_WRITE_MULTIPLE; qty 1~ET_MODBUS_WR_QTY_MAX; vals 每寄存器 2B 高字节在前)。
 * addr==0 为广播写: sent() 后立即终态 OK, 不等应答。 */
bool et_modbus_master_write(et_modbus_master_t *m, uint8_t fc, uint16_t reg,
                            const uint16_t *vals, uint16_t qty);

/* 取待上线请求(含 CRC); 无待发请求返回 NULL 且 *len = 0。
 * 返回非 NULL 即"必须上线"(首次组帧或超时重发), 上线后调用 _sent()。 */
const uint8_t *et_modbus_master_tx(const et_modbus_master_t *m, uint32_t *len);

/* 声明请求已上线 → 起等应答计时; 广播写在此直接进终态 OK(不等应答) */
void et_modbus_master_sent(et_modbus_master_t *m, uint32_t now_ms);

/* 喂入接收字节; 返回本次消耗的完整应答帧数(0 或 1)。
 * 非 BUSY(无在途事务)时喂入的字节按陈旧字节丢弃并计入 late。 */
uint32_t et_modbus_master_feed(et_modbus_master_t *m, const uint8_t *data, uint32_t len);

/* 周期调用: 推进状态机 —— 超时则置重发待发(tx() 会再返回请求)或进终态 TIMEOUT */
et_mb_status_t et_modbus_master_poll(et_modbus_master_t *m, uint32_t now_ms);

/* 读事务结果: 返回应答中**首个寄存器值**(无结果时 0); *qty 收寄存器个数(可为 NULL)。
 * 多寄存器读的全部值经 _data() 取用。 */
uint16_t et_modbus_master_result(const et_modbus_master_t *m, uint16_t *qty);

/* 读事务应答的寄存器原始字节(每寄存器 2B, 高字节在前); 无结果返回 NULL 且 *len = 0。
 * 数据位于内部 RX 缓冲(调用方持有), 在发起下一事务并喂入新字节前有效。 */
const uint8_t *et_modbus_master_data(const et_modbus_master_t *m, uint32_t *len);

/* 最近一次异常应答的异常码(ET_MB_EXC 时有效, 其余 0) */
uint8_t et_modbus_master_exc(const et_modbus_master_t *m);

/* 只读统计快照 */
void et_modbus_master_stats(const et_modbus_master_t *m, et_modbus_master_stats_t *st);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_MODBUS */
#endif /* ET_MODBUS_MASTER_H */

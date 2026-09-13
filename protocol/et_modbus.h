/**
 * @file    et_modbus.h
 * @brief   Modbus RTU 从站 (字节流喂入 + 主循环处理, 零分配)
 *
 * 定位:
 *  - 协议层第一个**标准应用协议**: 工业现场最常见的 Modbus RTU 从站 ——
 *    复用 et_crc16_modbus(ET_CRC_TABLE=1 时走查表)、字节流状态机范式
 *    (与 et_frame/et_xmodem 同构: ISR 只入 ringbuf, 主循环喂处理);
 *  - 库只做**协议与访问控制**; 寄存器**值域语义**(保持/输入寄存器、32 位双
 *    寄存器组合、浮点寄存器)由应用经 rd/wr 钩子定义 —— 见 API_GUIDE 配方;
 *  - 零动态内存: RX 组装缓冲与 TX 应答缓冲均由调用方持有。
 *
 * 支持范围(v2.4 决议):
 *  - 功能码 0x03/0x04(读保持/输入寄存器)、0x06/0x10(写单/多寄存器)——
 *    覆盖现场 90% 场景; 其余功能码回异常 0x01;
 *  - 异常码 0x01 非法功能 / 0x02 非法地址 / 0x03 非法数据值;
 *  - 广播(从站地址 0): **写执行、不应答**; 读忽略(无应答无动作);
 *  - **不做** Modbus TCP / ASCII / 主站模式(主站记 v2.5 候选)。
 *
 * 帧判定(双路径, 文档化):
 *  - **快路径**: 已知功能码由长度域驱动(0x03/0x04/0x06 定长 8;
 *    0x10 = 9 + bytecount), 收齐即处理 —— 低延迟;
 *  - **静默路径**: 未知功能码/畸形帧由帧间静默界定 —— 调用方按波特率换算
 *    3.5 字符时间写入 `cfg.silence_ms`, 周期调用 `et_modbus_tick(now_ms)`;
 *    缓冲内容在两次 tick 间无变化且累计静默满阈值时, 按完整帧做 CRC 兜底处理
 *    (CRC 无效 → 计入 crc_err 丢弃)。**不内部取时基**(库惯例)。
 *  - CRC16-MODBUS 线上顺序 **低字节在前**(与 MODBUS 规范/帧层约定一致)。
 *
 * 缓冲与粘包:
 *  - `rxbuf` 组装请求, `txbuf` 承载应答 —— **独立 TX 缓冲**是粘包安全的
 *    前提(应答构建不得覆盖缓冲中尚未处理的后续帧, 评审决议);
 *  - `feed()` 处理到"产生一个应答"即停, 缓冲中的后续帧留待下次 `feed(NULL,0)`
 *    (应答取走后继续排空) —— 语义确定、可测;
 *  - 缓冲溢出(请求长于 rxcap) → 丢整批 + `discarded++` 并重新同步。
 *
 * 并发约定: 全部 API 仅限 🏠MAIN(与 et_frame 同: ISR 只往 ringbuf 写)。
 */
#ifndef ET_MODBUS_H
#define ET_MODBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_MODBUS

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 功能码与异常码 ---- */
#define ET_MODBUS_FC_READ_HOLDING   0x03u
#define ET_MODBUS_FC_READ_INPUT     0x04u
#define ET_MODBUS_FC_WRITE_SINGLE   0x06u
#define ET_MODBUS_FC_WRITE_MULTIPLE 0x10u

#define ET_MODBUS_EXC_ILLEGAL_FUNC  0x01u
#define ET_MODBUS_EXC_ILLEGAL_ADDR  0x02u
#define ET_MODBUS_EXC_ILLEGAL_VALUE 0x03u

#define ET_MODBUS_RD_QTY_MAX        125u    /* 读寄存器数上限(250 数据字节) */
#define ET_MODBUS_WR_QTY_MAX        123u    /* 写寄存器数上限(246 数据字节) */
#define ET_MODBUS_ADU_MAX           256u    /* RTU ADU 上限(1+1+2+2+1+246+2) */

/* 读钩子: 把 [addr, addr+qty) 的 qty 个寄存器写入 dst(每寄存器 2B, 高字节在前);
 * 返回 0 = OK, 否则返回 Modbus 异常码(典型 0x02 非法地址)。可 NULL(读一律 0x02)。 */
typedef uint8_t (*et_modbus_rd_fn)(void *user, uint8_t func, uint16_t addr,
                                   uint16_t qty, uint8_t *dst);

/* 写钩子: 把 src(每寄存器 2B, 高字节在前)的 qty 个寄存器写入 [addr, addr+qty);
 * qty==1 表示 0x06 单寄存器写。返回 0 = OK, 否则返回异常码。可 NULL(写一律 0x02)。 */
typedef uint8_t (*et_modbus_wr_fn)(void *user, uint8_t func, uint16_t addr,
                                   uint16_t qty, const uint8_t *src);

typedef struct {
    uint8_t  slave_addr;        /* 本从站地址, 1~247 */
    uint16_t silence_ms;        /* 帧间静默阈值(3.5 字符 @ 波特率, 见文件头) */
    void    *user;              /* 透传 rd/wr */
    et_modbus_rd_fn rd;         /* 读钩子, 可 NULL */
    et_modbus_wr_fn wr;         /* 写钩子, 可 NULL */
} et_modbus_cfg_t;

/* 只读统计 */
typedef struct {
    uint32_t frames;            /* CRC 通过且地址匹配(或广播)的请求帧数 */
    uint32_t responses;         /* 已生成应答帧数(异常应答计入) */
    uint32_t exceptions;        /* 其中异常应答数 */
    uint32_t crc_err;           /* CRC 校验失败帧(静默丢弃) */
    uint32_t addr_mismatch;     /* 地址不符帧(静默丢弃) */
    uint32_t discarded;         /* 缓冲溢出/过短残帧等强制丢弃 */
} et_modbus_stats_t;

typedef struct {
    et_modbus_cfg_t cfg;        /* 初始化副本, 勿动 */
    uint8_t  *rx;               /* 调用方 RX 组装缓冲, 勿动 */
    uint32_t  rxcap;            /* RX 容量, 勿动 */
    uint32_t  rxlen;            /* 已组装字节数, 勿动 */
    uint8_t  *tx;               /* 调用方 TX 应答缓冲, 勿动 */
    uint32_t  txcap;            /* TX 容量, 勿动 */
    uint32_t  resplen;          /* 待发应答长度(0=无), 勿动 */
    uint32_t  last_ms;          /* 上次 tick 时刻, 勿动 */
    uint32_t  last_rxlen;       /* 上次 tick 时 rxlen(静默判据), 勿动 */
    uint32_t  idle_ms;          /* 两次 tick 间无新字节的累计时长, 勿动 */
    et_modbus_stats_t stats;    /* 统计, 勿动 */
} et_modbus_t;

/* 初始化: slave_addr 1~247; rxcap ≥ 8; txcap ≥ 8; rd/wr 可 NULL(对应操作回 0x02)。
 * buf 均为调用方持有的普通字节数组(零分配)。 */
bool et_modbus_init(et_modbus_t *mb, const et_modbus_cfg_t *cfg,
                    uint8_t *rxbuf, uint32_t rxcap,
                    uint8_t *txbuf, uint32_t txcap);

/* 喂入接收字节; 返回本次生成的应答帧数(0 或 1)。
 * 传 (NULL,0) = 继续排空缓冲中尚未处理的后续帧(应答取走后调用)。 */
uint32_t et_modbus_feed(et_modbus_t *mb, const uint8_t *data, uint32_t len);

/* 取待发应答(含 CRC; 无应答返回 NULL 且 *len=0) */
const uint8_t *et_modbus_response(const et_modbus_t *mb, uint32_t *len);

/* 周期调用: 静默超时界定残帧(未知功能码/畸形帧的应答依赖本调用) */
void et_modbus_tick(et_modbus_t *mb, uint32_t now_ms);

/* 只读统计快照 */
void et_modbus_stats(const et_modbus_t *mb, et_modbus_stats_t *st);

/* 已组装未处理的请求字节数(0 = 无半帧)。共享串口分流场景用: 收到首字节后
 * 若本值 > 0, 说明当前突发是上一帧的延续, 应继续喂给从站而非 shell。 */
uint32_t et_modbus_rx_pending(const et_modbus_t *mb);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_MODBUS */
#endif /* ET_MODBUS_H */

/**
 * @file    et_xmodem_tx.h
 * @brief   XMODEM-CRC 发送器 (v1.8, 与接收器 et_xmodem 对称)
 *
 * 定位: MCU 作发送方 (对传/镜像上报/日志回传)。与接收器共享协议常量
 * (ET_XM_SOH/STX/EOT/ACK/NAK/CAN、et_xmodem_crc16) —— 单一事实来源防漂移。
 *
 * 协议时序 (与 et_xmodem.c 接收器严格对齐):
 *   1. 起步: 等对端 NAK/'C' (poll 喂入) → 发块 1;
 *   2. 块: SOH+块号+~块号+128B+CRC16(高字节在前); 尾块 0x1A 填充至块型;
 *   3. 应答: ACK → 下一块; NAK → 重发当前块 (≤ retry_max, 超限 ERR);
 *   4. 尾: 全部确认后 EOT 两段确认 (EOT→NAK→EOT→ACK→DONE);
 *   5. 对端 CAN×2 → 中止; 应答超时 (tick) → 重发当前步骤。
 *
 * 数据源: src(user, off, dst, want) 返回实际读取字节数 —— 顺序只读,
 * 短读/0 表示数据不足, 发送端以 0x1A 补齐块型 (与 tools/xmodem_send.py 一致)。
 *
 * 并发策略: 全部 API 仅限 🏠MAIN; putc/src 回调在 poll/tick 内同步执行。
 */
#ifndef ET_XMODEM_TX_H
#define ET_XMODEM_TX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"
#include "et_xmodem.h"

#if ET_MODULE_XMODEM

#ifdef __cplusplus
extern "C" {
#endif

/* 数据源: 读 off 起 want 字节到 dst, 返回实际字节数 (可 < want, 0=数据尽) */
typedef uint32_t (*et_xmodem_src_fn)(void *user, uint32_t off,
                                     uint8_t *dst, uint32_t want);

/* 线路输出: 阻塞式单字节 (帧字节经此发出) */
typedef void (*et_xmodem_putc_fn)(void *user, uint8_t b);

typedef struct {
    et_xmodem_putc_fn putc;         /* 线路输出, 必填                    */
    et_xmodem_src_fn   src;         /* 载荷数据源, 必填                  */
    void              *user;        /* 透传 putc/src                     */
    uint32_t           total;       /* 待发总字节数 (≥1)                 */
    uint32_t           block_size;  /* 128; ET_XM_1K=1 时可 1024         */
    uint32_t           retry_max;   /* 单步重传上限 (建议 10)            */
    uint32_t           ack_timeout_ms; /* 应答超时 (建议 1000)           */
} et_xmodem_tx_cfg_t;

typedef struct et_xmodem_tx {
    et_xmodem_putc_fn putc;         /* 内部状态, 勿动 */
    et_xmodem_src_fn   src;
    void              *user;
    uint32_t           total;
    uint32_t           block_size;
    uint32_t           retry_max;
    uint32_t           ack_timeout_ms;
    uint8_t            state;       /* 内部状态机状态 */
    uint8_t            blk;         /* 当前块号 (1..255 环回) */
    uint32_t           sent;        /* 已确认字节数 */
    uint32_t           retries;     /* 当前步重传计数 */
    uint32_t           t_mark;      /* 当前步起始时刻 */
    bool               eot2;        /* EOT 第二段标志 */
    bool               done;
    bool               aborted;
} et_xmodem_tx_t;

/* 初始化: 校验 cfg 与块型 (1024 仅 ET_XM_1K=1); total ≥ 1。🏠MAIN */
bool et_xmodem_tx_init(et_xmodem_tx_t *x, const et_xmodem_tx_cfg_t *cfg);

/* 喂入对端应答字节: ACK 推进/NAK 重发/CAN 中止/其余忽略。
 * 返回动作语义: ACK=当前块确认并推进, NAK=已重发, CAN=中止,
 * DONE=传输完成, IDLE=无关字节, ERR=重传超限。🏠MAIN */
et_xm_act_t et_xmodem_tx_poll(et_xmodem_tx_t *x, uint8_t ch, uint32_t now);

/* 无应答驱动: 应答超时 → 重发当前块/EOT (返回 NAK 语义); 未超时 IDLE。
 * 调用节奏不限 (通常与 poll 同循环)。🏠MAIN */
et_xm_act_t et_xmodem_tx_tick(et_xmodem_tx_t *x, uint32_t now);

/* 状态查询: done = 传输完成; aborted = 已中止 (此后 poll/tick 恒 ERR/DONE) */
bool et_xmodem_tx_done(const et_xmodem_tx_t *x);
bool et_xmodem_tx_aborted(const et_xmodem_tx_t *x);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_XMODEM */
#endif /* ET_XMODEM_TX_H */

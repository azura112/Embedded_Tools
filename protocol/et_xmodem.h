/**
 * @file    et_xmodem.h
 * @brief   XMODEM-CRC 接收器 (固件/镜像传输, bootloader 拉取场景)
 *
 * 定位:
 *  - 接收器为核: 对端(上位机/发送脚本)发 SOH/STX 块, 本模块逐字节喂入,
 *    返回应答动作(ACK/NAK/CAN)由调用方写回线路;
 *  - CRC 复用 et_crc16_ccitt_update(): XMODEM-CRC 与 CCITT-FALSE 仅初始值
 *    之差(0x0000 vs 0xFFFF), 流式种子天然支持, 零新增 CRC 变体;
 *  - 超时/重试由调用方注入 now(host 虚拟时间可全速测试), 模块内部不取
 *    时基 —— 与 stimer/sched 同风格;
 *  - ET_XM_1K=1 使能 1024B 大块(STX), 默认关(仅 SOH/128B);
 *  - sink 回调把连续收到的数据交给调用方(写 flash 走 port_flash_write,
 *    或 RAM); sink 拒绝(返回 false)则立即中止传输。
 *
 * 会话时序 (协议约定):
 *   1. 调用方 rx_init 后循环 rx_tick: 每隔 1s 无块 → 返回 ET_XM_NAK
 *      (调用方发 NAK 催块; 如对端要求 'C' 启动, 由调用方自行改发 'C');
 *   2. 收满一块: 块号+补码+CRC16 全对 → ET_XM_ACK 并 sink; CRC 坏 → NAK;
 *      重复块(上一块重发) → ACK 不重复 sink; 失序 → CAN 中止;
 *   3. EOT 两段确认: 第一个 EOT → NAK, 第二个 EOT → ACK + ET_XM_DONE;
 *   4. 10s 静默(无任何字节) → ET_XM_ERR 一次, 会话自动复位可再来;
 *   5. CAN×2 或 sink 拒绝 → ET_XM_CAN 中止, 此后 rx/rx_tick 恒 ET_XM_ERR
 *      (须重新 init)。
 *
 * bootloader 数据通路 (与 port_flash_write 组接):
 *   UART RX ──字节──> et_xmodem_rx() ──整块──> sink 回调
 *       sink 内部: 首块擦除目标扇区 → port_flash_write(offset) → 返回 true
 *   应答动作 ──UART TX── 对端
 *
 * 并发约定: 单上下文模块(🏠MAIN); rx/rx_tick 不得并发调用。
 */
#ifndef ET_XMODEM_H
#define ET_XMODEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_XMODEM

/* 1K 大块(STX)开关: 默认关; 打开后 cap 须 >= 1028 */
#ifndef ET_XM_1K
#define ET_XM_1K                0
#endif

/* 会话时序参数(ms), 可 -D 覆盖 */
#ifndef ET_XM_PROMPT_MS
#define ET_XM_PROMPT_MS         1000u   /* 无块催块周期 */
#endif
#ifndef ET_XM_SILENCE_MS
#define ET_XM_SILENCE_MS        10000u  /* 静默放弃阈值 */
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ET_XM_SOH               0x01u   /* 128B 块头   */
#define ET_XM_STX               0x02u   /* 1024B 块头  */
#define ET_XM_EOT               0x04u   /* 传输结束    */
#define ET_XM_ACK_BYTE          0x06u
#define ET_XM_NAK_BYTE          0x15u
#define ET_XM_CAN_BYTE          0x18u

#define ET_XM_BLK128            128u    /* 基本块载荷 */
#define ET_XM_HDR_RAW           4u      /* 块号+补码+CRC16 的线级字节数 */

typedef enum {
    ET_XM_ACK = 0,      /* 回 ACK: 块收妥(或重复块) */
    ET_XM_NAK,          /* 回 NAK: CRC 坏/催块/补码错 */
    ET_XM_CAN,          /* 回 CAN: 对端取消/sink 拒绝/失序 —— 会话中止 */
    ET_XM_IDLE,         /* 无动作 */
    ET_XM_DONE,         /* 传输完成(EOT 二段确认后) */
    ET_XM_ERR           /* 会话已中止/静默放弃 */
} et_xm_act_t;

/* 接收数据出口: offset 为自会话起的累计字节; 返回 false = 拒绝(中止) */
typedef bool (*et_xm_sink_fn)(void *user, uint32_t offset,
                              const uint8_t *data, uint32_t len);

typedef struct et_xmodem {
    uint8_t        *buf;        /* 单块暂存(调用方提供), 勿动 */
    uint32_t        cap;        /* 暂存容量, 勿动 */
    et_xm_sink_fn   sink;       /* 数据出口, 勿动 */
    void           *user;       /* sink 透传, 勿动 */

    /* 内部状态, 勿动 */
    uint8_t         state;      /* WAIT / DATA / EOT1 */
    uint16_t        pos;        /* 块内已收字节数(含块号对) */
    uint16_t        blk_len;    /* 本块线级总长 = 4 + payload */
    uint8_t         expect;     /* 下一期望块号(1..255→0→1) */
    uint8_t         last_blk;   /* 上一收妥块号(重复判定) */
    bool            has_last;
    uint32_t        total;      /* sink 累计偏移 */
    bool            aborted;    /* CAN/sink 拒绝后的终态 */
    uint32_t        t_act;      /* 最后字节时刻(静默判定) */
    uint32_t        t_prompt;   /* 上次催块时刻 */
    bool            has_act;
    uint8_t         can_cnt;    /* 连续 CAN 计数 */
} et_xmodem_t;

/* 初始化: buf 为单块暂存, 容量须 >= 132(开 1K 须 >= 1028);
 * 不满足或空指针时实例进入 ERR 终态(调用方可检查首次 rx 返回)。🏠MAIN */
void et_xmodem_rx_init(et_xmodem_t *x, uint8_t *buf, uint32_t cap,
                       et_xm_sink_fn sink, void *user);

/* 逐字节喂入(线路字节), 返回应答动作(调用方按动作回写线路) 🏠MAIN */
et_xm_act_t et_xmodem_rx(et_xmodem_t *x, uint8_t ch, uint32_t now);

/* 超时驱动: 主循环周期调用, 返回催块/放弃动作 🏠MAIN */
et_xm_act_t et_xmodem_rx_tick(et_xmodem_t *x, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_XMODEM */
#endif /* ET_XMODEM_H */

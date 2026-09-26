/**
 * @file    port_ch32x035.h
 * @brief   CH32X035 平台适配层 - port.h 契约之外的平台接口
 *
 * 时钟假设: 复位后默认 HSI 48MHz(PD BMC 时序要求, 应用不得改配),
 *   SystemCoreClock = 48000000。
 *
 * 平台说明:
 *  - 1ms 时基 = TIM1 更新中断(48MHz/48/1000, 与 V1.7 逐配置相同), 由
 *    port_ch32x035_tick_init() 配置, 中断服务程序在 port_ch32x035.c 内:
 *      - 累加 port.h 的 32 位毫秒时基 port_tick_get_ms();
 *      - 同步累加 8 位旧制计数器 Tim_Ms_Cnt(PD 协议栈兼容, 见下);
 *  - ⚠ SysTick 不可作时基: WCH debug.c 的 Delay_Us/Delay_Ms 以轮询方式
 *    独占 SysTick(每次调用重写 CMP/CTLR)。时基放 SysTick 会被延时调用
 *    改频/互踩(实机教训: 复位循环)。SysTick 完整保留给延时函数。
 *  - Tim_Ms_Cnt: WCH EVT "USBPD_SNK" 协议栈的 8 位毫秒计数器, PD_Process.c
 *    以"上一值 + 增量"方式消费。本 port 在同一 TIM1 中断里替应用维护它,
 *    应用只需每轮主循环计算 Tmr_Ms_Dlt = Tim_Ms_Cnt - Tmr_Ms_Cnt_Last
 *    (8 位无符号回绕自动正确, 单字节读取天然原子)。
 *  - 临界区语义: mstatus.MIE/MPIE 保存恢复 + 嵌套计数(与 F103 port 的
 *    PRIMASK 方案同构);
 *  - 看门狗: IWDG, LSI 典型 ~47kHz, 超时 = (RLDR+1)*分频/LSI。
 *    ⚠ PSCR/RLDR 在 LSI 时钟域, 写入需 ~2 LSI 周期同步 —— 必须先等
 *    PVU/RVU 清零再 Enable, 否则计数器从错误暂态值起跑会造成秒级
 *    复位风暴(本板实机教训, 见 README)。
 *  - flash 参数区三件套未实现: ET_MODULE_KV=0 构建不需要; 如需启用 et_kv,
 *    参照 port/stm32f103/ 补 port_flash_* 并自行评估 62K flash 的分区。
 */
#ifndef PORT_CH32X035_H
#define PORT_CH32X035_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PD 协议栈兼容的 8 位毫秒计数器(TIM1 中断累加, 读取方: 应用主循环) */
extern volatile uint8_t Tim_Ms_Cnt;

/* 配置 TIM1 为 1ms 时基并使能更新中断(48MHz/48/1000, 同 V1.7)。
 * 须在首个 et_sched_register() 之前调用(注册时刻锚定首轮周期)。
 * 依赖 main() 已先行调用 NVIC_PriorityGroupConfig(与 V1.7 顺序一致)。 */
void port_ch32x035_tick_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CH32X035_H */

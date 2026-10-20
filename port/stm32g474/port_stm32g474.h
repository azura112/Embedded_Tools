/**
 * @file    port_stm32g474.h
 * @brief   STM32G474 平台初始化接口 (port.h 契约之外的平台辅助)
 */
#ifndef PORT_STM32G474_H
#define PORT_STM32G474_H

#ifdef __cplusplus
extern "C" {
#endif

/* 使能外设时钟 + 配置 LED/按键/USART1 引脚 + 时钟升至 144MHz + 启动 SysTick 1ms。
 * main 里最先调用; USART1 已配置为 115200-8-N-1 供日志输出。
 * 若 PLL 未就绪(异常), 自动回退 HSI16 16MHz, 串口/时基仍保持正确。
 * 另: 复位后校验 FLASH_OPTR.DBANK=1(出厂默认), 不满足则打印错误并停机 ——
 * 本移植的 flash 几何(2KB 页)以双 bank 模式为前提。 */
void port_stm32g474_init(void);

/* ===================== 分区表几何 (v2.29 P2-1/P2-3, REQ-7) =====================
 * 放在平台头而非 .c: 消费侧(板侧 demo 的 `et_bootctl_cfg_t.slot_base`)与 port 内部的
 * 区域表读**同一处**——两处各写一份就是双源, 迟早漂移。全部带 #ifndef, 可 -D 覆盖。
 * 位形与容量算式(64+64+2+32 = 162 KB ≤ 512 KB)见 port_stm32g474.c 区域段头注。
 */
#ifndef PORT_G474_SLOT_A_BASE
#define PORT_G474_SLOT_A_BASE  0x08000000u   /* A 槽 = 当前在跑的应用 */
#endif
#ifndef PORT_G474_SLOT_B_BASE
#define PORT_G474_SLOT_B_BASE  0x08010000u   /* B 槽 = 新镜像落地处 */
#endif
#ifndef PORT_G474_STATE_BASE
#define PORT_G474_STATE_BASE   0x08020000u   /* 平台声明的状态页 */
#endif
#ifndef PORT_G474_SLOT_SIZE
#define PORT_G474_SLOT_SIZE    (64u * 1024u) /* 单槽容量(链接脚本同值收口) */
#endif
#ifndef PORT_G474_STATE_SIZE
#define PORT_G474_STATE_SIZE   2048u
#endif

#ifdef __cplusplus
}
#endif

#endif /* PORT_STM32G474_H */

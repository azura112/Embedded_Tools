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

#ifdef __cplusplus
}
#endif

#endif /* PORT_STM32G474_H */

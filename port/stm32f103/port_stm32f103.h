/**
 * @file    port_stm32f103.h
 * @brief   STM32F103 平台初始化接口 (port.h 契约之外的平台辅助)
 */
#ifndef PORT_STM32F103_H
#define PORT_STM32F103_H

#ifdef __cplusplus
extern "C" {
#endif

/* 使能外设时钟 + 配置 LED/USART1 引脚 + 启动 SysTick 1ms。
 * main 里最先调用; USART1 已配置为 115200-8-N-1 供日志输出。 */
void port_stm32f103_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_STM32F103_H */

/********************************** (C) COPYRIGHT *******************************
 * File Name          : uart_cmd.h
 * Description        : UART command channel (COM16 / USART1 PB10-PB11, H1.4/H1.5).
 *
 *                      V1.8 起:单字符命令(n/s/o/... )改由调试串口 RX 接收,
 *                      USB CDC 退为纯数据上报(TX)通道。
 *                      ISR 把收到的字节推入 et_ringbuf,10ms 任务解析后经
 *                      et_queue 交给主循环执行(与原 CDC 命令路径同构)。
 *******************************************************************************/
#ifndef USER_UART_CMD_H_
#define USER_UART_CMD_H_

#include "debug.h"

/* 须在 USART_Printf_Init() 之后调用(波特率/时钟/UE 已就绪):
 * 打开 USART1 接收器 + RXNE 中断, 使能 NVIC。 */
void UART_Cmd_Init(void);

/* 每 ~10ms 调用: 把 RX 环字节解析进命令队列(主循环上下文) */
void UART_Cmd_Task(void);

/* 弹出一条待执行命令, 0 = 无 */
uint8_t UART_Cmd_Read(void);

/* 已收字节数(诊断) */
uint32_t UART_Cmd_RxTotal(void);

#endif /* USER_UART_CMD_H_ */

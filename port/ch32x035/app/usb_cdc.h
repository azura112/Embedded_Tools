/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.h
 * Description        : Board CDC-ACM application layer for the PD test board.
 *
 *                      The board enumerates as a virtual COM port (CDC-ACM,
 *                      WCH VID/PID, driver-free on Windows 10+/Linux/macOS).
 *                      Device -> host: status lines every 500ms plus on-demand
 *                      reports (VBUS=/SWEEP* CSV, same bytes as the UART).
 *
 *                      (V1.8: 命令输入已全部移至调试串口 COM16/USART1,
 *                       见 uart_cmd.h —— CDC 为纯数据上报(TX)通道,
 *                       RX 字节排空丢弃。)
 *******************************************************************************/
#ifndef USER_USB_CDC_H_
#define USER_USB_CDC_H_

#include "debug.h"

/* line coding reported to the host (115200-8-N-1, logical only) */
extern volatile uint8_t CDC_Line_Coding[7];

void    CDC_Init(void);                 /* USBFS RCC + device init          */
void    CDC_Task(void);                 /* call ~10ms: drain RX, flush TX   */
uint8_t CDC_Connected(void);            /* 1 if host configured the device  */
void    CDC_Write(const void *p, uint16_t len);   /* queue bytes to host    */
void    CDC_Printf(const char *fmt, ...);         /* printf to host         */
uint32_t CDC_RxTotal(void);             /* bytes received since boot (diag)   */

/* ISR-context hooks used by ch32x035_usbfs_device.c */
void    CDC_Rx_Write(const uint8_t *p, uint8_t len);
void    CDC_On_Reset(void);

#endif /* USER_USB_CDC_H_ */

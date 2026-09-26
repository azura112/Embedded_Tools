/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.h
 * Description        : Board CDC-ACM application layer for the PD test board.
 *
 *                      The board enumerates as a virtual COM port (CDC-ACM,
 *                      WCH VID/PID, driver-free on Windows 10+/Linux/macOS).
 *                      Device -> host: status lines every 500ms plus on-demand
 *                      reports. Host -> device: single-letter ASCII commands,
 *                      executed in the main loop (see App_Command in main.c):
 *                        n     next target PDO (same as KEY0)
 *                        1..8  select target PDO index
 *                        b     cycle backlight (same as KEY1)
 *                        s     full status dump (same as KEY2)
 *                        o     cycle LCD orientation (MADCTL bring-up tuner)
 *                        c     toggle LCD red/blue order (MADCTL BGR bit)
 *                        ?     help
 *
 *                      (et 重构版: 对外接口与 V1.7 逐签名一致; 内部 TX/RX
 *                       环改为 et_ringbuf, 命令队列改为 et_queue)
 *******************************************************************************/
#ifndef USER_USB_CDC_H_
#define USER_USB_CDC_H_

#include "debug.h"

/* line coding reported to the host (115200-8-N-1, logical only) */
extern volatile uint8_t CDC_Line_Coding[7];

void    CDC_Init(void);                 /* USBFS RCC + device init          */
void    CDC_Task(void);                 /* call ~10ms: parse RX, flush TX   */
uint8_t CDC_Connected(void);            /* 1 if host configured the device  */
void    CDC_Write(const void *p, uint16_t len);   /* queue bytes to host    */
void    CDC_Printf(const char *fmt, ...);         /* printf to host         */
uint8_t CDC_Read_Cmd(void);             /* pop pending command, 0 = none    */
uint32_t CDC_RxTotal(void);             /* bytes received since boot (diag)   */

/* ISR-context hooks used by ch32x035_usbfs_device.c */
void    CDC_Rx_Write(const uint8_t *p, uint8_t len);
void    CDC_On_Reset(void);

#endif /* USER_USB_CDC_H_ */

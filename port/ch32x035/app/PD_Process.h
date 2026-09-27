/********************************** (C) COPYRIGHT *******************************
* File Name          : PD_Process.h
* Description        : PD sink protocol, based on WCH CH32X035 EVT USBPD_SNK
*                      example, adapted for the PD test board:
*                        - selectable target PDO (KEY0 cycles through SrcCap)
*                        - contract voltage exported after PS_RDY
*                        - no blocking error loop
*******************************************************************************/
#ifndef USER_PD_PROCESS_H_
#define USER_PD_PROCESS_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32x035_usbpd.h"

/* Variable extents */
extern __IO UINT8  Tmr_Ms_Cnt_Last;
extern __IO UINT8  Tmr_Ms_Dlt;
extern volatile UINT8  Tim_Ms_Cnt;

extern __IO UINT8  PDO_Len;
extern PD_CONTROL PD_Ctl;

extern UINT8 PD_Ack_Buf[];
extern __attribute__ ((aligned(4))) UINT8 PD_Rx_Buf[34];
extern __attribute__ ((aligned(4))) UINT8 PD_Tx_Buf[34];
extern UINT8 Adapter_SrcCap[30];

/* Test board extensions */
extern __IO UINT8   PD_Target_PDO;    /* requested PDO index, starts at 1 (5V) */
extern __IO UINT16  PD_Request_mV;    /* voltage of the last REQUEST sent      */
extern __IO UINT16  PD_Contract_mV;   /* valid contract voltage after PS_RDY   */

/* Function prototypes */
extern void PD_Rx_Mode(void);
extern void PD_SRC_Init(void);
extern void PD_SINK_Init(void);
extern void PD_PHY_Reset(void);
extern void PD_Init(void);
extern UINT8 PD_Detect(void);
extern void PD_Det_Proc(void);
extern void PD_Load_Header(UINT8 ex, UINT8 msg_type);
extern UINT8 PD_Send_Handle(UINT8 *pbuf, UINT8 len);
extern void PD_Phy_SendPack(UINT8 mode, UINT8 *pbuf, UINT8 len, UINT8 sop);
extern void PD_Main_Proc(void);
extern void PD_PDO_Analyse(UINT8 pdo_idx, UINT8 *srccap, UINT16 *current, UINT16 *voltage);
extern void PDO_Request(UINT8 pdo_index);
extern void PDO_Request_Cur(UINT8 pdo_index, UINT16 cur_pct);   /* V1.9 测试钩子 */
extern void PD_Send_HardReset(void);                            /* V1.9 测试钩子 */
extern void PD_Send_SoftReset(void);                            /* V1.9 测试钩子 */
extern void PD_Save_Adapter_SrcCap(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_PD_PROCESS_H_ */

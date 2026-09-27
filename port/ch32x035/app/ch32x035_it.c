/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32x035_it.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/10/28
 * Description        : Main Interrupt Service Routines.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "ch32x035_it.h"
#include <stdio.h>

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
void NMI_Handler(void)
{
    while (1)
  {
  }
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 */
void HardFault_Handler(void)
{
  /* V1.9 诊断: 转储 mcause/mepc 后再复位 —— mcause 低 4 位区分
   * 1=取指访问故障 / 5=Load 访问故障 / 7=Store 访问故障 / 2=非法指令,
   * mepc 即出错指令地址(与 elf/objdump 对位) */
  uint32_t mc, ep;

  __asm__ __volatile__ ("csrr %0, mcause" : "=r"(mc));
  __asm__ __volatile__ ("csrr %0, mepc"   : "=r"(ep));
  printf( "HF! mcause=%08x mepc=%08x\r\n",
          (unsigned)mc, (unsigned)ep );

  NVIC_SystemReset();
  while (1)
  {
  }
}



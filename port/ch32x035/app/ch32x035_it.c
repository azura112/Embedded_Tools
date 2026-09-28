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

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/* HardFault 现场 + 轨迹暂存(RAM 0x20004400, bss 之上/栈之下, 跨软复位保留):
 * [0]=magic [1]=mcause [2]=mepc [3]=轨迹头 [4..35]=轨迹环(32 槽)
 * 启动代码在串口就绪后打印并清除。
 * (不在故障上下文 printf —— flash 故障类 HardFault 中 printf 会再次取指故障) */
#define HF_MAGIC        0xC0DEF00Du
/* 暂存区 = .noinit 段(链接器保留, 不参与 bss 清零/堆分配, 跨软复位保留) */
volatile uint32_t hf_scratch[ 48 ] __attribute__((section(".noinit"), used));
#define HF_SCRATCH      hf_scratch

/* 轨迹点: 主循环上下文每到达一个检查点写入编号(LRU 覆盖) */
void Trace_Push( uint32_t id )  __attribute__((noinline));
void Trace_Push( uint32_t id )
{
    uint32_t h = HF_SCRATCH[ 3 ];
    HF_SCRATCH[ 4 + ( h & 31u ) ] = id;
    HF_SCRATCH[ 3 ] = h + 1u;
}

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
  /* V1.9 诊断: 现场写入 RAM 暂存区后复位(mcause: 1=取指访问故障
   * 5=Load 访问故障 7=Store 访问故障 2=非法指令; mepc=出错指令地址)。
   * 打印由下一轮启动的 main 完成(不在故障上下文触碰 flash) */
  uint32_t mc, ep;

  __asm__ __volatile__ ("csrr %0, mcause" : "=r"(mc));
  __asm__ __volatile__ ("csrr %0, mepc"   : "=r"(ep));

  HF_SCRATCH[ 0 ] = HF_MAGIC;
  HF_SCRATCH[ 1 ] = mc;
  HF_SCRATCH[ 2 ] = ep;

  NVIC_SystemReset();
  while (1)
  {
  }
}



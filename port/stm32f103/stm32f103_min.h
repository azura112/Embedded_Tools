/**
 * @file    stm32f103_min.h
 * @brief   STM32F103 最小寄存器定义 (自包含, 不依赖 CMSIS 设备头)
 *
 * 仅覆盖本移植用到的外设: RCC / GPIOA / GPIOC / USART1 / SysTick / SCB。
 * 工程内已有 CMSIS 设备头时可整体替换为 #include "stm32f1xx.h"。
 */
#ifndef STM32F103_MIN_H
#define STM32F103_MIN_H

#include <stdint.h>

#define REG32(addr)             (*(volatile uint32_t *)(uintptr_t)(addr))

/* ===================== RCC ===================== */
#define RCC_APB2ENR             REG32(0x40021018u)
#define RCC_APB2ENR_AFIOEN      (1u << 0)
#define RCC_APB2ENR_IOPAEN      (1u << 2)
#define RCC_APB2ENR_IOPCEN      (1u << 4)
#define RCC_APB2ENR_USART1EN    (1u << 14)

/* ===================== GPIO ===================== */
#define GPIOA_CRL               REG32(0x40010800u)
#define GPIOA_CRH               REG32(0x40010804u)
#define GPIOA_IDR               REG32(0x40010808u)
#define GPIOA_ODR               REG32(0x4001080Cu)
#define GPIOC_CRH               REG32(0x40011004u)
#define GPIOC_BSRR              REG32(0x40011010u)
#define GPIOC_BRR               REG32(0x40011014u)

/* ===================== USART1 ===================== */
#define USART1_SR               REG32(0x40013800u)
#define USART1_DR               REG32(0x40013804u)
#define USART1_BRR              REG32(0x40013808u)
#define USART1_CR1              REG32(0x4001380Cu)
#define USART_SR_TXE            (1u << 7)
#define USART_SR_RXNE           (1u << 5)
#define USART_CR1_UE            (1u << 13)
#define USART_CR1_TE            (1u << 3)
#define USART_CR1_RE            (1u << 2)

/* ===================== SysTick ===================== */
#define SYST_CSR                REG32(0xE000E010u)
#define SYST_RVR                REG32(0xE000E014u)
#define SYST_CVR                REG32(0xE000E018u)
#define SYST_CSR_ENABLE         (1u << 0)
#define SYST_CSR_TICKINT        (1u << 1)
#define SYST_CSR_CLKSOURCE      (1u << 2)   /* 1=HCLK, 0=HCLK/8 */
#define SYST_CSR_COUNTFLAG      (1u << 16)

/* ===================== SCB ===================== */
#define SCB_SCR                 REG32(0xE000ED10u)
#define SCB_SCR_SLEEPDEEP       (1u << 2)
#define SCB_AIRCR               REG32(0xE000ED0Cu)      /* 应用中断/复位控制 */
#define SCB_AIRCR_SYSRESETREQ   0x05FA0004u             /* VECTKEY | SYSRESETREQ */

/* ===================== FLASH (PM0056) ===================== */
/* 映射: 0x40022000-0x40022013; 仅用中容量 F103 相关的 ACR/KEYR/SR/CR */
#define FLASH_ACR               REG32(0x40022000u)      /* 等待周期/预取 */
#define FLASH_KEYR              REG32(0x40022004u)      /* 解锁键寄存器 */
#define FLASH_SR                REG32(0x4002200Cu)      /* 状态寄存器 */
#define FLASH_CR                REG32(0x40022010u)      /* 控制寄存器 */

#define FLASH_ACR_LATENCY_SHIFT 0u

#define FLASH_SR_BSY            (1u << 0)               /* 忙(编程/擦除中) */
#define FLASH_SR_PGERR          (1u << 2)               /* 编程错误 */
#define FLASH_SR_WRPRTERR       (1u << 4)               /* 写保护错误 */
#define FLASH_SR_EOP            (1u << 5)               /* 操作完成 */

#define FLASH_CR_PG             (1u << 0)   /* 编程使能 */
#define FLASH_CR_PER            (1u << 1)   /* 页擦除使能 */
#define FLASH_CR_MER            (1u << 2)   /* 全片擦除使能 */
#define FLASH_CR_STRT           (1u << 6)   /* 启动擦除 */
#define FLASH_CR_LOCK           (1u << 7)   /* 上锁(保护配置) */

/* PM0056 3.3.4: KEY1=0x45670123, KEY2=0xCDEF89AB 顺序写入 KEYR 解锁 */
#define FLASH_KEY1              0x45670123u
#define FLASH_KEY2              0xCDEF89ABu

/* ===================== IWDG (RM0008 独立看门狗章) ===================== */
/* 映射: 0x40003000-0x4000300C; LSI ≈ 40kHz, 计数时钟 = 40kHz/分频 */
#define IWDG_KR                 REG32(0x40003000u)      /* 键寄存器 */
#define IWDG_PR                 REG32(0x40003004u)      /* 预分频 (0..6 → 4..256) */
#define IWDG_RLR                REG32(0x40003008u)      /* 重装载 (12 位有效) */
#define IWDG_SR                 REG32(0x4000300Cu)      /* 状态: bit0 PVU / bit1 RVU */

#define IWDG_KEY_FEED           0xAAAAu                 /* 喂狗 */
#define IWDG_KEY_UNLOCK         0x5555u                 /* 解锁 PR/RLR 写入 */
#define IWDG_KEY_START          0xCCCCu                 /* 启动(启动后不可停) */
#define IWDG_SR_PVU             (1u << 0)
#define IWDG_SR_RVU             (1u << 1)

#endif /* STM32F103_MIN_H */

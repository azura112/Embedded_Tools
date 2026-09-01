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

#endif /* STM32F103_MIN_H */

/**
 * @file    stm32g474_min.h
 * @brief   STM32G474 最小寄存器定义 (自包含, 不依赖 CMSIS 设备头)
 *
 * 仅覆盖本移植用到的外设: RCC / PWR / GPIOA,C,D / USART1 / SysTick / SCB /
 * FLASH / IWDG。位定义逐一对照 STM32CubeMX G4 包 CMSIS 头
 * (stm32g474xx.h, ST G4 FW 1.x) 与 RM0440 核实, 未用的位不定义。
 * 工程内已有 CMSIS 设备头时可整体替换为 #include "stm32g4xx.h"。
 */
#ifndef STM32G474_MIN_H
#define STM32G474_MIN_H

#include <stdint.h>

#define REG32(addr)             (*(volatile uint32_t *)(uintptr_t)(addr))

/* ===================== 时钟树要点 (与源 CubeMX 工程一致) =====================
 * 复位默认 HSI16 开启 (RCC_CR 复位值 0x100); 时钟初始化按 .ioc 配置:
 * HSI16 /1 *18 /2 = 144MHz (PLLM=1, PLLN=18, PLLR=DIV2), FLASH 4 等待周期。
 */

/* ===================== RCC (RM0440 6) ===================== */
#define RCC_CR                  REG32(0x40020000u)
#define RCC_CFGR                REG32(0x40020008u)
#define RCC_PLLCFGR             REG32(0x4002000Cu)
#define RCC_AHB1ENR             REG32(0x40020048u)
#define RCC_APB1ENR1            REG32(0x40020058u)
#define RCC_APB2ENR             REG32(0x40020060u)

#define RCC_CR_HSION            (1u << 8)
#define RCC_CR_HSIRDY           (1u << 10)
#define RCC_CR_PLLON            (1u << 24)
#define RCC_CR_PLLRDY           (1u << 25)

/* CFGR.SW 编码 (G4 特有): 00=HSI16, 01=HSE, 11=PLL —— 不是 F4 的 10 */
#define RCC_CFGR_SW_PLL         0x3u
#define RCC_CFGR_SWS_PLL        (0x3u << 2)     /* SWS 读数: 11=PLL */

/* PLLCFGR: PLLSRC@0(HSI16=0), PLLM[3:0]@4(÷(M+1)), PLLN[6:0]@8(直接值),
 * PLLP[3:0]@17(÷2(P+1)), PLLQ[3:0]@21, PLLREN@24, PLLR[2:0]@25(÷2(R+1)) */
#define RCC_PLLCFGR_PLLN_Pos    8u
#define RCC_PLLCFGR_PLLQ_Pos    21u
#define RCC_PLLCFGR_PLLREN      (1u << 24)

#define RCC_AHB1ENR_GPIOAEN     (1u << 0)
#define RCC_AHB1ENR_GPIOBEN     (1u << 1)
#define RCC_AHB1ENR_GPIOCEN     (1u << 2)
#define RCC_AHB1ENR_GPIODEN     (1u << 3)
#define RCC_APB1ENR1_PWREN      (1u << 28)
#define RCC_APB2ENR_USART1EN    (1u << 14)

/* ===================== PWR (RM0440 5) ===================== */
#define PWR_CR1                 REG32(0x40007000u)
#define PWR_CR1_VOS_RANGE1      0x400u          /* VOS[1:0]=10 → Range 1 (1.2V) */
#define PWR_CR1_VOS_MSK         0x600u

/* ===================== GPIO (AHB2, RM0440 8) ===================== */
/* MODER 每引脚 2 位: 00=输入 01=输出 10=AF 11=模拟(G4 复位默认全模拟) */
#define GPIOA_MODER             REG32(0x48000000u + 0x00u)
#define GPIOA_AFRH              REG32(0x48000000u + 0x24u)
#define GPIOC_MODER             REG32(0x48000800u + 0x00u)
#define GPIOC_PUPDR             REG32(0x48000800u + 0x0Cu)
#define GPIOC_BSRR              REG32(0x48000800u + 0x18u)  /* 低16位置1, 高16位清0 */
#define GPIOD_MODER             REG32(0x48000C00u + 0x00u)
#define GPIOD_PUPDR             REG32(0x48000C00u + 0x0Cu)
#define GPIOD_IDR               REG32(0x48000C00u + 0x10u)

/* ===================== USART1 (APB2, G4 为 ISR/RDR/TDR 型) ===================== */
#define USART1_CR1              REG32(0x40013800u + 0x00u)
#define USART1_BRR              REG32(0x40013800u + 0x0Cu)
#define USART1_ISR              REG32(0x40013800u + 0x1Cu)
#define USART1_RDR              REG32(0x40013800u + 0x24u)
#define USART1_TDR              REG32(0x40013800u + 0x28u)
#define USART_ISR_TXE           (1u << 7)
#define USART_ISR_RXNE          (1u << 5)
#define USART_CR1_UE            (1u << 0)       /* G4: UE 在 bit0 (F1 在 bit13) */
#define USART_CR1_RE            (1u << 2)
#define USART_CR1_TE            (1u << 3)

/* ===================== SysTick / SCB ===================== */
#define SYST_CSR                REG32(0xE000E010u)
#define SYST_RVR                REG32(0xE000E014u)
#define SYST_CVR                REG32(0xE000E018u)
#define SYST_CSR_ENABLE         (1u << 0)
#define SYST_CSR_TICKINT        (1u << 1)
#define SYST_CSR_CLKSOURCE      (1u << 2)   /* 1=HCLK, 0=HCLK/8 */
#define SCB_AIRCR               REG32(0xE000ED0Cu)      /* 应用中断/复位控制 */
#define SCB_AIRCR_SYSRESETREQ   0x05FA0004u             /* VECTKEY | SYSRESETREQ */

/* ===================== FLASH (RM0440 3, 双 bank / 2KB 页) ===================== */
#define FLASH_ACR               REG32(0x40022000u)      /* 等待周期/缓存 */
#define FLASH_KEYR              REG32(0x40022008u)
#define FLASH_SR                REG32(0x40022010u)
#define FLASH_CR                REG32(0x40022014u)
#define FLASH_OPTR              REG32(0x40022020u)

#define FLASH_ACR_LATENCY_Msk   0xFu
#define FLASH_SR_BSY            (1u << 16)
#define FLASH_SR_EOP            (1u << 0)
#define FLASH_SR_OPERR          (1u << 1)
#define FLASH_SR_PROGERR        (1u << 3)   /* 目标双字非全 1(单次编程违例) */
#define FLASH_SR_WRPERR         (1u << 4)
#define FLASH_SR_PGAERR         (1u << 5)   /* 编程地址未对齐双字 */
#define FLASH_SR_SIZERR         (1u << 6)   /* 访问宽度非 32 位字 */
#define FLASH_SR_PGSERR         (1u << 7)   /* 编程序列错误 */
#define FLASH_SR_MISERR         (1u << 8)
#define FLASH_SR_FASTERR        (1u << 9)
#define FLASH_SR_ERR_ALL        (FLASH_SR_OPERR | FLASH_SR_PROGERR | \
                                 FLASH_SR_WRPERR | FLASH_SR_PGAERR | \
                                 FLASH_SR_SIZERR | FLASH_SR_PGSERR | \
                                 FLASH_SR_MISERR | FLASH_SR_FASTERR)

#define FLASH_CR_PG             (1u << 0)   /* 编程使能(双字: 连续两字写) */
#define FLASH_CR_PER            (1u << 1)   /* 页擦除使能 */
#define FLASH_CR_PNB_Pos        3u          /* 页号[10:3], bank 内 0..127 */
#define FLASH_CR_PNB_Msk        (0x7Fu << FLASH_CR_PNB_Pos)
#define FLASH_CR_BKER           (1u << 11)  /* 页擦 bank 选择: 1=bank2 */
#define FLASH_CR_STRT           (1u << 16)
#define FLASH_CR_LOCK           (1u << 31)

#define FLASH_KEY1              0x45670123u
#define FLASH_KEY2              0xCDEF89ABu

#define FLASH_OPTR_DBANK        (1u << 22)  /* 双 bank 使能(出厂默认 1, 2KB 页) */

/* ===================== IWDG (RM0440 42) ===================== */
/* 映射与 F1 相同: 0x40003000-0x4000300C; G4 LSI ≈ 32kHz(数据手册典型值) */
#define IWDG_KR                 REG32(0x40003000u)
#define IWDG_PR                 REG32(0x40003004u)
#define IWDG_RLR                REG32(0x40003008u)
#define IWDG_SR                 REG32(0x4000300Cu)

#define IWDG_KEY_FEED           0xAAAAu
#define IWDG_KEY_UNLOCK         0x5555u
#define IWDG_KEY_START          0xCCCCu
#define IWDG_SR_PVU             (1u << 0)
#define IWDG_SR_RVU             (1u << 1)

#endif /* STM32G474_MIN_H */

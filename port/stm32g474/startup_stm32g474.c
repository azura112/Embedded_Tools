/**
 * @file    startup_stm32g474.c
 * @brief   Cortex-M4 启动代码: 向量表 + 数据段初始化 (不依赖 libc crt0)
 *
 * 复位后由硬件从向量表[0]装载 MSP, 跳转 Reset_Handler。
 * Reset_Handler 完成 .data 拷贝与 .bss 清零后调用 main, 不返回。
 *
 * 向量表按需稀疏: 内核异常 16 项 + USART1 (IRQ37, 轮询收发未用中断,
 * 落 Default_Handler 兜底)。其余外设中断未使能, 不会触发; 完整 102 项
 * 外设向量见 CubeMX 生成的 startup_stm32g474xx.s。
 */
#include <stdint.h>

extern uint32_t _estack;    /* 链接脚本: 栈顶 = RAM 末尾      */
extern uint32_t _sidata;    /* 链接脚本: .data 的加载地址     */
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

int  main(void);
void Reset_Handler(void);
void Default_Handler(void);

void SysTick_Handler(void);         /* 实现在 port_stm32g474.c */

/* 常规异常弱定义, 用户可按需覆盖 */
void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));

typedef void (*isr_fn_t)(void);

__attribute__((used, section(".isr_vector")))
const isr_fn_t g_vector_table[] = {
    (isr_fn_t)(uintptr_t)&_estack,  /* 0: 初始 MSP          */
    Reset_Handler,                  /* 1: 复位              */
    NMI_Handler,                    /* 2                    */
    HardFault_Handler,              /* 3                    */
    MemManage_Handler,              /* 4                    */
    BusFault_Handler,               /* 5                    */
    UsageFault_Handler,             /* 6                    */
    0, 0, 0, 0,                     /* 7~10: 保留           */
    SVC_Handler,                    /* 11                   */
    DebugMon_Handler,               /* 12                   */
    0,                              /* 13: 保留             */
    PendSV_Handler,                 /* 14                   */
    SysTick_Handler,                /* 15: 1ms 时基         */
    [16 + 37] = Default_Handler,    /* 53: USART1_IRQHandler(轮询收发, 未用) */
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {             /* .data: flash → ram */
        *dst++ = *src++;
    }
    dst = &_sbss;
    while (dst < &_ebss) {              /* .bss 清零 */
        *dst++ = 0u;
    }
    (void)main();
    for (;;) {                          /* main 返回异常情况: 原地停机 */
    }
}

void Default_Handler(void)
{
    for (;;) {
        /* 未实现的中断: 挂死等待调试器, 便于定位 */
    }
}

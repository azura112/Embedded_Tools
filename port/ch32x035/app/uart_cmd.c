/********************************** (C) COPYRIGHT *******************************
 * File Name          : uart_cmd.c
 * Description        : UART command channel (COM16 / USART1 PB10-PB11, H1.4/H1.5).
 *
 *                      并发拓扑(与原 CDC 命令路径同构):
 *                        RX:  USART1 ISR 写(et_ringbuf_write) / 主循环读
 *                        命令: 主循环生产(UART_Cmd_Task) / 主循环消费
 *                      解析规则与 V1.7 CDC 通道逐条一致:
 *                        跳过 \r \n 空格;A-Z 折成小写;队满丢弃。
 *******************************************************************************/

#include "uart_cmd.h"
#include "et_ringbuf.h"
#include "et_queue.h"

#define UART_RX_RING_SIZE   64      /* 2 的幂: ET_RINGBUF_POW2=1 构建 */
#define UART_CMD_QUEUE_SIZE 8

static et_ringbuf_t uart_rx_rb;
static uint8_t      uart_rx_mem[UART_RX_RING_SIZE];

static et_queue_t   uart_cmd_q;
static uint8_t      uart_cmd_mem[UART_CMD_QUEUE_SIZE];  /* 每槽 1 字节命令字符 */

static volatile uint32_t uart_rx_total;     /* bytes received since boot (diagnostics) */

void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      USART1_IRQHandler
 *
 * @brief   RXNE: 把收到的字节推入 RX 环(满则丢弃)。ORE: 读 STATR+DATAR
 *          清溢出(人手键入速率远低于轮询消费, 溢出仅在异常时出现)。
 *
 * @return  none
 */
void USART1_IRQHandler(void)
{
    if( USART_GetFlagStatus( USART1, USART_FLAG_ORE ) != RESET )
    {
        (void)USART1->STATR;                /* ORE 清除序列: 读 STATR */
        (void)USART1->DATAR;                /*               再读 DATAR */
    }

    if( USART_GetFlagStatus( USART1, USART_FLAG_RXNE ) != RESET )
    {
        uint8_t c = (uint8_t)USART_ReceiveData( USART1 );

        (void)et_ringbuf_write( &uart_rx_rb, &c, 1 );
        uart_rx_total++;
    }
}

/*********************************************************************
 * @fn      UART_Cmd_Init
 *
 * @brief   Open the USART1 receiver + RXNE interrupt. Call after
 *          USART_Printf_Init() (baud/clock/UE already configured).
 *
 * @return  none
 */
void UART_Cmd_Init( void )
{
    et_ringbuf_init( &uart_rx_rb, uart_rx_mem, sizeof( uart_rx_mem ) );
    et_queue_init( &uart_cmd_q, uart_cmd_mem, sizeof( uart_cmd_mem ), 1 );

    USART1->CTLR1 |= USART_Mode_Rx;         /* 打开接收器(发送侧已由 printf 初始化配好) */

    USART_ITConfig( USART1, USART_IT_RXNE, ENABLE );
    NVIC_EnableIRQ( USART1_IRQn );
}

/*********************************************************************
 * @fn      UART_Cmd_Task
 *
 * @brief   Parse received bytes into the command queue. Call ~10ms.
 *
 * @return  none
 */
void UART_Cmd_Task( void )
{
    uint8_t c;

    while( et_ringbuf_read( &uart_rx_rb, &c, 1 ) == 1 )
    {
        if( ( c == '\r' ) || ( c == '\n' ) || ( c == ' ' ) )
        {
            continue;
        }
        if( ( c >= 'A' ) && ( c <= 'Z' ) )
        {
            c = (uint8_t)( c + 32 );
        }
        (void)et_queue_push( &uart_cmd_q, &c );  /* 队满丢弃, 与 V1.7 一致 */
    }
}

/*********************************************************************
 * @fn      UART_Cmd_Read / UART_Cmd_RxTotal
 *
 * @brief   Pop one pending command (0 = none) / diagnostics counter.
 *
 * @return  see brief
 */
uint8_t UART_Cmd_Read( void )
{
    uint8_t c = 0;

    (void)et_queue_pop( &uart_cmd_q, &c );
    return c;
}

uint32_t UART_Cmd_RxTotal( void )
{
    return uart_rx_total;
}

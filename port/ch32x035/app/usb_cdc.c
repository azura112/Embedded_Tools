/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.c
 * Description        : Board CDC-ACM application layer. TX/RX rings between
 *                      the USB ISR and the main loop; single-producer /
 *                      single-consumer.
 *
 *                      (et 重构版): RX/TX 环 = et_ringbuf(SPSC 无锁, 自由
 *                      递增索引抗回绕), 命令队列 = et_queue。并发拓扑与
 *                      V1.7 一致:
 *                        RX: ISR 写(et_ringbuf_write) / 主循环读(read)
 *                        TX: 主循环写(CDC_Write)      / 主循环读(flush)
 *                        命令: 主循环生产 / 主循环消费
 *                      et_ringbuf_reset/et_queue_reset 在 USB 总线复位(ISR)
 *                      中调用: 与 V1.7 的"直接清零索引"同构, 复位瞬间主循环
 *                      若正访问环, 最坏丢几个字节, 由命令解析层自然过滤。
 *******************************************************************************/

#include "usb_cdc.h"
#include "ch32x035_usbfs_device.h"
#include "ch32x035_pwr.h"
#include "et_ringbuf.h"
#include "et_queue.h"
#include <stdarg.h>
#include <stdio.h>

volatile uint8_t CDC_Line_Coding[7] = { 0x00, 0xC2, 0x01, 0x00,   /* 115200 */
                                        0x00, 0x00, 0x08 };       /* 1-N-8  */

#define CDC_TX_RING_SIZE    512     /* 2 的幂: ET_RINGBUF_POW2=1 构建 */
#define CDC_RX_RING_SIZE    256
#define CDC_CMD_QUEUE_SIZE  8

static et_ringbuf_t cdc_tx_rb;
static uint8_t      cdc_tx_mem[CDC_TX_RING_SIZE];

static et_ringbuf_t cdc_rx_rb;
static uint8_t      cdc_rx_mem[CDC_RX_RING_SIZE];

static et_queue_t   cdc_cmd_q;
static uint8_t      cdc_cmd_mem[CDC_CMD_QUEUE_SIZE];    /* 每槽 1 字节命令字符 */

static volatile uint32_t cdc_rx_total;   /* bytes received since boot (diagnostics) */

/*********************************************************************
 * @fn      CDC_Init
 *
 * @brief   Set up the et containers and start the USBFS device
 *          (enumerates as CDC-ACM virtual COM).
 *
 * @return  none
 */
void CDC_Init( void )
{
    et_ringbuf_init( &cdc_tx_rb, cdc_tx_mem, sizeof( cdc_tx_mem ) );
    et_ringbuf_init( &cdc_rx_rb, cdc_rx_mem, sizeof( cdc_rx_mem ) );
    et_queue_init( &cdc_cmd_q, cdc_cmd_mem, sizeof( cdc_cmd_mem ), 1 );

    USBFS_RCC_Init( );
    USBFS_Device_Init( ENABLE, PWR_VDD_SupplyVoltage( ) );
}

/*********************************************************************
 * @fn      CDC_Connected
 *
 * @brief   1 if the host has set a configuration (data can flow).
 *
 * @return  uint8_t
 */
uint8_t CDC_Connected( void )
{
    return ( USBFS_DevConfig != 0 ) ? 1 : 0;
}

/*********************************************************************
 * @fn      CDC_Rx_Write
 *
 * @brief   ISR context: move one received EP2 packet into the RX ring.
 *
 * @return  none
 */
void CDC_Rx_Write( const uint8_t *p, uint8_t len )
{
    cdc_rx_total += et_ringbuf_write( &cdc_rx_rb, p, len );
}

/*********************************************************************
 * @fn      CDC_On_Reset
 *
 * @brief   ISR context: flush all buffers on USB bus reset.
 *          (仅中断上下文与主循环并发窗口极小, 语义见文件头)
 *
 * @return  none
 */
void CDC_On_Reset( void )
{
    et_ringbuf_reset( &cdc_tx_rb );
    et_ringbuf_reset( &cdc_rx_rb );
    et_queue_reset( &cdc_cmd_q );
}

/*********************************************************************
 * @fn      CDC_Write
 *
 * @brief   Queue bytes for upload to the host (dropped when ring full).
 *
 * @return  none
 */
void CDC_Write( const void *p, uint16_t len )
{
    if( USBFS_DevConfig == 0 )
    {
        return;
    }
    (void)et_ringbuf_write( &cdc_tx_rb, p, len );
}

/*********************************************************************
 * @fn      CDC_Printf
 *
 * @brief   Formatted output to the virtual COM port.
 *
 * @return  none
 */
void CDC_Printf( const char *fmt, ... )
{
    char buf[96];
    int n;
    va_list ap;

    if( USBFS_DevConfig == 0 )
    {
        return;
    }

    va_start( ap, fmt );
    n = vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );

    if( n > 0 )
    {
        CDC_Write( buf, (uint16_t)n );
    }
}

/*********************************************************************
 * @fn      CDC_RxTotal / CDC_Read_Cmd
 *
 * @brief   Diagnostics counter and command dequeue (0 = empty).
 *
 * @return  see brief
 */
uint32_t CDC_RxTotal( void )
{
    return cdc_rx_total;
}

uint8_t CDC_Read_Cmd( void )
{
    uint8_t c = 0;

    (void)et_queue_pop( &cdc_cmd_q, &c );
    return c;
}

/*********************************************************************
 * @fn      CDC_Task
 *
 * @brief   Parse received bytes into the command queue and flush the
 *          TX ring to the host. Call from the main loop (~10ms).
 *
 * @return  none
 */
void CDC_Task( void )
{
    const uint8_t *seg;
    uint32_t got;
    uint32_t used;
    uint8_t c;

    /* parse command bytes: RX 环消费者(ISR 是生产者) */
    while( et_ringbuf_read( &cdc_rx_rb, &c, 1 ) == 1 )
    {
        if( ( c == '\r' ) || ( c == '\n' ) || ( c == ' ' ) )
        {
            continue;
        }
        if( ( c >= 'A' ) && ( c <= 'Z' ) )
        {
            c = (uint8_t)( c + 32 );
        }
        (void)et_queue_push( &cdc_cmd_q, &c );  /* 队满丢弃, 与 V1.7 一致 */
    }

    /* flush TX ring: 零拷贝取连续段直接上送(每空闲 EP3 槽一个最大包) */
    if( USBFS_DevConfig != 0 )
    {
        while( ( ( used = et_ringbuf_used( &cdc_tx_rb ) ) != 0 ) &&
               ( USBFS_Endp_Busy[ DEF_UEP3 ] == 0 ) )
        {
            seg = et_ringbuf_read_peek( &cdc_tx_rb, DEF_USBD_FS_PACK_SIZE, &got );
            if( ( seg == NULL ) || ( got == 0 ) )
            {
                break;
            }
            /* DataUp 只在 CPY_LOAD 模式下读 pbuf(拷入端点缓冲), 去.const 安全 */
            USBFS_Endp_DataUp( DEF_UEP3, (uint8_t *)seg, (uint16_t)got, DEF_UEP_CPY_LOAD );
            et_ringbuf_drop( &cdc_tx_rb, got );
        }

        /* re-arm EP2 OUT after the ISR flow-control NAK */
        USBFSD->UEP2_CTRL_H = ( USBFSD->UEP2_CTRL_H & ~USBFS_UEP_R_RES_MASK ) | USBFS_UEP_R_RES_ACK;
    }
}

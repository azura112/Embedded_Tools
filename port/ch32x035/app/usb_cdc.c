/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.c
 * Description        : Board CDC-ACM application layer. TX ring between
 *                      the USB ISR and the main loop; single-producer /
 *                      single-consumer.
 *
 *                      (V1.8: 命令通道已移至调试串口 COM16 —— 见 uart_cmd.c;
 *                       本层为纯数据上报: TX 环 = et_ringbuf(SPSC 无锁,
 *                       零拷贝段上送), RX 环仅排空丢弃, 不再解析命令。)
 *******************************************************************************/

#include "usb_cdc.h"
#include "ch32x035_usbfs_device.h"
#include "ch32x035_pwr.h"
#include "et_ringbuf.h"
#include <stdarg.h>
#include <stdio.h>

volatile uint8_t CDC_Line_Coding[7] = { 0x00, 0xC2, 0x01, 0x00,   /* 115200 */
                                        0x00, 0x00, 0x08 };       /* 1-N-8  */

#define CDC_TX_RING_SIZE    512     /* 2 的幂: ET_RINGBUF_POW2=1 构建 */
#define CDC_RX_RING_SIZE    256

static et_ringbuf_t cdc_tx_rb;
static uint8_t      cdc_tx_mem[CDC_TX_RING_SIZE];

static et_ringbuf_t cdc_rx_rb;
static uint8_t      cdc_rx_mem[CDC_RX_RING_SIZE];

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
 *          (RX 字节不再解析命令, 由 CDC_Task 排空丢弃)
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
 * @fn      CDC_RxTotal
 *
 * @brief   Diagnostics counter (bytes received since boot).
 *
 * @return  uint32_t
 */
uint32_t CDC_RxTotal( void )
{
    return cdc_rx_total;
}

/*********************************************************************
 * @fn      CDC_Task
 *
 * @brief   Discard received bytes (commands moved to COM16/uart_cmd) and
 *          flush the TX ring to the host. Call from the main loop (~10ms).
 *
 * @return  none
 */
void CDC_Task( void )
{
    const uint8_t *seg;
    uint32_t got;
    uint32_t used;
    uint8_t c;

    /* RX 环排空丢弃: CDC 不再承担命令输入(ISR 是生产者) */
    while( et_ringbuf_read( &cdc_rx_rb, &c, 1 ) == 1 )
    {
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

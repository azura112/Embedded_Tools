/********************************** (C) COPYRIGHT *******************************
* File Name          : PD_Process.c
* Description        : PD sink firmware, based on WCH CH32X035 EVT USBPD_SNK
*                      example (V1.0.1, 2025/10/27), adapted for the PD test
*                      board:
*                        - selectable target PDO (no blocking error loop)
*                        - PD_Request_mV / PD_Contract_mV exported
*                        - PD_PHY_Reset clears the contract record
*******************************************************************************/

#include "debug.h"
#include <string.h>
#include "PD_Process.h"

void USBPD_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

__attribute__ ((aligned(4))) uint8_t PD_Rx_Buf[ 34 ];                           /* PD receive buffer */
__attribute__ ((aligned(4))) uint8_t PD_Tx_Buf[ 34 ];                           /* PD send buffer */

/******************************************************************************/
UINT8 PD_Ack_Buf[ 2 ];                                                          /* PD-ACK buffer */

__IO UINT8  Tmr_Ms_Cnt_Last;                                                    /* System timer ms last */
__IO UINT8  Tmr_Ms_Dlt;                                                         /* System timer ms delta */

PD_CONTROL PD_Ctl;                                                              /* PD control structure */

UINT8  Adapter_SrcCap[ 30 ];                                                    /* SrcCap from adapter */

__IO UINT8  PDO_Len;

/* Test board extensions */
__IO UINT8   PD_Target_PDO  = 1;    /* requested PDO index, default 1 (5V) */
__IO UINT16  PD_Request_mV  = 0;    /* voltage of the last REQUEST sent     */
__IO UINT16  PD_Contract_mV = 0;    /* valid contract voltage after PS_RDY  */

/* SrcCap Tables */
UINT8 SrcCap_5V3A_Tab[ 4 ]  = { 0X2C, 0X91, 0X01, 0X3E };
UINT8 SrcCap_5V2A_Tab[ 4 ]  = { 0XC8, 0X90, 0X01, 0X3E };
UINT8 SinkCap_5V1A_Tab[ 4 ] = { 0X64, 0X90, 0X01, 0X36 };

/* PD3.0 extended tables (only used when queried by the source) */
UINT8 SrcCap_Ext_Tab[ 28 ] =
{
    0X18, 0X80, 0X63, 0X00,
    0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X01, 0X00,
    0X00, 0X00, 0X07, 0X03,
    0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X03,
    0X00, 0X12, 0X00, 0X00,
};

UINT8 Status_Ext_Tab[ 8 ] =
{
    0X06, 0X80, 0X16, 0X00,
    0X00, 0X00, 0X00, 0X00,
};

/*********************************************************************
 * @fn      USBPD_IRQHandler
 *
 * @brief   Handles the USBPD interrupt (PHY events).
 *
 * @return  none
 */
void USBPD_IRQHandler(void)
{
    if(USBPD->STATUS & IF_RX_ACT)
    {
        USBPD->STATUS |= IF_RX_ACT;
        if( ( USBPD->STATUS & MASK_PD_STAT ) == PD_RX_SOP0 )
        {
            if( USBPD->BMC_BYTE_CNT >= 6 )
            {
                /* GoodCRC is answered by hardware, ignore this reception */
                if( ( USBPD->BMC_BYTE_CNT != 6 ) || ( ( PD_Rx_Buf[ 0 ] & 0x1F ) != DEF_TYPE_GOODCRC ) )
                {
                    Delay_Us(30);                       /* Delay 30us, answer GoodCRC */
                    PD_Ack_Buf[ 0 ] = 0x41;
                    PD_Ack_Buf[ 1 ] = ( PD_Rx_Buf[ 1 ] & 0x0E ) | PD_Ctl.Flag.Bit.Auto_Ack_PRRole;
                    USBPD->CONFIG |= IE_TX_END ;
                    PD_Phy_SendPack( 0, PD_Ack_Buf, 2, UPD_SOP0 );
                }
            }
        }
    }
    if(USBPD->STATUS & IF_TX_END)
    {
        /* Packet send completion (only after GoodCRC send completes) */
        USBPD->PORT_CC1 &= ~CC_LVE;
        USBPD->PORT_CC2 &= ~CC_LVE;

        NVIC_DisableIRQ(USBPD_IRQn);
        PD_Ctl.Flag.Bit.Msg_Recvd = 1;
        USBPD->STATUS |= IF_TX_END;
    }
    if(USBPD->STATUS & IF_RX_RESET)
    {
        USBPD->STATUS |= IF_RX_RESET;
        PD_SINK_Init( );
        printf("IF_RX_RESET\r\n");
    }
}

/*********************************************************************
 * @fn      PD_Rx_Mode
 *
 * @brief   Enter reception mode.
 *
 * @return  none
 */
void PD_Rx_Mode( void )
{
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;
    USBPD->CONFIG |= IE_RX_ACT | IE_RX_RESET|PD_DMA_EN;
    USBPD->DMA = (UINT32)(UINT8 *)PD_Rx_Buf;
    USBPD->CONTROL &= ~PD_TX_EN;
    USBPD->BMC_CLK_CNT = UPD_TMR_RX_48M;
    USBPD->CONTROL |= BMC_START ;
    NVIC_EnableIRQ( USBPD_IRQn );
}

/*********************************************************************
 * @fn      PD_SRC_Init
 *
 * @brief   Initialize SRC mode.
 *
 * @return  none
 */
void PD_SRC_Init( )
{
    PD_Ctl.Flag.Bit.PR_Role = 1;
    PD_Ctl.Flag.Bit.Auto_Ack_PRRole = 1;
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PU_330;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PU_330;
}

/*********************************************************************
 * @fn      PD_SINK_Init
 *
 * @brief   Initialize SNK mode.
 *
 * @return  none
 */
void PD_SINK_Init( )
{
    PD_Ctl.Flag.Bit.PR_Role = 0;
    PD_Ctl.Flag.Bit.Auto_Ack_PRRole = 0;
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;
}

/*********************************************************************
 * @fn      PD_PHY_Reset
 *
 * @brief   Reset the PD PHY and clear the contract record.
 *
 * @return  none
 */
void PD_PHY_Reset( void )
{
    PD_SINK_Init( );
    PD_Ctl.Flag.Bit.Stop_Det_Chk = 0;
    PD_Ctl.PD_State = STA_IDLE;
    PD_Ctl.Flag.Bit.PD_Comm_Succ = 0;
    PD_Contract_mV = 0;                         /* no valid contract any more */
    PD_Request_mV  = 0;
}

/*********************************************************************
 * @fn      PD_Init
 *
 * @brief   Initialize the PD registers and state.
 *
 * @return  none
 */
void PD_Init( void )
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBPD, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    AFIO->CTLR |= USBPD_IN_HVT | USBPD_PHY_V33;
    USBPD->CONFIG = PD_DMA_EN;
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;
    memset( &PD_Ctl.PD_State, 0x00, sizeof( PD_CONTROL ) );
    Adapter_SrcCap[ 0 ] = 1;
    memcpy( &Adapter_SrcCap[ 1 ], SrcCap_5V3A_Tab, 4 );
    PD_PHY_Reset( );
    PD_Rx_Mode( );
}

/*********************************************************************
 * @fn      PD_Detect
 *
 * @brief   Detect CC connection.
 *
 * @return  0:no connection; 1:CC1; 2:CC2
 */
UINT8 PD_Detect( void )
{
    UINT8  ret = 0;
    UINT8  cmp_cc1 = 0;
    UINT8  cmp_cc2 = 0;

    if(PD_Ctl.Flag.Bit.Connected)
    {
        /* disconnection is handled by VBUS monitoring (see main.c) */
    }
    else
    {
        USBPD->PORT_CC1 &= ~( CC_CMP_Mask|PA_CC_AI );
        USBPD->PORT_CC1 |= CC_CMP_22;
        Delay_Us(2);
        if( USBPD->PORT_CC1 & PA_CC_AI )
        {
            cmp_cc1 |= bCC_CMP_22;
        }

        USBPD->PORT_CC2 &= ~( CC_CMP_Mask|PA_CC_AI );
        USBPD->PORT_CC2 |= CC_CMP_22;
        Delay_Us(2);
        if( USBPD->PORT_CC2 & PA_CC_AI )
        {
            cmp_cc2 |= bCC_CMP_22;
        }

        if (USBPD->PORT_CC1 & CC_PD)
        {
            if ((cmp_cc1 & bCC_CMP_22) == bCC_CMP_22)
            {
                ret = 1;
            }
            if ((cmp_cc2 & bCC_CMP_22) == bCC_CMP_22)
            {
                if( ret )
                {
                    ret = 1;   /* Huawei A to C cable has two pull-up resistors */
                }
                else
                {
                    ret = 2;
                }
            }
        }
        else
        {
            /* SRC mode insertion detection */
        }
    }
    return( ret );
}

/*********************************************************************
 * @fn      PD_Det_Proc
 *
 * @brief   Process the CC connection detection result.
 *
 * @return  none
 */
void PD_Det_Proc( void )
{
    UINT8  status;

    if( PD_Ctl.Flag.Bit.Connected )
    {
        /* disconnection is handled by VBUS monitoring (see main.c) */
    }
    else
    {
        status = PD_Detect( );
        if( status == 0 )
        {
            PD_Ctl.Det_Cnt = 0;
        }
        else
        {
            PD_Ctl.Det_Cnt++;
        }
        if( PD_Ctl.Det_Cnt >= 5 )
        {
            PD_Ctl.Det_Cnt = 0;
            PD_Ctl.Flag.Bit.Connected = 1;
            if( PD_Ctl.Flag.Bit.Stop_Det_Chk == 0 )
            {
                if( (USBPD->PORT_CC1 & CC_PD) || (USBPD->PORT_CC2 & CC_PD) )
                {
                    if( status == 1 )
                    {
                        USBPD->CONFIG &= ~CC_SEL;
                    }
                    else
                    {
                        USBPD->CONFIG |= CC_SEL;
                    }
                    PD_Ctl.PD_State = STA_SRC_CONNECT;
                    printf("CC%d SRC Connect\r\n",status);
                }
                PD_Ctl.PD_Comm_Timer = 0;
            }
        }
    }
}

/*********************************************************************
 * @fn      PD_Phy_SendPack
 *
 * @brief   Send a PD packet.
 *
 * @return  none
 */
void PD_Phy_SendPack( UINT8 mode, UINT8 *pbuf, UINT8 len, UINT8 sop )
{
    if ((USBPD->CONFIG & CC_SEL) == CC_SEL )
    {
        USBPD->PORT_CC2 |= CC_LVE;
    }
    else
    {
        USBPD->PORT_CC1 |= CC_LVE;
    }

    USBPD->BMC_CLK_CNT = UPD_TMR_TX_48M;
    USBPD->DMA = (UINT32)(UINT8 *)pbuf;
    USBPD->TX_SEL = sop;
    USBPD->BMC_TX_SZ = len;
    USBPD->CONTROL |= PD_TX_EN;
    USBPD->STATUS &= BMC_AUX_INVALID;
    USBPD->CONTROL |= BMC_START;

    if( mode )
    {
        while( (USBPD->STATUS & IF_TX_END) == 0 );
        USBPD->STATUS |= IF_TX_END;
        if((USBPD->CONFIG & CC_SEL) == CC_SEL )
        {
            USBPD->PORT_CC2 &= ~CC_LVE;
        }
        else
        {
            USBPD->PORT_CC1 &= ~CC_LVE;
        }

        USBPD->CONFIG |=  PD_ALL_CLR ;
        USBPD->CONFIG &= ~( PD_ALL_CLR );
        USBPD->CONTROL &= ~ ( PD_TX_EN );
        USBPD->DMA = (UINT32)(UINT8 *)PD_Rx_Buf;
        USBPD->BMC_CLK_CNT = UPD_TMR_RX_48M;
        USBPD->CONTROL |= BMC_START;
    }
}

/*********************************************************************
 * @fn      PD_Load_Header
 *
 * @brief   Build the PD message header.
 *
 * @return  none
 */
void PD_Load_Header( UINT8 ex, UINT8 msg_type )
{
    PD_Tx_Buf[ 0 ] = msg_type;
    if( PD_Ctl.Flag.Bit.PD_Role )
    {
        PD_Tx_Buf[ 0 ] |= 0x20;
    }
    if( PD_Ctl.Flag.Bit.PD_Version )
    {
        PD_Tx_Buf[ 0 ] |= 0x80;                 /* PD3.0 */
    }
    else
    {
        PD_Tx_Buf[ 0 ] |= 0x40;                 /* PD2.0 */
    }

    PD_Tx_Buf[ 1 ] = PD_Ctl.Msg_ID & 0x0E;
    if( PD_Ctl.Flag.Bit.PR_Role )
    {
        PD_Tx_Buf[ 1 ] |= 0x01;
    }
    if( ex )
    {
        PD_Tx_Buf[ 1 ] |= 0x80;
    }
}

/*********************************************************************
 * @fn      PD_Send_Handle
 *
 * @brief   Handle a send transaction (retry on missing GoodCRC).
 *
 * @return  0:success; 1:fail
 */
UINT8 PD_Send_Handle( UINT8 *pbuf, UINT8 len )
{
    UINT8  pd_tx_trycnt;
    UINT8  cnt;

    if( ( len % 4 ) != 0 )
    {
        return( DEF_PD_TX_FAIL );
    }
    if( len > 28 )
    {
        return( DEF_PD_TX_FAIL );
    }

    cnt = len >> 2;
    PD_Tx_Buf[ 1 ] |= ( cnt << 4 );
    for( cnt = 0; cnt != len; cnt++ )
    {
        PD_Tx_Buf[ 2 + cnt ] = pbuf[ cnt ];
    }

    pd_tx_trycnt = 4;
    while( --pd_tx_trycnt )
    {
        NVIC_DisableIRQ( USBPD_IRQn );
        PD_Phy_SendPack( 0x01, PD_Tx_Buf, ( len + 2 ), UPD_SOP0 );

        cnt = 250;                              /* ~750us receive timeout */
        while( --cnt )
        {
            if( (USBPD->STATUS & IF_RX_ACT) == IF_RX_ACT)
            {
                USBPD->STATUS |= IF_RX_ACT;
                if( ( USBPD->BMC_BYTE_CNT == 6 ) && ( ( PD_Rx_Buf[ 0 ] & 0x1F ) == DEF_TYPE_GOODCRC ) )
                {
                    PD_Ctl.Msg_ID += 2;
                    break;
                }
            }
            Delay_Us( 3 );
        }
        if( cnt !=0 )
        {
            break;
        }
    }

    PD_Rx_Mode( );
    if( pd_tx_trycnt )
    {
        return( DEF_PD_TX_OK );
    }
    else
    {
        return( DEF_PD_TX_FAIL );
    }
}

/*********************************************************************
 * @fn      PDO_Request
 *
 * @brief   Send a REQUEST for the specified PDO.
 *
 * @return  none
 */
void PDO_Request( UINT8 pdo_index )
{
    UINT16 Current,Voltage;
    UINT8  status;

    if( pdo_index == 0 )
    {
        pdo_index = 1;
    }
    if( pdo_index > PDO_Len )
    {
        pdo_index = PDO_Len;                    /* clamp to the highest PDO */
    }
    if( pdo_index == 0 )
    {
        printf("No valid PDO to request\r\n");
        return;
    }

    memcpy( &PD_Rx_Buf[ 2 ], &Adapter_SrcCap[ 4*(pdo_index-1) + 1 ], 4 );
    PD_PDO_Analyse( 1, &PD_Rx_Buf[ 2 ], &Current, &Voltage );
    PD_Request_mV = Voltage;
    printf("Request PDO%d: %d mV / %d mA\r\n", pdo_index, Voltage, Current);

    PD_Load_Header( 0x00, DEF_TYPE_REQUEST );
    PD_Rx_Buf[ 5 ] = 0x03;
    PD_Rx_Buf[ 5 ] |= pdo_index<<4;
    PD_Rx_Buf[ 3 ] = PD_Rx_Buf[ 3 ] & 0x03;
    PD_Rx_Buf[ 3 ] |= ( PD_Rx_Buf[ 2 ] << 2 );
    PD_Rx_Buf[ 4 ] = PD_Rx_Buf[ 3 ];
    PD_Rx_Buf[ 4 ] <<= 2;
    PD_Rx_Buf[ 4 ] = PD_Rx_Buf[ 4 ] & 0x0C;
    PD_Rx_Buf[ 4 ] |= ( PD_Rx_Buf[ 2 ] >> 6 );

    status = PD_Send_Handle( &PD_Rx_Buf[ 2 ], 4 );

    if( status == DEF_PD_TX_OK )
    {
        PD_Ctl.PD_State = STA_RX_ACCEPT_WAIT;
    }
    else
    {
        PD_Ctl.PD_State = STA_TX_SOFTRST;
    }
    PD_Ctl.PD_Comm_Timer = 0;
    PD_Ctl.Flag.Bit.PD_Comm_Succ = 1;
}

/*********************************************************************
 * @fn      PD_Save_Adapter_SrcCap
 *
 * @brief   Parse and store the adapter SrcCap message.
 *
 * @return  none
 */
void PD_Save_Adapter_SrcCap( void )
{
    UINT8  i, len;

    len = ( ( PD_Rx_Buf[ 1 ] >> 4 ) & 0x07 );

    /* skip the PPS / APDO objects */
    for( i = 0; i < len; i++ )
    {
        if( ( PD_Rx_Buf[ 2 + ( i << 2 ) + 3 ] & 0xC0 ) == 0xC0 )
        {
            break;
        }
    }

    PDO_Len = i;
    PD_Rx_Buf[ 5 ] = 0x3E;
    PD_Rx_Buf[ 1 ] &= 0x8F;
    PD_Rx_Buf[ 1 ] |= i << 4;
    Adapter_SrcCap[ 0 ] = i;
    memcpy( &Adapter_SrcCap[ 1 ], &PD_Rx_Buf[ 2 ], ( i << 2 ) );
}

/*********************************************************************
 * @fn      PD_PDO_Analyse
 *
 * @brief   Decode a PDO into current (mA) and voltage (mV).
 *
 * @return  none
 */
void PD_PDO_Analyse( UINT8 pdo_idx, UINT8 *srccap, UINT16 *current, UINT16 *voltage )
{
    UINT32 temp32;

    temp32 = srccap[ (  ( pdo_idx - 1 ) << 2 ) + 0 ] +
                        ( (UINT32)srccap[ ( ( pdo_idx - 1 ) << 2 ) + 1 ] << 8 ) +
                        ( (UINT32)srccap[ ( ( pdo_idx - 1 ) << 2 ) + 2 ] << 16 );

    if( current != NULL )
    {
        *current = ( temp32 & 0x000003FF ) * 10;
    }
    if( voltage != NULL )
    {
        temp32 = temp32 >> 10;
        *voltage = ( temp32 & 0x000003FF ) * 50;
    }
}

/*********************************************************************
 * @fn      PD_Main_Proc
 *
 * @brief   Main PD protocol state machine.
 *
 * @return  none
 */
void PD_Main_Proc( )
{
    UINT8  status;
    UINT8  pd_header;
    UINT8 var;
    UINT16 Current,Voltage;

    PD_Ctl.PD_BusIdle_Timer += Tmr_Ms_Dlt;

    switch( PD_Ctl.PD_State )
    {
        case STA_DISCONNECT:
            printf("Disconnect\r\n");
            PD_PHY_Reset( );
            break;

        case STA_SRC_CONNECT:
            PD_Ctl.PD_Comm_Timer += Tmr_Ms_Dlt;
            if( PD_Ctl.PD_Comm_Timer > 999 )
            {
                PD_Ctl.Err_Op_Cnt++;
                if( PD_Ctl.Err_Op_Cnt > 5 )
                {
                    PD_Ctl.Err_Op_Cnt = 0;
                    PD_Ctl.PD_State = STA_IDLE;
                }
                else
                {
                    PD_PHY_Reset( );
                }
            }
            break;

        case STA_RX_ACCEPT_WAIT:
        case STA_RX_PS_RDY_WAIT:
            PD_Ctl.PD_Comm_Timer += Tmr_Ms_Dlt;
            if( PD_Ctl.PD_Comm_Timer > 499 )
            {
                PD_Ctl.Flag.Bit.Stop_Det_Chk = 0;
                PD_Ctl.PD_State = STA_TX_SOFTRST;
                PD_Ctl.PD_Comm_Timer = 0;
            }
            break;

        case STA_RX_PS_RDY:
            PD_Ctl.PD_State = STA_IDLE;
            if( PD_Ctl.PD_State == STA_RX_APD_PS_RDY_WAIT )
            {
                PD_Ctl.PD_State = STA_RX_APD_PS_RDY;
            }
            break;

        case STA_TX_SOFTRST:
            PD_Load_Header( 0x00, DEF_TYPE_SOFT_RESET );
            status = PD_Send_Handle( NULL, 0 );
            if( status == DEF_PD_TX_OK )
            {
                PD_Ctl.PD_State = STA_IDLE;
            }
            else
            {
                PD_Ctl.PD_State = STA_TX_HRST;
            }
            PD_Ctl.PD_Comm_Timer = 0;
            break;

        case STA_TX_HRST:
            PD_Ctl.Flag.Bit.Stop_Det_Chk = 1;
            PD_Phy_SendPack( 0x01, NULL, 0, UPD_HARD_RESET );
            PD_Rx_Mode( );
            PD_Ctl.PD_State = STA_IDLE;
            PD_Ctl.PD_Comm_Timer = 0;
            break;

        default:
            break;
    }

    if( PD_Ctl.Flag.Bit.Msg_Recvd )
    {
        PD_Ctl.Adapter_Idle_Cnt = 0x00;
        pd_header = PD_Rx_Buf[ 0 ] & 0x1F;
        switch( pd_header )
        {
            case DEF_TYPE_SRC_CAP:
                Delay_Ms( 5 );
                PD_Ctl.Flag.Bit.Stop_Det_Chk = 0;

                PD_Save_Adapter_SrcCap( );

                for (var = 1; var <= PDO_Len; ++var)
                {
                    PD_PDO_Analyse( var, &PD_Rx_Buf[ 2 ], &Current, &Voltage );
                    printf("PDO%d: %d mV / %d mA\r\n",var,Voltage,Current);
                }

                /* request the user-selected target (clamped to available) */
                PDO_Request( PD_Target_PDO );
                break;

            case DEF_TYPE_ACCEPT:
                PD_Ctl.PD_State = STA_RX_PS_RDY_WAIT;
                PD_Ctl.PD_Comm_Timer = 0;
                break;

            case DEF_TYPE_PS_RDY:
                printf("Success\r\n");
                PD_Contract_mV = PD_Request_mV;     /* contract established */
                printf("PD Contract: %d mV\r\n", PD_Contract_mV);
                PD_Ctl.PD_State = STA_RX_PS_RDY;
                break;

            case DEF_TYPE_WAIT:
                break;

            case DEF_TYPE_GET_SNK_CAP:
                Delay_Ms( 1 );
                PD_Load_Header( 0x00, DEF_TYPE_SNK_CAP );
                PD_Send_Handle( SinkCap_5V1A_Tab, sizeof( SinkCap_5V1A_Tab ) );
                break;

            case DEF_TYPE_SOFT_RESET:
                Delay_Ms( 1 );
                PD_Load_Header( 0x00, DEF_TYPE_ACCEPT );
                PD_Send_Handle( NULL, 0 );
                break;

            case DEF_TYPE_GET_SRC_CAP_EX:
                Delay_Ms( 1 );
                PD_Load_Header( 0x01, DEF_TYPE_SRC_CAP );
                PD_Send_Handle( SrcCap_Ext_Tab, sizeof( SrcCap_Ext_Tab ) );
                break;

            case DEF_TYPE_GET_STATUS:
                Delay_Ms( 1 );
                PD_Load_Header( 0x01, DEF_TYPE_GET_STATUS_R );
                PD_Send_Handle( Status_Ext_Tab, sizeof( Status_Ext_Tab ) );
                break;

            case DEF_TYPE_VCONN_SWAP:
                Delay_Ms( 1 );
                PD_Load_Header( 0x00, DEF_TYPE_REJECT );
                PD_Send_Handle( NULL, 0 );
                break;

            case DEF_TYPE_VENDOR_DEFINED:
                if( ( PD_Rx_Buf[ 2 ] & 0xC0 ) == 0 )
                {
                    Delay_Ms( 1 );
                    PD_Load_Header( 0x00, DEF_TYPE_VENDOR_DEFINED );
                    if( ( PD_Rx_Buf[ 3 ] & 0x60 ) == 0 )
                    {
                        PD_Ctl.Flag.Bit.VDM_Version = 0;
                    }
                    else
                    {
                        PD_Ctl.Flag.Bit.VDM_Version = 1;
                    }
                    PD_Rx_Buf[ 2 ] |= 0x80;
                    PD_Send_Handle( &PD_Rx_Buf[ 2 ], 4 );
                }
                break;

            default:
                printf("Unsupported Command\r\n");
                break;
        }

        PD_Rx_Mode( );
        PD_Ctl.Flag.Bit.Msg_Recvd = 0;
        PD_Ctl.PD_BusIdle_Timer = 0;
    }
}

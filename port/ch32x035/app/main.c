/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : CH32X035G8U6 USB PD Sink test board application.
 *                      (et 重构版,基于 User V1.7 基线)
 *
 *                      与 V1.7 的结构差异(行为对齐,数据流逐字节一致):
 *                        - 主循环骨架改用 et_sched 协作式任务
 *                          (5ms PD 检测 / 10ms 应用 / 100ms VBUS 守护 /
 *                           500ms 报告+LCD+扫档), 手写 ms 计数器删除;
 *                        - 1ms 时基由 TIM1 改为核内 SysTick(port 层维护,
 *                          同时替 PD 栈累加 8 位旧制计数器 Tim_Ms_Cnt);
 *                        - 按键消抖/事件归 et_key(板层只做电平采样);
 *                        - 状态 LED 归 et_led 模式管理;
 *                        - CDC 环/命令队列归 et_ringbuf/et_queue(见 usb_cdc.c);
 *                        - IWDG 归 port_wdt_* 契约(et_wdt 启用),
 *                          PVU/RVU 等待序列原样保留在 port 层;
 *                        - 诊断性 printf 改 et_log(UART, 带 [ms][级别][标签]
 *                          前缀); UART+CDC 双路数据行(SWBUS/SWEEP 等 CSV
 *                          及全部命令应答)仍走 Rep_Printf, 格式未动。
 *
 *                      Functions:
 *                        - USB PD sink: negotiates with the adapter over CC,
 *                          requests the selected PDO (default 5V), then can
 *                          re-request a higher voltage.
 *                        - VBUS monitored through OPA1 + ADC on PA3.
 *                        - LED2 (PB12) status: solid=contract, blink=searching.
 *                        - Keys: KEY0 cycle target voltage, KEY1 backlight,
 *                          KEY2 dump status.
 *                        - LCD (ST7735S 160x80): banner + live VBUS/PD status.
 *                        - USB CDC virtual COM (COM19): status stream only.
 *                          Commands are on the debug UART COM16 (V1.8):
 *                          n/1-8/b/s/a/o/c/,.<>? over USART1 RX (H1.4),
 *                          same actions as the keys.
 *******************************************************************************/

#include "debug.h"
#include "board.h"
#include "PD_Process.h"
#include "lcd.h"
#include "usb_cdc.h"
#include "uart_cmd.h"
#include "port_ch32x035.h"
#include "port.h"               /* port_wdt_feed() 兜底喂狗 */
#include "et_sched.h"
#include "et_key.h"
#include "et_led.h"
#include "et_wdt.h"
#include "et_log.h"
#include "et_kv.h"
#include "et_config.h"
#include <stdarg.h>
#include <stdio.h>

/* application handles */
static et_task_t task_pd_det;       /* 5ms   : PD CC detection            */
static et_task_t task_app;          /* 10ms  : wdt/keys/CDC/LED           */
static et_task_t task_vbus;         /* 100ms : VBUS disconnect guard      */
static et_task_t task_report;       /* 500ms : report + LCD + auto-sweep  */

static et_key_t app_key[BOARD_KEYn];
static et_led_t app_led;

static uint8_t backlight = 80;

/* auto-sweep state (command 'a'): walk every advertised PDO with a 2s dwell,
 * CSV-report each step over UART + USB CDC */
static uint8_t  sweep_on = 0;
static uint8_t  sweep_idx = 1;
static uint16_t sweep_ms = 0;

/* ------------------- 掉电记忆(V1.9): et_kv 双扇区乒乓 -------------------
 * 记忆项: 上次目标 PDO(每次切档即存) + 背光百分比。
 * 恢复时机: 建连并收到 SrcCap 后, 若记忆档 > 1 则自动重请求(一次性,
 * 断连后重新武装)。et_kv 写入为人手按键速率, 扇区寿命损耗可忽略。 */
static et_kv_t        app_kv;
static const et_kv_layout_t app_kv_layout = { 0u, 1u };  /* 参数区两扇区 */
static uint8_t        kv_ready = 0;
static uint8_t        kv_pdo_saved = 1;     /* 上电读出的记忆档 */
static uint8_t        pdo_restored = 0;     /* 本次连接是否已恢复 */

#define KV_KEY_TARGET_PDO   1u
#define KV_KEY_BACKLIGHT    2u

/*********************************************************************
 * @fn      App_KV_BootTest
 *
 * @brief   启动期 flash 自检(仅由 'q' 置 RAM 标志后复位触发): 在 UART
 *          初始化之后、任何中断使能之前执行擦写 —— 零中断上下文, 用于
 *          最终判定"擦写本身"与"并发中断"哪个是病根。此窗口 port 的中断
 *          门控自动关闭(port_flash_irq_gate_set(false))。
 *
 * @return  none
 */
static void App_KV_BootTest( void )
{
    static const uint8_t pat[ 8 ] = { 'B', 'T', '0', '1', 0x5A, 0xA5, 0x00, 0xFF };
    uint8_t rd[ 8 ];
    uint8_t i, ok = 1;
    uint32_t n;

    printf( "kvboot: erase s0\r\n" );
    if( !port_flash_erase_sector( 0u ) )
    {
        printf( "kvboot: erase FAIL\r\n" );
        return;
    }
    port_flash_read( 0u, rd, 8u );
    for( i = 0u; i < 8u; i++ )
    {
        if( rd[ i ] != 0xFFu ) ok = 0;
    }
    printf( "kvboot: erase %s\r\n", ok ? "PASS(FF)" : "FAIL(not FF)" );
    if( !ok ) return;
    printf( "kvboot: write 8B\r\n" );
    n = port_flash_write( 0u, pat, 8u );
    printf( "kvboot: write n=%u\r\n", (unsigned)n );
    port_flash_read( 0u, rd, 8u );
    ok = 1;
    for( i = 0u; i < 8u; i++ )
    {
        if( rd[ i ] != pat[ i ] ) ok = 0;
    }
    printf( "kvboot: write %s\r\n", ok ? "PASS" : "FAIL" );
}

/*********************************************************************
 * @fn      App_KV_SelfTest
 *
 * @brief   V1.9 诊断命令 'k': 逐级 flash 自检(每步打印, 崩溃点即最后
 *          一行)。PASS 后才置 kv_ready 并读回记忆项 —— flash 路径
 *          未验证期间, 开机不自动触碰 flash(安全引导)。
 *
 * @return  none
 */
static void Rep_Printf(const char *fmt, ...);
static uint8_t App_KV_Load_U8(uint16_t key, uint8_t dflt);
extern void Trace_Push(uint32_t id);        /* ch32x035_it.c: RAM 轨迹(诊断) */

static void App_KV_SelfTest( void )
{
    static const uint8_t pat[ 8 ] = { 'K', 'V', 'T', '1', 0x5A, 0xA5, 0x00, 0xFF };
    uint8_t rdbk[ 8 ];
    uint32_t n;
    uint8_t i, ok = 1;

    Trace_Push( 103u );
    Rep_Printf("kv1 erase s0\r\n");
    Trace_Push( 105u );
    if( !port_flash_erase_sector( 0u ) ) { Rep_Printf("kv FAIL erase0\r\n"); return; }
    Trace_Push( 106u );
    Rep_Printf("kv2 erase s1\r\n");
    Trace_Push( 107u );
    if( !port_flash_erase_sector( 1u ) ) { Rep_Printf("kv FAIL erase1\r\n"); return; }
    Trace_Push( 108u );
    Rep_Printf("kv3 write 8B\r\n");
    Trace_Push( 109u );
    n = port_flash_write( 0u, pat, 8u );
    Trace_Push( 110u );
    if( n != 8u ) { Rep_Printf("kv FAIL write=%u\r\n", (unsigned)n); return; }
    Rep_Printf("kv4 readback\r\n");
    Trace_Push( 111u );
    port_flash_read( 0u, rdbk, 8u );
    Trace_Push( 112u );
    for( i = 0; i < 8u; i++ )
    {
        if( rdbk[ i ] != pat[ i ] ) ok = 0;
    }
    if( !ok ) { Rep_Printf("kv FAIL verify\r\n"); return; }
    Rep_Printf("kv5 et_kv init+set+get\r\n");
    Trace_Push( 113u );
    if( !et_kv_init( &app_kv, &app_kv_layout ) )
    {
        if( !et_kv_format( &app_kv, &app_kv_layout ) ||
            !et_kv_init( &app_kv, &app_kv_layout ) )
        {
            Rep_Printf("kv FAIL et init\r\n"); return;
        }
    }
    Trace_Push( 114u );
    if( !et_kv_set( &app_kv, KV_KEY_TARGET_PDO, &kv_pdo_saved, 1u ) )
    {
        Rep_Printf("kv FAIL et set\r\n"); return;
    }
    if( !et_kv_get( &app_kv, KV_KEY_TARGET_PDO, rdbk, 1u, NULL ) ||
        ( rdbk[ 0 ] != kv_pdo_saved ) )
    {
        Rep_Printf("kv FAIL et get\r\n"); return;
    }
    kv_ready = 1;
    /* 记忆项读回: 背光立即生效, 目标档交由建连恢复逻辑 */
    backlight = App_KV_Load_U8( KV_KEY_BACKLIGHT, 80 );
    if( backlight > 100 ) backlight = 80;
    Board_Backlight_Set( backlight );
    kv_pdo_saved = App_KV_Load_U8( KV_KEY_TARGET_PDO, 1 );
    if( ( kv_pdo_saved < 1 ) || ( kv_pdo_saved > 8 ) ) kv_pdo_saved = 1;
    pdo_restored = 0;                       /* 重新武装建连恢复 */
    Rep_Printf("kv PASS, persistence on (backlight %u%%, saved PDO%u)\r\n",
               (unsigned)backlight, (unsigned)kv_pdo_saved);
}

static void App_KV_Save_U8( uint16_t key, uint8_t val )
{
    if( kv_ready )
    {
        (void)et_kv_set( &app_kv, key, &val, 1u );
    }
}

static uint8_t App_KV_Load_U8( uint16_t key, uint8_t dflt )
{
    uint8_t v = dflt;

    if( kv_ready )
    {
        (void)et_kv_get( &app_kv, key, &v, 1u, NULL );
    }
    return v;
}

/* LED 状态缓存: et_led 的 set_* 会重置相位起点, 只有状态切换才允许调用 */
static uint8_t led_state_last = 0xFFu;   /* 0=no source, 1=negotiating, 2=contract */

static void LCD_Status_Init(void);
static void LCD_Status_Update(uint16_t vbus);

/*********************************************************************
 * @fn      Rep_Printf
 *
 * @brief   Print a report line to the debug UART and, when a host is
 *          connected, to the USB CDC virtual COM port.
 *          (数据行: 格式与 V1.7 逐字节一致, 上位机脚本以此为准)
 *
 * @return  none
 */
static void Rep_Printf( const char *fmt, ... )
{
    char buf[96];
    int n;
    va_list ap;

    va_start( ap, fmt );
    n = vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );

    if( n > 0 )
    {
        printf( "%s", buf );
        CDC_Write( buf, (uint16_t)n );
    }
}

/*********************************************************************
 * @fn      PD_Force_Disconnect
 *
 * @brief   Called when VBUS is lost while a contract was expected.
 *
 * @return  none
 */
static void PD_Force_Disconnect( void )
{
    ET_LOGW( "pd", "VBUS lost, disconnect" );
    PD_Ctl.Flag.Bit.Connected = 0;
    PD_PHY_Reset( );
}

/*********************************************************************
 * @fn      App_PDO_Power_mW
 *
 * @brief   V1.9: 目标 PDO 的标称功率(mW) = 电压 mV × 电流 mA / 1000。
 *          idx 越界或无 SrcCap 返回 0。
 *
 * @return  uint32_t
 */
static uint32_t App_PDO_Power_mW( uint8_t idx )
{
    uint16_t cur, vol;

    if( ( PDO_Len == 0 ) || ( idx == 0 ) || ( idx > PDO_Len ) )
    {
        return 0;
    }
    PD_PDO_Analyse( idx, &Adapter_SrcCap[ 1 ], &cur, &vol );
    return ( (uint32_t)vol * cur ) / 1000u;
}

/*********************************************************************
 * @fn      Status_Dump
 *
 * @brief   Print the full PD status (KEY2 / CDC 's' command).
 *
 * @return  none
 */
static void Status_Dump( void )
{
    uint16_t vbus = Board_VBUS_Read_mV();
    uint8_t i;
    uint8_t max_i = 1;
    uint16_t cur, vol;
    uint32_t pmax = 0;

    /* 标记最高功率档(在 PDO 列表行尾输出 " (max)") */
    for( i = 1; i <= PDO_Len; i++ )
    {
        PD_PDO_Analyse( i, &Adapter_SrcCap[ 1 ], &cur, &vol );
        if( ( (uint32_t)vol * cur ) > pmax )
        {
            pmax = (uint32_t)vol * cur;
            max_i = i;
        }
    }

    Rep_Printf("---- PD status ----\r\n");
    Rep_Printf("PDO count: %d\r\n", PDO_Len);
    for( i = 1; i <= PDO_Len; i++ )
    {
        PD_PDO_Analyse( i, &Adapter_SrcCap[ 1 ], &cur, &vol );
        Rep_Printf("  PDO%d: %d mV / %d mA%s\r\n", i, vol, cur,
                   ( i == max_i ) ? " (max)" : "");
    }
    Rep_Printf("State: %d, target PDO: %d\r\n", PD_Ctl.PD_State, PD_Target_PDO);
    Rep_Printf("Contract: %d mV\r\n", PD_Contract_mV);
    Rep_Printf("VBUS: %d mV%s\r\n", vbus, Board_VBUS_OverRange() ? " (saturated!)" : "");
    Rep_Printf("-------------------\r\n");
}

/*********************************************************************
 * @fn      App_Cancel_Sweep
 *
 * @brief   V1.9: 手动切档/压测命令与自动扫档互斥 —— 打断当前扫档并
 *          告知(CSV 消费方以此行对齐)。
 *
 * @return  none
 */
static void App_Cancel_Sweep( void )
{
    if( sweep_on )
    {
        sweep_on = 0;
        Rep_Printf("SWEEP cancelled\r\n");
    }
}

/*********************************************************************
 * @fn      App_Command
 *
 * @brief   Execute one application command; shared by the user keys and
 *          the debug UART (COM16) command channel.
 *
 * @return  none
 */
static void App_Command( uint8_t cmd )
{
    switch( cmd )
    {
        /* KEY0 / 'n': cycle the target PDO voltage */
        case 'n':
            App_Cancel_Sweep( );
            PD_Target_PDO++;
            /* 有 SrcCap 时按实际档数环绕(未连接时 PDO_Len=0, 退回 1..8) */
            if( PD_Target_PDO > ( ( PDO_Len >= 1 ) ? PDO_Len : 8 ) )
            {
                PD_Target_PDO = 1;
            }
            Rep_Printf("Target PDO -> %d\r\n", PD_Target_PDO);
            kv_pdo_saved = PD_Target_PDO;
            App_KV_Save_U8( KV_KEY_TARGET_PDO, kv_pdo_saved );

            /* re-request now if a source is already connected */
            if( PD_Ctl.Flag.Bit.Connected && PDO_Len )
            {
                PDO_Request( PD_Target_PDO );
            }
            break;

        /* KEY1 / 'b': cycle backlight brightness */
        case 'b':
        {
            static const uint8_t steps[5] = { 0, 25, 50, 80, 100 };
            static uint8_t step = 3;
            step = (step + 1) % 5;
            Board_Backlight_Set( steps[ step ] );
            Rep_Printf("Backlight %d%%\r\n", steps[ step ]);
            App_KV_Save_U8( KV_KEY_BACKLIGHT, steps[ step ] );
            break;
        }

        /* KEY2 / 's': dump status */
        case 's':
            Status_Dump();
            break;

        /* 'a': auto-sweep every advertised PDO, 2s dwell, CSV report */
        case 'a':
            if( !PD_Ctl.Flag.Bit.Connected || !PDO_Len )
            {
                Rep_Printf("sweep: no source connected\r\n");
                break;
            }
            sweep_on = 1;
            sweep_idx = 1;
            sweep_ms = 0;
            PD_Target_PDO = 1;
            PDO_Request( 1 );
            Rep_Printf("SWEEP start,%d PDOs,2000ms dwell\r\n", PDO_Len);
            Rep_Printf("SWEEP,pdo,target_mV,vbus_mV,contract_mV\r\n");
            break;

        /* 'q': 置 RAM 标志并复位, 下轮开机在零中断上下文执行 flash 自检 */
        case 'q':
            Rep_Printf("arming kv boot test, rebooting...\r\n");
            *(volatile uint32_t *)0x200044A0u = 0x005174E5u;
            NVIC_SystemReset( );
            break;

        /* 'k': flash/kv 逐级自检(安全引导下唯一触碰 flash 的入口) */
        case 'k':
            Trace_Push( 101u );
            App_Cancel_Sweep( );
            Trace_Push( 102u );
            App_KV_SelfTest( );
            break;

        /* 'r': Hard Reset(压测) —— 源端断开重连后从头协商 */
        case 'r':
            App_Cancel_Sweep( );
            Rep_Printf("PD hard reset\r\n");
            PD_Contract_mV = 0;
            PD_Request_mV = 0;
            pdo_restored = 0;               /* 重新广播后自动回到记忆档 */
            PD_Send_HardReset( );
            break;

        /* 'e': Soft Reset(压测) —— 源端 Accept 后重发 SrcCap */
        case 'e':
            App_Cancel_Sweep( );
            Rep_Printf("PD soft reset\r\n");
            PD_Contract_mV = 0;
            PD_Request_mV = 0;
            pdo_restored = 0;
            PD_Send_SoftReset( );
            break;

        /* 'x': 半电流请求(压测) —— 只请求标称 50% 电流 */
        case 'x':
            App_Cancel_Sweep( );
            if( !PD_Ctl.Flag.Bit.Connected || !PDO_Len )
            {
                Rep_Printf("no source connected\r\n");
                break;
            }
            PDO_Request_Cur( PD_Target_PDO, 50 );
            break;

        /* 'z': 超标电流请求(压测) —— 请求标称 300% 电流, 源端应 Reject */
        case 'z':
            App_Cancel_Sweep( );
            if( !PD_Ctl.Flag.Bit.Connected || !PDO_Len )
            {
                Rep_Printf("no source connected\r\n");
                break;
            }
            PDO_Request_Cur( PD_Target_PDO, 300 );
            break;

        /* 'o' / 'c': LCD orientation & color-order tuning (bring-up helpers) */
        case 'o':
            LCD_SetRotation( (uint8_t)(LCD_GetRotation() + 1) );
            Rep_Printf("LCD rotation -> %d\r\n", LCD_GetRotation());
            LCD_Status_Init();
            LCD_Status_Update(Board_VBUS_Read_mV());
            break;
        case 'c':
            LCD_SetBgr( (uint8_t)!LCD_GetBgr() );
            Rep_Printf("LCD BGR -> %s\r\n", LCD_GetBgr() ? "on" : "off");
            LCD_Status_Init();
            LCD_Status_Update(Board_VBUS_Read_mV());
            break;

        /* offset tuning: , . shift CASET axis; < > shift RASET axis */
        case ',': case '.': case '<': case '>':
        {
            u8 offc, offr;
            LCD_GetOffsets(&offc, &offr);
            if(cmd == ',') { if(offc) offc--; }
            if(cmd == '.') { offc++; }
            if(cmd == '<') { if(offr) offr--; }
            if(cmd == '>') { offr++; }
            LCD_SetOffsets(offc, offr);
            Rep_Printf("LCD offsets: CASET=%d RASET=%d\r\n", offc, offr);
            LCD_Status_Init();
            LCD_Status_Update(Board_VBUS_Read_mV());
            break;
        }

        default:
            if( ( cmd >= '1' ) && ( cmd <= '8' ) )
            {
                App_Cancel_Sweep( );
                PD_Target_PDO = cmd - '0';
                Rep_Printf("Target PDO -> %d\r\n", PD_Target_PDO);
                kv_pdo_saved = PD_Target_PDO;
                App_KV_Save_U8( KV_KEY_TARGET_PDO, kv_pdo_saved );
                if( PD_Ctl.Flag.Bit.Connected && PDO_Len )
                {
                    PDO_Request( PD_Target_PDO );
                }
            }
            else if( cmd == '?' )
            {
                Rep_Printf("Commands: n=next PDO, 1-8=select PDO, b=backlight, s=status,\r\n"
                           "          a=auto-sweep all PDOs (CSV report)\r\n"
                           "PD test:  r=hard reset, e=soft reset, x=half-current req,\r\n"
                           "          z=over-current req (300%%, expect Reject), k=kv test, q=kv boot test\r\n"
                           "LCD tune: o=orientation(0-7), c=BGR, , . < > =shift offsets, ?=help\r\n");
            }
            break;
    }
}

/* ------------------------------ keys (et_key) ----------------------------- */

/* KEY0/1/2 → 与 CDC 命令字相同的动作 */
static const uint8_t key_cmd[BOARD_KEYn] = { 'n', 'b', 's' };

static bool key_read_fn( void *user )
{
    return Board_Key_Read_Raw( (uint8_t)(uintptr_t)user );
}

static void key_event_fn( et_key_t *k, et_key_event_t ev, void *user )
{
    (void)k;
    /* V1.7 语义: 稳定按下沿触发一次(非释放) → 映射 PRESS 事件 */
    if( ev == ET_KEY_PRESS )
    {
        App_Command( key_cmd[ (uint8_t)(uintptr_t)user ] );
    }
}

static void Keys_Init( void )
{
    static const et_key_params_t prm = { .debounce_ms = 30u,     /* 3×10ms, 同 V1.7 */
                                         .long_press_ms = 600u,  /* 未用长按 */
                                         .repeat_ms = 0u };      /* 关闭连发 */
    uint8_t i;

    Board_Key_Init();
    for( i = 0; i < BOARD_KEYn; i++ )
    {
        et_key_init( &app_key[ i ], key_read_fn, key_event_fn,
                     (void *)(uintptr_t)i, &prm );
    }
}

/* ------------------------------- LED (et_led) ----------------------------- */

static void led_write_fn( void *user, uint8_t brightness )
{
    (void)user;
    Board_LED_Set( (uint8_t)( brightness >= 128u ) );
}

/*********************************************************************
 * @fn      LED_Proc
 *
 * @brief   Update the status LED mode from the PD state, then poll.
 *          语义对齐 V1.7 意图: contract=常亮, 协商中=100ms 周期快闪,
 *          无源=400ms 周期慢闪。
 *          (V1.7 的手写实现有一处缺陷: 以 10ms 槽累加器做闪烁相位,
 *           ms10/10 恒为 0, 两个闪烁分支实际恒灭 —— et_led 版按注释
 *           意图实现了真正的闪烁)
 *
 * @return  none
 */
static void LED_Proc( void )
{
    uint8_t st = ( PD_Contract_mV > 0 ) ? 2u :
                 ( PD_Ctl.Flag.Bit.Connected ? 1u : 0u );

    if( st != led_state_last )
    {
        led_state_last = st;
        if( st == 2u )
        {
            et_led_set_on( &app_led );                      /* solid: contract */
        }
        else if( st == 1u )
        {
            et_led_set_blink( &app_led, 200u, 50u, 0u );    /* fast blink */
        }
        else
        {
            et_led_set_blink( &app_led, 800u, 50u, 0u );    /* slow blink */
        }
    }
    et_led_poll( &app_led, port_tick_get_ms() );
}

/* ----------------------------- scheduler tasks ---------------------------- */

/* 5ms: PD CC 检测(V1.7 的 Det_Timer 累加阈值等价于 5ms 周期任务) */
static void task_pd_det_fn( void *arg )
{
    (void)arg;
    PD_Det_Proc( );
}

/* 10ms: 看门狗 + 按键 + CDC(上报) + UART 命令 + LED */
static void task_app_fn( void *arg )
{
    uint8_t cmd;
    uint32_t now = port_tick_get_ms();
    uint8_t i;

    (void)arg;
    et_wdt_feed( );
    for( i = 0; i < BOARD_KEYn; i++ )
    {
        et_key_scan( &app_key[ i ], now );
    }
    CDC_Task( );                    /* 数据上报 + CDC RX 排空(命令已移走) */
    UART_Cmd_Task( );               /* UART RX 环 → 命令队列 */
    while( ( cmd = UART_Cmd_Read() ) != 0 )
    {
        App_Command( cmd );
    }
    LED_Proc( );
}

/* 100ms: VBUS sampling + disconnect check(逻辑与 V1.7 一致) */
static void task_vbus_fn( void *arg )
{
    static uint16_t vbus_low_ms = 0;

    (void)arg;
    if( PD_Ctl.Flag.Bit.Connected )
    {
        uint16_t vbus = Board_VBUS_Read_mV();
        if( vbus < 800 )
        {
            vbus_low_ms += 100;
            if( vbus_low_ms > 1000 )   /* >1s of low VBUS while attached */
            {
                vbus_low_ms = 0;
                PD_Force_Disconnect();
            }
        }
        else
        {
            vbus_low_ms = 0;
        }
    }
    else
    {
        vbus_low_ms = 0;
    }
}

/* 500ms: report VBUS + contract (UART + USB CDC), LCD refresh, kv restore, sweep */
static void task_report_fn( void *arg )
{
    static uint8_t prev_conn = 0xFFu;
    static uint16_t prev_contract = 0xFFFFu;
    uint16_t vbus = Board_VBUS_Read_mV();

    (void)arg;
    Rep_Printf("VBUS=%d mV%s Contract=%d mV\r\n",
               vbus,
               Board_VBUS_OverRange() ? "+" : "",
               PD_Contract_mV);
    LCD_Status_Update(vbus);

    /* 事件日志: contract 变化带 [ms] 时间戳, 便于与 SWEEP CSV 对时 */
    if( PD_Contract_mV != prev_contract )
    {
        if( PD_Contract_mV )
        {
            ET_LOGI( "pd", "contract %u mV", (unsigned)PD_Contract_mV );
        }
        else
        {
            ET_LOGW( "pd", "contract lost" );
        }
        prev_contract = PD_Contract_mV;
    }

    /* 掉电记忆恢复: 建连并收到 SrcCap 后, 回到上次目标档(一次性) */
    if( !PD_Ctl.Flag.Bit.Connected )
    {
        pdo_restored = 0;
        prev_conn = 0u;
    }
    else if( ( !pdo_restored ) && PDO_Len )
    {
        pdo_restored = 1;
        if( ( kv_pdo_saved > 1 ) && ( kv_pdo_saved <= PDO_Len ) &&
            ( kv_pdo_saved != PD_Target_PDO ) )
        {
            PD_Target_PDO = kv_pdo_saved;
            PDO_Request( kv_pdo_saved );
            ET_LOGI( "kv", "restore PDO%u", (unsigned)kv_pdo_saved );
        }
    }
    else if( prev_conn == 0u )
    {
        ET_LOGI( "pd", "connected" );
    }
    prev_conn = 1u;

    /* auto-sweep: log current step, advance to next PDO */
    if( sweep_on )
    {
        sweep_ms += 500;
        if( !PD_Ctl.Flag.Bit.Connected )
        {
            sweep_on = 0;
            Rep_Printf("SWEEP aborted: source lost\r\n");
        }
        else if( sweep_ms >= 2000 )
        {
            Rep_Printf("SWEEP,%d,%d,%d,%d\r\n",
                       sweep_idx, PD_Request_mV, vbus, PD_Contract_mV);
            sweep_ms = 0;
            sweep_idx++;
            if( sweep_idx > PDO_Len )
            {
                sweep_on = 0;
                PD_Target_PDO = 1;
                PDO_Request( 1 );
                Rep_Printf("SWEEP done, back to PDO1\r\n");
            }
            else
            {
                PD_Target_PDO = sweep_idx;
                PDO_Request( sweep_idx );
            }
        }
    }
}

/*********************************************************************
 * @fn      LCD_Status_Init
 *
 * @brief   Draw the static banner of the LCD status screen.
 *          (与 V1.7 一致)
 *
 * @return  none
 */
static void LCD_Status_Init( void )
{
    LCD_Fill( LCD_COLOR_BLACK );
    LCD_DrawString( 2, 2, "CH32X035 PD SNK TEST", LCD_COLOR_CYAN, LCD_COLOR_BLACK, 1 );
    LCD_DrawHLine( 0, LCD_W - 1, 12, LCD_COLOR_DGRAY );
}

/*********************************************************************
 * @fn      LCD_Status_Update
 *
 * @brief   Refresh the LCD live area: measured VBUS, target PDO and
 *          PD state. Called every 500ms. (与 V1.7 一致)
 *
 * @param   vbus - measured VBUS reading in mV
 *
 * @return  none
 */
static void LCD_Status_Update( uint16_t vbus )
{
    uint16_t fg;
    const char *st;
    u32 v;
    u8 d;
    u16 px;

    LCD_FillRect( 0, 14, LCD_W - 1, LCD_H - 1, LCD_COLOR_BLACK );

    /* line 1: measured VBUS, 5-digit right-aligned field ending at x=122 */
    LCD_DrawString( 2, 16, "VBUS", LCD_COLOR_WHITE, LCD_COLOR_BLACK, 2 );
    v = vbus;
    d = 1;
    while( v >= 10 )
    {
        v /= 10;
        d++;
    }
    LCD_DrawUInt( 122 - d * 12, 16, vbus, LCD_COLOR_WHITE, LCD_COLOR_BLACK, 2 );
    LCD_DrawString( 122, 16, "mV", LCD_COLOR_WHITE, LCD_COLOR_BLACK, 2 );
    if( Board_VBUS_OverRange() )
    {
        LCD_DrawString( 146, 16, "+", LCD_COLOR_CYAN, LCD_COLOR_BLACK, 2 );
    }

    /* line 2: target PDO + requested voltage */
    if( PD_Ctl.Flag.Bit.Connected && PDO_Len )
    {
        fg = PD_Contract_mV ? LCD_COLOR_GREEN : LCD_COLOR_YELLOW;
        px = LCD_DrawString( 2, 36, "PDO", fg, LCD_COLOR_BLACK, 2 );
        LCD_DrawChar( px, 36, '0' + PD_Target_PDO, fg, LCD_COLOR_BLACK, 2 );
        if( PD_Request_mV )
        {
            v = PD_Request_mV;
            d = 1;
            while( v >= 10 )
            {
                v /= 10;
                d++;
            }
            LCD_DrawUInt( 122 - d * 12, 36, PD_Request_mV, fg, LCD_COLOR_BLACK, 2 );
        }
        else
        {
            LCD_DrawString( 74, 36, "----", fg, LCD_COLOR_BLACK, 2 );
        }
        LCD_DrawString( 122, 36, "mV", fg, LCD_COLOR_BLACK, 2 );
    }
    else
    {
        px = LCD_DrawString( 2, 36, "PDO", LCD_COLOR_GRAY, LCD_COLOR_BLACK, 2 );
        LCD_DrawChar( px, 36, '0' + PD_Target_PDO, LCD_COLOR_GRAY, LCD_COLOR_BLACK, 2 );
    }

    /* line 3: PD state + 目标档标称功率(V1.9) */
    if( PD_Contract_mV )
    {
        st = "CONTRACT ESTABLISHED";
        fg = LCD_COLOR_GREEN;
    }
    else if( PD_Ctl.Flag.Bit.Connected )
    {
        st = "NEGOTIATING";
        fg = LCD_COLOR_YELLOW;
    }
    else
    {
        st = "NO SOURCE";
        fg = LCD_COLOR_GRAY;
    }
    LCD_DrawString( 2, 58, st, fg, LCD_COLOR_BLACK, 1 );

    /* 右侧: 目标 PDO 标称功率 "xx.xW"(无 SrcCap 时占位); x=124 → 最宽
     * "100.0W"(6 字符 × 6px)恰好收在 x=159 */
    if( PDO_Len && ( PD_Target_PDO <= PDO_Len ) )
    {
        uint32_t pmw = App_PDO_Power_mW( PD_Target_PDO );
        char pwr[8];
        snprintf( pwr, sizeof( pwr ), "%u.%uW",
                  (unsigned)( pmw / 1000u ), (unsigned)( ( pmw / 100u ) % 10u ) );
        LCD_DrawString( 124, 58, pwr, fg, LCD_COLOR_BLACK, 1 );
    }
    else
    {
        LCD_DrawString( 124, 58, "--.-W", LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1 );
    }
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    /* 复位原因先行采样(V1.9 诊断): RCC 标志跨软复位保持, 打印后清除。
     * IWDG=看门狗超时 / SFT=HardFault 软复位 / POR+PIN=正常上电 */
    uint8_t rst_iwdg, rst_sft, rst_wwdg, rst_lpw, rst_por, rst_pin;

    rst_iwdg = ( RCC_GetFlagStatus( RCC_FLAG_IWDGRST ) != RESET );
    rst_sft  = ( RCC_GetFlagStatus( RCC_FLAG_SFTRST ) != RESET );
    rst_wwdg = ( RCC_GetFlagStatus( RCC_FLAG_WWDGRST ) != RESET );
    rst_lpw  = ( RCC_GetFlagStatus( RCC_FLAG_LPWRRST ) != RESET );
    rst_por  = ( RCC_GetFlagStatus( RCC_FLAG_PORRST ) != RESET );
    rst_pin  = ( RCC_GetFlagStatus( RCC_FLAG_PINRST ) != RESET );

    /* IWDG cannot be stopped once enabled and keeps counting across resets:
     * feed immediately so an inherited short timeout cannot bite mid-boot
     * before et_wdt_enable below reconfigures it. Harmless if never enabled. */
    port_wdt_feed( );

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);

    /* 零中断上下文的 flash 自检(仅 'q' 置位后的一轮开机执行) */
    if( *(volatile uint32_t *)0x200044A0u == 0x005174E5u )
    {
        *(volatile uint32_t *)0x200044A0u = 0u;
        port_flash_irq_gate_set( false );       /* 此刻尚无任何中断, 门控关闭 */
        App_KV_BootTest( );
        port_flash_irq_gate_set( true );
    }

    UART_Cmd_Init( );       /* 打开 USART1 接收: 命令通道在 COM16(V1.8 起) */

    ET_LOGI( "boot", "rst iwdg=%u sft=%u wwdg=%u lpw=%u por=%u pin=%u",
             rst_iwdg, rst_sft, rst_wwdg, rst_lpw, rst_por, rst_pin );
    RCC_ClearFlag( );

    /* 上一轮 HardFault 现场 + 轨迹(mcause/mepc/最后检查点, RAM 跨复位保留) */
    {
        volatile uint32_t *hf = (volatile uint32_t *)0x20004400u;
        if( hf[ 0 ] == 0xC0DEF00Du )
        {
            uint32_t h = hf[ 3 ], i;
            ET_LOGE( "hf", "mcause=%08x mepc=%08x",
                     (unsigned)hf[ 1 ], (unsigned)hf[ 2 ] );
            printf( "hf trace:" );
            for( i = 0u; i < 32u; i++ )
            {
                uint32_t v = hf[ 4 + ( ( h + i ) & 31u ) ];
                if( v != 0u )
                {
                    printf( " %u", (unsigned)v );
                }
            }
            printf( "\r\n" );
            hf[ 0 ] = 0;
        }
    }

    ET_LOGI( "app", "SystemClk:%u", (unsigned)SystemCoreClock );
    ET_LOGI( "app", "ChipID:%08x", (unsigned)DBGMCU_GetCHIPID() );
    ET_LOGI( "app", "CH32X035 PD SNK Test Board fw V1.9 (et %s)", ET_VERSION_STRING );

    /* arm IWDG ~10s via port contract (PVU/RVU wait recipe lives in port);
     * boot path below is fed explicitly */
    if( et_wdt_enable( 10000u ) )
    {
        ET_LOGI( "wdt", "armed, ~10s" );
    }
    else
    {
        ET_LOGE( "wdt", "arm failed" );
    }

    ET_LOGI( "boot", "b3 pre-kv" );

    /* 掉电记忆: flash 路径未在板上验证前, 开机不自动触碰 flash(安全引导)。
     * 用 'k' 命令运行逐级自检, PASS 后本轮上电内启用持久化 */
    kv_pdo_saved = 1;

    Board_LED_Init();
    Keys_Init();
    Board_Backlight_Init( backlight );
    Board_VBUS_Init();
    et_led_init( &app_led, led_write_fn, NULL );
    led_state_last = 0xFFu;                 /* 首个 10ms 槽按 PD 状态落模式 */

    LCD_Init();
    et_wdt_feed( );                         /* LCD init burns ~450ms of delays */
    LCD_Status_Init();
    ET_LOGI( "boot", "b5 lcd done" );

    PD_Init( );
    CDC_Init( );

    /* 1ms TIM1 时基先于任务注册: et_sched_register 以当前 tick 锚定首轮 */
    port_ch32x035_tick_init( );

    et_sched_register( &task_pd_det, task_pd_det_fn, NULL, 5u );
    et_sched_register( &task_app,    task_app_fn,    NULL, 10u );
    et_sched_register( &task_vbus,   task_vbus_fn,   NULL, 100u );
    et_sched_register( &task_report, task_report_fn, NULL, 500u );
    ET_LOGI( "boot", "b6 loop start" );

    while(1)
    {
        /* PD 协议栈的 8 位毫秒增量: 单次读取取快照, 快照内一致(8 位
         * 无符号回绕由减法自动正确; 单字节读取天然原子, 无需关中断) */
        uint8_t cnt_now = Tim_Ms_Cnt;
        Tmr_Ms_Dlt = cnt_now - Tmr_Ms_Cnt_Last;
        Tmr_Ms_Cnt_Last = cnt_now;

        /* 到期任务: 5ms 检测 → 10ms 应用 → 100ms → 500ms(注册序, 与
         * V1.7 的"检测先于协议泵"次序一致) */
        et_sched_poll_once( );

        /* PD 协议泵: 每轮主循环执行, 消费 Tmr_Ms_Dlt */
        PD_Main_Proc( );
    }
}

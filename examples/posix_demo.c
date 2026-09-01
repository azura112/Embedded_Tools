/**
 * @file    posix_demo.c
 * @brief   Embedded_Tools 全栈联动演示 (宿主机可运行, 输出确定性)
 *
 * 场景: 模拟一条 UART 链路
 *   生产者任务 --(组帧)--> 环形缓冲 --(消费)--> 帧解析 --> AT命令 --> 日志应答
 * 同时演示软定时器心跳 / LED 闪烁 / 按键事件。
 *
 * 构建运行: make demo
 */
#include <stdio.h>
#include "et_config.h"
#include "et_ringbuf.h"
#include "et_frame.h"
#include "et_atcmd.h"
#include "et_stimer.h"
#include "et_sched.h"
#include "et_key.h"
#include "et_led.h"
#include "et_log.h"
#include "port_host.h"

/* ---- 共享资源 ---- */
static et_ringbuf_t        g_uart_rx;            /* 模拟串口接收缓冲 */
static uint8_t             g_rb_mem[128];

static et_frame_parser_t   g_parser;             /* 帧: AA 55 [len] [payload] [sum8] */
static uint8_t             g_pay[48];

static et_atcmd_proc_t     g_at;                 /* 载荷即命令行 */
static char                g_line[40];

static et_stimer_t         g_heartbeat;
static et_task_t           g_prod, g_cons;
static et_led_t            g_led;
static et_key_t            g_key;

static bool                g_btn_down;
static bool                g_led_want;
static uint32_t            g_msg_no;

/* ---- AT 命令实现 ---- */
static void cmd_ping(char *args, void *user)
{
    (void)args;
    (void)user;
    ET_LOGI("cmd", "PONG");
}

static void cmd_echo(char *args, void *user)
{
    (void)user;
    ET_LOGI("cmd", "echo: %s", args);
}

static void on_unknown(const char *name, uint16_t len,
                       const char *args, void *user)
{
    (void)args;
    (void)user;
    ET_LOGW("cmd", "unknown(%u): %.*s", len, (int)len, name);
}

static const et_atcmd_entry_t g_cmds[] = {
    { "PING", cmd_ping },
    { "ECHO", cmd_echo },
};

/* ---- 帧解析完成回调: 把载荷交给 AT 解析器 ---- */
static void on_frame(et_frame_parser_t *p, uint16_t len, void *user)
{
    uint16_t i;

    (void)user;
    for (i = 0u; i < len; i++) {
        (void)et_atcmd_feed(&g_at, (char)p->rx_buf[i]);
    }
    (void)et_atcmd_feed(&g_at, '\r');           /* 结算该命令行 */
}

/* ---- 任务: 生产者(模拟远端设备发帧, 含一帧故意损坏的数据) ---- */
static void prod_run(void *arg)
{
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    et_frame_cfg_t c;
    uint8_t  tx[64];
    char     msg[24];
    int      m;
    uint16_t n;

    (void)arg;
    if ((g_msg_no & 1u) == 0u) {
        m = sprintf(msg, "PING");
    } else {
        m = sprintf(msg, "ECHO msg%u", (unsigned)g_msg_no / 2u);
    }

    c.header = hdr;     c.header_len = 2u;
    c.use_len = true;   c.fixed_len = 0u;
    c.crc = ET_FRAME_CRC_SUM8;
    c.use_tail = false; c.tail = 0u;
    c.rx_buf = NULL;    c.rx_cap = 0u;
    c.on_frame = NULL;  c.user = NULL;
    n = et_frame_pack(&c, (const uint8_t *)msg, (uint16_t)m, tx, sizeof(tx));

    if (n != 0u) {
        if (g_msg_no == 4u) {
            tx[3] ^= 0xFFu;                     /* 注入损坏帧(校验将失败) */
            ET_LOGW("prod", "inject corrupted frame");
        }
        (void)et_ringbuf_write(&g_uart_rx, tx, n);  /* 模拟 ISR 收包入缓冲 */
    }
    g_msg_no++;
}

/* ---- 任务: 消费者(逐字节喂帧解析器) ---- */
static void cons_run(void *arg)
{
    uint8_t b;

    (void)arg;
    while (et_ringbuf_read(&g_uart_rx, &b, 1u) == 1u) {
        (void)et_frame_feed(&g_parser, b);
    }
}

/* ---- 软定时器: 心跳 ---- */
static void hb_fire(void *arg)
{
    (void)arg;
    ET_LOGI("sys", "alive, rb_used=%u", et_ringbuf_used(&g_uart_rx));
}

/* ---- 按键与 LED ---- */
static bool key_read(void *user)
{
    (void)user;
    return g_btn_down;                          /* 已按高有效归一化 */
}

static void led_write(void *user, uint8_t brightness)
{
    (void)user;
    ET_LOGD("led", "brightness=%u", brightness);
}

static void key_event(struct et_key *k, et_key_event_t ev, void *user)
{
    (void)k;
    (void)user;
    if (ev == ET_KEY_CLICK) {
        ET_LOGI("key", "click -> %s led",
                g_led_want ? "off" : "on");
        g_led_want = !g_led_want;
        if (g_led_want) {
            (void)et_led_set_blink(&g_led, 200u, 50u, 0u);
        } else {
            et_led_set_off(&g_led);
        }
    }
}

int main(void)
{
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    et_frame_cfg_t fcfg;
    et_key_params_t kp = { 20u, 600u, 0u };
    uint32_t now;

    port_host_tick_set(0u);

    /* 初始化各模块 */
    (void)et_ringbuf_init(&g_uart_rx, g_rb_mem, sizeof(g_rb_mem));

    fcfg.header = hdr;          fcfg.header_len = 2u;
    fcfg.use_len = true;        fcfg.fixed_len = 0u;
    fcfg.crc = ET_FRAME_CRC_SUM8;
    fcfg.use_tail = false;      fcfg.tail = 0u;
    fcfg.rx_buf = g_pay;        fcfg.rx_cap = sizeof(g_pay);
    fcfg.on_frame = on_frame;   fcfg.user = NULL;
    (void)et_frame_parser_init(&g_parser, &fcfg);

    (void)et_atcmd_init(&g_at, g_cmds, 2u, g_line, sizeof(g_line), NULL);
    g_at.on_unknown = on_unknown;

    (void)et_stimer_init(&g_heartbeat, hb_fire, NULL);
    (void)et_stimer_start_periodic(&g_heartbeat, 500u);

    (void)et_sched_register(&g_prod, prod_run, NULL, 60u);
    (void)et_sched_register(&g_cons, cons_run, NULL, 20u);

    (void)et_led_init(&g_led, led_write, NULL);
    (void)et_key_init(&g_key, key_read, key_event, NULL, &kp);

    et_log_set_level(ET_LOG_LEVEL_INFO);
    ET_LOGI("main", "demo start");

    /* 主循环: 推进虚拟时间 5ms/次, 至 2500ms */
    for (now = 0u; now <= 2500u; ) {
        port_host_tick_advance(5u);
        now = port_host_tick_now();

        /* 模拟一次按键: 300ms~380ms 期间按下 */
        g_btn_down = (now >= 300u) && (now <= 380u);

        et_sched_poll_once();
        et_stimer_poll(now);
        et_led_poll(&g_led, now);
        et_key_scan(&g_key, now);
    }

    /* 运行统计 */
    ET_LOGI("main", "frames=%u errs=%u",
            (unsigned)g_parser.frame_count,
            (unsigned)g_parser.err_count);
    return 0;
}

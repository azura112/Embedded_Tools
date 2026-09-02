/**
 * @file    stm32f103_demo.c
 * @brief   STM32F103 (BluePill) 真机最小 demo
 *
 * 功能:
 *   1. 开机经 USART1(115200-8-N-1) 打印版本横幅, LED 连闪 3 次;
 *   2. 心跳: 每 2s 输出一次运行时长日志 (含重启计数与软时钟 UTC 时间);
 *   3. 按键短按: 启动/停止 LED 慢闪 (et_led BLINK 模式);
 *   4. 按键长按: 切换"呼吸灯" (et_led BREATH → et_spwm 软件 PWM 调光,
 *      演示 write 回调直连: 亮度值直接作为软件 PWM 占空比);
 *   5. 掉电参数区 (et_kv, 片内 flash 末端两扇区):
 *      - 重启计数: 上电 +1 写回, 心跳展示 —— 直观验证掉电持久化;
 *      - 软时钟 (et_softclock) 时间恢复: 上电读上次保存的 UTC 秒恢复,
 *        每 30s 持久化一次, 断电时间不跳变。
 *
 * 接线 (BluePill F103C8T6):
 *   - LED  : PC13 板载灯, 低电平点亮;
 *   - 按键 : PA0 对地按键 (内部上拉, 按下为低);
 *   - 串口 : PA9(TX) → USB 转串口 RX, 115200-8-N-1。
 *
 * param 区布局: 片内 flash 末端 16KB 为参数区 (port 契约), 本 demo 使用
 * 其中扇区 14/15 两页作 et_kv 双扇区乒乓; 代码/常量不得落入该区
 * (链接脚本 ASSERT 兜底, 见 port/stm32f103/stm32f103c8t6.ld)。
 *
 * 构建/烧录/仿真见 port/stm32f103/README.md。
 */
#include <stdint.h>
#include <stdbool.h>

#include "et_config.h"
#include "et_log.h"
#include "et_led.h"
#include "et_key.h"
#include "et_stimer.h"
#include "et_spwm.h"
#include "et_kv.h"
#include "et_softclock.h"
#include "port.h"
#include "port_stm32f103.h"
#include "stm32f103_min.h"

#define LED_PIN     13u                 /* PC13, 低电平点亮 */
#define SPWM_CH_LED 0u                  /* LED 所占用的软件 PWM 通道 */

#define KV_SECTOR_A         14u         /* 参数区内扇区序号 (见链接脚本注释) */
#define KV_SECTOR_B         15u
#define KV_KEY_REBOOT       1u          /* 重启计数 (u32) */
#define KV_KEY_UNIX_T       2u          /* 软时钟 UTC 秒 (u32) */
#define SC_PERSIST_MS       30000u      /* 软时钟持久化周期 */
#define UNIX_2026_01_01     1767225600u /* 无保存值时的时间起点 (UTC) */

static et_led_t       g_led;
static et_key_t       g_key;
static et_stimer_t    g_hb;
static et_kv_t        g_kv;
static et_softclock_t g_sc;
static uint32_t       g_boot_count = 0u;
static uint32_t       g_sc_save_at  = 0u;

/* ---- 应用硬件: LED 电平 (经软件 PWM 通道驱动) ---- */

static void spwm_led_write(void *user, uint8_t on)
{
    (void)user;
    if (on != 0u) {
        GPIOC_BRR = (1u << LED_PIN);        /* 点亮: PC13 拉低 */
    } else {
        GPIOC_BSRR = (1u << LED_PIN);       /* 熄灭: PC13 拉高 */
    }
}

/* et_led 亮度输出 → 软件 PWM 占空比 (write 回调直连) */
static void led_brightness_out(void *user, uint8_t brightness)
{
    (void)user;
    (void)et_spwm_set(SPWM_CH_LED, brightness);
}

/* ---- 应用硬件: 按键 ---- */

static bool key_read(void *user)
{
    (void)user;
    return ((GPIOA_IDR & 1u) == 0u);        /* PA0 内部上拉, 按下为低 */
}

static void on_key_event(struct et_key *k, et_key_event_t ev, void *user)
{
    (void)k;
    (void)user;
    switch (ev) {
    case ET_KEY_CLICK:
        if (et_led_set_blink(&g_led, 400u, 50u, 0u)) {      /* 慢闪, 无限 */
            ET_LOGI("demo", "blink on");
        }
        break;
    case ET_KEY_LONG_PRESS:
        (void)et_led_set_breath(&g_led, 2000u);             /* 2s 呼吸 */
        ET_LOGI("demo", "breath on (soft pwm)");
        break;
    case ET_KEY_RELEASE:
        break;
    default:
        break;
    }
}

/* ---- 心跳: 运行信息 + 软时钟周期持久化 ---- */

static void heartbeat(void *arg)
{
    et_datetime_t dt;
    uint32_t now = port_tick_get_ms();

    (void)arg;
    if (et_softclock_get_datetime(&g_sc, &dt)) {
        /* et_log 不支持域宽(%04u), 年月日补零在栈上手工拼 */
        char dt_line[24];
        dt_line[0] = '0' + (char)((dt.year / 1000u) % 10u);
        dt_line[1] = '0' + (char)((dt.year / 100u) % 10u);
        dt_line[2] = '0' + (char)((dt.year / 10u) % 10u);
        dt_line[3] = '0' + (char)(dt.year % 10u);
        dt_line[4] = '-';
        dt_line[5] = '0' + (char)(dt.month / 10u);
        dt_line[6] = '0' + (char)(dt.month % 10u);
        dt_line[7] = '-';
        dt_line[8] = '0' + (char)(dt.day / 10u);
        dt_line[9] = '0' + (char)(dt.day % 10u);
        dt_line[10] = ' ';
        dt_line[11] = '0' + (char)(dt.hour / 10u);
        dt_line[12] = '0' + (char)(dt.hour % 10u);
        dt_line[13] = ':';
        dt_line[14] = '0' + (char)(dt.min / 10u);
        dt_line[15] = '0' + (char)(dt.min % 10u);
        dt_line[16] = ':';
        dt_line[17] = '0' + (char)(dt.sec / 10u);
        dt_line[18] = '0' + (char)(dt.sec % 10u);
        dt_line[19] = '\0';
        ET_LOGI("demo", "alive %u ms | boot #%u | %s (UTC)",
                (unsigned)now, (unsigned)g_boot_count, dt_line);
    } else {
        ET_LOGI("demo", "alive %u ms | boot #%u",
                (unsigned)now, (unsigned)g_boot_count);
    }

    /* 软时钟 UTC 秒周期持久化 (断电恢复用; 2s 心跳下约每 30s 一存) */
    if ((now - g_sc_save_at) >= SC_PERSIST_MS) {
        uint32_t unix_sec = et_softclock_unix(&g_sc);

        (void)et_kv_set(&g_kv, KV_KEY_UNIX_T, &unix_sec, sizeof(unix_sec));
        g_sc_save_at = now;
    }
}

/* et_kv 初始化 + 重启计数 + 软时钟时间恢复 */
static void kv_boot_setup(void)
{
    const et_kv_layout_t layout = { KV_SECTOR_A, KV_SECTOR_B };
    uint32_t unix0 = UNIX_2026_01_01;
    et_kv_stats_t st;
    uint32_t boot = 0u;

    if (!et_kv_init(&g_kv, &layout)) {
        ET_LOGW("demo", "kv invalid, formatting...");
        if (!et_kv_format(&g_kv, &layout)) {
            ET_LOGE("demo", "kv format failed");
        } else {
            (void)et_kv_init(&g_kv, &layout);       /* 格式化后重建活跃页 */
        }
    }

    /* 重启计数: 读出 +1 写回 (首次上电为 1) */
    (void)et_kv_get(&g_kv, KV_KEY_REBOOT, &boot, sizeof(boot), NULL);
    boot = (boot >= 0xFFFFFFFFu) ? 1u : boot + 1u;
    (void)et_kv_set(&g_kv, KV_KEY_REBOOT, &boot, sizeof(boot));
    g_boot_count = boot;

    /* 软时钟恢复: 用上次保存的 UTC 秒, 无则从常量起点开始 */
    (void)et_kv_get(&g_kv, KV_KEY_UNIX_T, &unix0, sizeof(unix0), NULL);
    (void)et_softclock_init(&g_sc, unix0);

    et_kv_stats(&g_kv, &st);
    ET_LOGI("demo", "kv: seq=%u free=%u rec=%u key=%u",
            (unsigned)st.seq, (unsigned)st.free_bytes,
            (unsigned)st.record_count, (unsigned)st.key_count);
}

int main(void)
{
    uint32_t now;

    port_stm32f103_init();                  /* 时钟/引脚/SysTick/串口 */
    et_log_set_level(ET_LOG_LEVEL_INFO);

    ET_LOGI("demo", "Embedded_Tools v%s (0x%x)",
            ET_VERSION_STRING, (unsigned)ET_VERSION);

    kv_boot_setup();                        /* et_kv + 重启计数 + 时间恢复 */

    /* 软件 PWM: 500Hz (2ms), 分辨率 1/2ms —— 见 et_spwm.h 分辨率假设 */
    (void)et_spwm_init(SPWM_CH_LED, spwm_led_write, NULL, 2u);
    (void)et_led_init(&g_led, led_brightness_out, NULL);

    /* 开机连闪 3 次: 400ms 周期闪 3 个周期后自熄 */
    (void)et_led_set_blink(&g_led, 400u, 50u, 3u);

    {
        et_key_params_t kp = { 20u, 600u, 0u };             /* 消抖20/长按600/关连发 */
        (void)et_key_init(&g_key, key_read, on_key_event, NULL, &kp);
    }

    (void)et_stimer_init(&g_hb, heartbeat, NULL);
    (void)et_stimer_start_periodic(&g_hb, 2000u);

    for (;;) {
        now = port_tick_get_ms();

        et_softclock_poll(&g_sc, now);      /* 每 1ms 一次: ms 累计 → 秒进位 */
        et_stimer_poll(now);
        et_led_poll(&g_led, now);
        et_spwm_poll(now);
        et_key_scan(&g_key, now);

        __asm__ __volatile__ ("wfi");       /* 等 SysTick 唤醒, 每 1ms 醒一次 */
    }
}

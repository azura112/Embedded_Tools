/**
 * @file    stm32f103_demo.c
 * @brief   STM32F103 (BluePill) 真机最小 demo
 *
 * 功能:
 *   1. 开机经 USART1(115200-8-N-1) 打印版本横幅, LED 连闪 3 次;
 *   2. 心跳: 每 2s 输出一次运行时长日志;
 *   3. 按键短按: 启动/停止 LED 慢闪 (et_led BLINK 模式);
 *   4. 按键长按: 切换"呼吸灯" (et_led BREATH → et_spwm 软件 PWM 调光,
 *      演示 write 回调直连: 亮度值直接作为软件 PWM 占空比)。
 *
 * 接线 (BluePill F103C8T6):
 *   - LED  : PC13 板载灯, 低电平点亮;
 *   - 按键 : PA0 对地按键 (内部上拉, 按下为低);
 *   - 串口 : PA9(TX) → USB 转串口 RX, 115200-8-N-1。
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
#include "port.h"
#include "port_stm32f103.h"
#include "stm32f103_min.h"

#define LED_PIN     13u                 /* PC13, 低电平点亮 */
#define SPWM_CH_LED 0u                  /* LED 所占用的软件 PWM 通道 */

static et_led_t    g_led;
static et_key_t    g_key;
static et_stimer_t g_hb;

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

/* ---- 心跳 ---- */

static void heartbeat(void *arg)
{
    (void)arg;
    ET_LOGI("demo", "alive %u ms", (unsigned)port_tick_get_ms());
}

int main(void)
{
    uint32_t now;

    port_stm32f103_init();                  /* 时钟/引脚/SysTick/串口 */
    et_log_set_level(ET_LOG_LEVEL_INFO);

    ET_LOGI("demo", "Embedded_Tools v%s (0x%x)",
            ET_VERSION_STRING, (unsigned)ET_VERSION);

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

        et_stimer_poll(now);
        et_led_poll(&g_led, now);
        et_spwm_poll(now);
        et_key_scan(&g_key, now);

        __asm__ __volatile__ ("wfi");       /* 等 SysTick 唤醒, 每 1ms 醒一次 */
    }
}

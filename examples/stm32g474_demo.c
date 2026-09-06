/**
 * @file    stm32g474_demo.c
 * @brief   STM32G474 (G474VET6_ET_TEST 板) 真机最小 demo
 *
 * 功能 (与 stm32f103_demo.c 同一套全栈联动):
 *   1. 开机经 USART1(115200-8-N-1) 打印版本横幅, LED 连闪 3 次;
 *   2. 心跳: 每 2s 输出一次运行时长日志 (含重启计数与软时钟 UTC 时间);
 *   3. 按键短按: 启动/停止 LED 慢闪 (et_led BLINK 模式);
 *   4. 按键长按: 切换"呼吸灯" (et_led BREATH → et_spwm 软件 PWM 调光);
 *   5. 掉电参数区 (et_kv, 片内 flash 末端 32KB = 16×2KB 页):
 *      - 重启计数: 上电 +1 写回, 心跳展示;
 *      - 软时钟时间恢复: 每 30s 持久化 UTC 秒;
 *   6. v1.5 交互壳 + 升级链路: shell(atcmd) → xmodem → bootctl
 *      (AT+VER / AT+BOOTINFO / AT+SIMUPGRADE / AT+UPGRADE / AT+HELP)。
 *
 * 接线 (源 CubeMX 工程 G474VET6_ET_TEST 的引脚规划):
 *   - LED  : PC0 (推挽输出, 高电平点亮; 板载灯低电平点亮时对调 LED_ON/OFF);
 *   - 按键 : PD15 (输入上拉, 按下为低; 源工程为下降沿事件, demo 内轮询扫描);
 *   - 串口 : PA9(TX)/PA10(RX) → USB 转串口, 115200-8-N-1;
 *   - 时钟 : HSI16 ×PLL = 144MHz (与 .ioc 时钟树一致, PLL 失锁回退 16MHz)。
 *
 * param 区布局: 片内 flash 末端 32KB (16×2KB 页, port 契约), 本 demo 使用
 * 扇区 14/15 作 et_kv 双扇区乒乓, 扇区 11/12/13 作 bootctl 槽/状态;
 * 代码/常量不得落入该区 (链接脚本 ASSERT 兜底, 见 stm32g474vet6.ld)。
 * G4 flash 为 64 位双字单次编程, kv/bootctl 布局已按 8B 槽适配
 * (见 port/stm32g474/README.md "flash 约束")。
 *
 * 构建/烧录见 port/stm32g474/README.md。
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "et_config.h"
#include "et_log.h"
#include "et_led.h"
#include "et_key.h"
#include "et_stimer.h"
#include "et_spwm.h"
#include "et_kv.h"
#include "et_softclock.h"
#include "et_bootctl.h"
#include "et_xmodem.h"
#include "et_atcmd.h"
#include "et_shell.h"
#include "et_crc.h"
#include "port.h"
#include "port_stm32g474.h"
#include "stm32g474_min.h"

#define LED_PIN     0u                  /* PC0, 高电平点亮 */
#define SPWM_CH_LED 0u                  /* LED 所占用的软件 PWM 通道 */

#define KV_SECTOR_A         14u         /* 参数区内扇区序号 (16×2KB, 见链接脚本注释) */
#define KV_SECTOR_B         15u
#define KV_KEY_REBOOT       1u          /* 重启计数 (u32) */
#define KV_KEY_UNIX_T       2u          /* 软时钟 UTC 秒 (u32) */
#define SC_PERSIST_MS       30000u      /* 软时钟持久化周期 */
#define UNIX_2026_01_01     1767225600u /* 无保存值时的时间起点 (UTC) */

/* v1.5 升级链路布局: 槽 A/B + 状态扇区 (均参数区内, 与 kv 不重叠) */
#define BOOT_SLOT_A         11u
#define BOOT_SLOT_B         12u
#define BOOT_STATE_SEC      13u
#define BOOT_MAX_ATTEMPTS   2u

static et_led_t       g_led;
static et_key_t       g_key;
static et_stimer_t    g_hb;
static et_kv_t        g_kv;
static et_softclock_t g_sc;
static et_bootctl_t   g_bc;
static et_bootctl_cfg_t g_bcfg = { BOOT_STATE_SEC,
                                   { BOOT_SLOT_A, BOOT_SLOT_B },
                                   PORT_FLASH_SECTOR_SIZE, BOOT_MAX_ATTEMPTS };
static et_xmodem_t    g_xm;
static et_shell_t     g_sh;
static et_atcmd_proc_t g_at;
static uint8_t        g_xmbuf[132];
static char           g_cmdline[48];
static uint32_t       g_boot_count = 0u;
static uint32_t       g_sc_save_at  = 0u;

/* ===================== v1.5 升级链路 (shell → xmodem → bootctl) ===================== */

static void shell_put(void *user, char ch)
{
    (void)user;
    port_putc(ch);                          /* 回显/提示符 → USART1 */
}

static void boot_reset(void)
{
    uint32_t i;

    for (i = 0u; i < 200000u; i++) {        /* ~数十 ms: 等日志/应答发完 */
        __asm__ __volatile__ ("nop");
    }
    SCB_AIRCR = SCB_AIRCR_SYSRESETREQ;
    for (;;) {
        __asm__ __volatile__ ("wfi");
    }
}

static void cmd_ver(char *args, void *user)
{
    (void)args;
    (void)user;
    ET_LOGI("at", "ver=%s boot=%u", ET_VERSION_STRING,
            (unsigned)g_boot_count);
}

static void cmd_bootinfo(char *args, void *user)
{
    et_bootctl_state_t st;

    (void)args;
    (void)user;
    et_bootctl_state(&g_bc, &st);
    ET_LOGI("at", "staged=%d confirmed=%d attempts=%u",
            (int)st.staged_slot, (int)st.confirmed_slot,
            (unsigned)st.attempts);
}

/* xmodem sink: 首块前擦 B 槽, 数据顺序写入 (头 32B + 镜像体);
 * 128B 块 8B 对齐, 满足 G4 双字编程约束 */
static bool xm_sink(void *user, uint32_t off, const uint8_t *d, uint32_t len)
{
    (void)user;
    if (off == 0u) {
        if (!port_flash_erase_sector(BOOT_SLOT_B)) {
            return false;
        }
    }
    return port_flash_write(BOOT_SLOT_B * PORT_FLASH_SECTOR_SIZE + off,
                            d, len) == len;
}

static void xmodem_reply(et_xm_act_t act)
{
    switch (act) {
    case ET_XM_ACK: port_putc((char)ET_XM_ACK_BYTE); break;
    case ET_XM_NAK: port_putc((char)ET_XM_NAK_BYTE); break;
    case ET_XM_CAN: port_putc((char)ET_XM_CAN_BYTE); break;
    default: break;
    }
}

/* AT+UPGRADE: 阻塞收一轮 xmodem (期间主循环/心跳暂停) */
static void cmd_upgrade(char *args, void *user)
{
    bool done = false;

    (void)args;
    (void)user;
    ET_LOGW("at", "send image via XMODEM-CRC (slot B)...");
    et_xmodem_rx_init(&g_xm, g_xmbuf, sizeof(g_xmbuf), xm_sink, NULL);
    while (!done) {
        uint32_t   now = port_tick_get_ms();
        et_xm_act_t a;

        if ((USART1_ISR & USART_ISR_RXNE) != 0u) {
            a = et_xmodem_rx(&g_xm, (uint8_t)USART1_RDR, now);
            xmodem_reply(a);
            if ((a == ET_XM_DONE) || (a == ET_XM_CAN) || (a == ET_XM_ERR)) {
                done = true;
            }
        } else {
            a = et_xmodem_rx_tick(&g_xm, now);
            xmodem_reply(a);
            if ((a == ET_XM_ERR) || (a == ET_XM_CAN)) {
                done = true;
            }
        }
    }
    if ((g_xm.total > 0u) &&
        et_bootctl_verify_image(&g_bc, BOOT_SLOT_B) &&
        et_bootctl_stage(&g_bc, BOOT_SLOT_B)) {
        ET_LOGW("at", "UPGRADE STAGED, rebooting...");
        boot_reset();
    }
    ET_LOGE("at", "upgrade failed/aborted");
}

/* AT+SIMUPGRADE <ver>: 合成 64B 假镜像写 B 槽 (免上位机, 演示全状态机)。
 * ver 为偶数 → 模拟"自检失败"路径; 奇数 → 自检通过可确认 */
static void cmd_simupgrade(char *args, void *user)
{
    uint8_t hdr[32];
    uint8_t img[64];
    char    *cursor = args;
    char    *av = et_atcmd_next_arg(&cursor);
    uint32_t ver = 1u;
    uint32_t crc;
    uint32_t i;

    (void)user;
    if (av != NULL) {
        while ((*av >= '0') && (*av <= '9')) {
            ver = (ver * 10u) + (uint32_t)(*av - '0');
            av++;
        }
    }
    for (i = 0u; i < sizeof(img); i++) {
        img[i] = (uint8_t)(0xC0u + (uint8_t)i);
    }
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 0x45u; hdr[1] = 0x54u; hdr[2] = 0x42u; hdr[3] = 0x49u; /* 'ETBI' */
    hdr[4] = (uint8_t)ET_BOOT_HDR_VER;
    hdr[6] = (uint8_t)ET_BOOT_HDR_SIZE;
    hdr[8]  = (uint8_t)sizeof(img);
    hdr[12] = 0u; hdr[13] = 0u; hdr[14] = 0u; hdr[15] = 0u;     /* 先占位 */
    hdr[16] = (uint8_t)(ver);
    hdr[17] = (uint8_t)(ver >> 8);
    crc = et_crc32_update(ET_CRC32_INIT, img, sizeof(img)) ^ ET_CRC32_INIT;
    hdr[12] = (uint8_t)(crc);
    hdr[13] = (uint8_t)(crc >> 8);
    hdr[14] = (uint8_t)(crc >> 16);
    hdr[15] = (uint8_t)(crc >> 24);
    crc = et_crc32(hdr, 28u);
    hdr[28] = (uint8_t)(crc);
    hdr[29] = (uint8_t)(crc >> 8);
    hdr[30] = (uint8_t)(crc >> 16);
    hdr[31] = (uint8_t)(crc >> 24);

    if (!port_flash_erase_sector(BOOT_SLOT_B)) {
        ET_LOGE("at", "erase slot B failed");
        return;
    }
    if ((port_flash_write(BOOT_SLOT_B * PORT_FLASH_SECTOR_SIZE, hdr, sizeof(hdr)) !=
         sizeof(hdr)) ||
        (port_flash_write(BOOT_SLOT_B * PORT_FLASH_SECTOR_SIZE + sizeof(hdr),
                          img, sizeof(img)) != sizeof(img))) {
        ET_LOGE("at", "write slot B failed");
        return;
    }
    ET_LOGI("at", "sim image ver=%u written", (unsigned)ver);
    if (et_bootctl_verify_image(&g_bc, BOOT_SLOT_B) &&
        et_bootctl_stage(&g_bc, BOOT_SLOT_B)) {
        ET_LOGW("at", "STAGED slot B, rebooting...");
        boot_reset();
    }
    ET_LOGE("at", "verify/stage failed");
}

static const et_atcmd_entry_t g_atcmds[] = {
    { "VER",        cmd_ver,        "print version" },
    { "BOOTINFO",   cmd_bootinfo,   "show bootctl state" },
    { "SIMUPGRADE", cmd_simupgrade, "synthetic image -> slot B (even ver = bad self-check)" },
    { "UPGRADE",    cmd_upgrade,    "xmodem receive -> slot B" },
    { "HELP",       et_shell_help_cmd, "list commands" },
};

/* 开机引导决策段 (bootloader 职责的应用内演示) */
static void boot_flow(void)
{
    et_bootctl_state_t st;
    uint8_t  hdr[32];
    uint32_t img_ver = 0u;

    if (!et_bootctl_init(&g_bc, &g_bcfg)) {
        ET_LOGE("boot", "bootctl init failed");
        return;
    }
    et_bootctl_state(&g_bc, &st);
    if (st.staged_slot < 0) {
        ET_LOGI("boot", "no staged slot");
        return;
    }
    {
        uint32_t n = et_bootctl_boot_attempt(&g_bc, (uint32_t)st.staged_slot);
        uint32_t slot_sec = ((uint32_t)st.staged_slot == 0u) ?
                            BOOT_SLOT_A : BOOT_SLOT_B;

        ET_LOGW("boot", "boot slot %d attempt %u", (int)st.staged_slot,
                (unsigned)n);
        if (et_bootctl_should_rollback(&g_bc, (uint32_t)st.staged_slot)) {
            ET_LOGE("boot", "ROLLBACK: attempts exhausted");
            (void)et_bootctl_abandon(&g_bc);
            return;
        }
        /* 演示自检: 读镜像版本号, 奇数 = 自检通过可确认 */
        if (port_flash_read(slot_sec * PORT_FLASH_SECTOR_SIZE, hdr, sizeof(hdr))) {
            img_ver = (uint32_t)hdr[16] | ((uint32_t)hdr[17] << 8);
        }
        if ((img_ver % 2u) != 0u) {
            if (et_bootctl_confirm(&g_bc, (uint32_t)st.staged_slot)) {
                ET_LOGI("boot", "slot %d CONFIRMED (self-check ok)",
                        (int)st.staged_slot);
            }
        } else {
            ET_LOGW("boot", "self-check FAILED (even ver), not confirmed");
        }
    }
}

/* ---- 应用硬件: LED 电平 (经软件 PWM 通道驱动, PC0 高电平点亮) ---- */

static void spwm_led_write(void *user, uint8_t on)
{
    (void)user;
    if (on != 0u) {
        GPIOC_BSRR = (1u << LED_PIN);       /* 点亮: PC0 拉高 */
    } else {
        GPIOC_BSRR = (1u << (LED_PIN + 16u));   /* 熄灭: PC0 拉低 */
    }
}

/* et_led 亮度输出 → 软件 PWM 占空比 (write 回调直连) */
static void led_brightness_out(void *user, uint8_t brightness)
{
    (void)user;
    (void)et_spwm_set(SPWM_CH_LED, brightness);
}

/* ---- 应用硬件: 按键 (PD15 上拉, 按下为低) ---- */

static bool key_read(void *user)
{
    (void)user;
    return ((GPIOD_IDR & (1u << 15u)) == 0u);
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

    port_stm32g474_init();                  /* 时钟/引脚/SysTick/串口 */
    et_log_set_level(ET_LOG_LEVEL_INFO);

    ET_LOGI("demo", "Embedded_Tools v%s (0x%x)",
            ET_VERSION_STRING, (unsigned)ET_VERSION);

    kv_boot_setup();                        /* et_kv + 重启计数 + 时间恢复 */
    boot_flow();                            /* v1.5: staged 槽引导决策段 */

    /* 软件 PWM: 500Hz (2ms), 分辨率 1/2ms —— 见 et_spwm.h 分辨率假设 */
    (void)et_spwm_init(SPWM_CH_LED, spwm_led_write, NULL, 2u);
    (void)et_led_init(&g_led, led_brightness_out, NULL);

    /* v1.5 交互壳: USART1 RX (PA10 已配 AF7) + AT 命令表 */
    USART1_CR1 |= USART_CR1_RE;
    (void)et_atcmd_init(&g_at, g_atcmds, 5u, g_cmdline, sizeof(g_cmdline), &g_sh);
    (void)et_shell_init(&g_sh, &g_at, shell_put, NULL);
    et_shell_set_prompt(&g_sh, "ET> ");

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

        if ((USART1_ISR & USART_ISR_RXNE) != 0u) {
            (void)et_shell_feed(&g_sh, (char)USART1_RDR);   /* 交互壳收字节 */
        }
        et_softclock_poll(&g_sc, now);      /* 每 1ms 一次: ms 累计 → 秒进位 */
        et_stimer_poll(now);
        et_led_poll(&g_led, now);
        et_spwm_poll(now);
        et_key_scan(&g_key, now);

        __asm__ __volatile__ ("wfi");       /* 等 SysTick 唤醒, 每 1ms 醒一次 */
    }
}

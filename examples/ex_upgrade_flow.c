/**
 * @file    ex_upgrade_flow.c
 * @brief   配方可执行载体 #3: 安全升级流程 (xmodem + bootctl 组合的 host 版)
 *
 * 流程 (API_GUIDE 5.4/5.5/6.2 组合): 镜像(头+体)经 et_xmodem_tx → 内存线路
 *               → et_xmodem_rx → sink 直写 port_flash(B 槽, 首块前擦除)
 *               → et_bootctl_verify_image / stage → 模拟重启(re-init)
 *               → boot_attempt 计数 → 应用自检 → confirm;回滚路径:
 *               自检不过 → 反复重启至 attempts≥max → should_rollback →
 *               abandon 回干净态。
 *
 * 全部使用公开 API + host 虚拟 flash(state=6, A=7, B=8); "应用自检"沿用
 * 板上 demo 约定(镜像版本奇=过/偶=不过), 纯应用语义非库行为。
 *
 * 自检(确定性): 回环写入逐字节一致; confirm 路径状态机 staged→confirmed;
 *               回滚路径 attempts 递增→should_rollback→abandon 清态。
 *
 * 运行: make ex; 退出码 0=PASS。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "et_bootctl.h"
#include "et_crc.h"
#include "et_bytes.h"
#include "et_xmodem.h"
#include "et_xmodem_tx.h"
#include "port.h"
#include "port_host.h"

static int g_fail;

#define EX_CHECK(cond)                                                      \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("  FAIL: %s (line %d)\n", #cond, __LINE__);              \
            g_fail++;                                                       \
        }                                                                   \
    } while (0)

#define IMG_SIZE        64u
#define SLOT_B          8u
#define SLOT_B_OFF      (SLOT_B * PORT_FLASH_SECTOR_SIZE)

static const et_bootctl_cfg_t g_bcfg = {
    6u, { 7u, 8u }, PORT_FLASH_SECTOR_SIZE, 2u      /* state/A/B, max=2 */
};

/* ---- 镜像与传输状态 ---- */
static uint8_t  g_image[ET_BOOT_HDR_SIZE + IMG_SIZE];
static et_xmodem_tx_t g_tx;
static et_xmodem_t    g_rx;
static uint8_t        g_rx_blk[132];
static uint8_t        g_reply[16];
static uint32_t       g_reply_n;
static uint32_t       g_rx_now;
static uint32_t       g_tx_now;
static bool           g_rx_done;

/* xmodem sink: 首块前擦槽, 数据直写 flash 参数区 (demo AT+UPGRADE 同款) */
static bool flash_sink(void *user, uint32_t off, const uint8_t *d, uint32_t len)
{
    (void)user;
    if (off == 0u) {
        if (!port_flash_erase_sector(SLOT_B)) {
            return false;
        }
    }
    return port_flash_write(SLOT_B_OFF + off, d, len) == len;
}

static void wire_putc(void *user, uint8_t b)
{
    et_xm_act_t a;

    (void)user;
    a = et_xmodem_rx(&g_rx, b, g_rx_now++);
    if (a == ET_XM_ACK) {
        g_reply[g_reply_n++] = ET_XM_ACK_BYTE;
    } else if (a == ET_XM_NAK) {
        g_reply[g_reply_n++] = ET_XM_NAK_BYTE;
    } else if (a == ET_XM_CAN) {
        g_reply[g_reply_n++] = ET_XM_CAN_BYTE;
    } else if (a == ET_XM_DONE) {
        g_rx_done = true;
        g_reply[g_reply_n++] = ET_XM_ACK_BYTE;
    }
}

static uint32_t image_src(void *user, uint32_t off, uint8_t *dst, uint32_t want)
{
    (void)user;
    if (off >= sizeof(g_image)) {
        return 0u;
    }
    if ((off + want) > sizeof(g_image)) {
        want = sizeof(g_image) - off;
    }
    memcpy(dst, g_image + off, want);
    return want;
}

/* 组帧: 'ETBI' 头(32B) + 镜像体; ver 奇/偶 = 板上 demo 的自检约定 */
static void build_image(uint32_t ver)
{
    uint32_t crc;
    uint32_t i;

    for (i = 0u; i < IMG_SIZE; i++) {
        g_image[ET_BOOT_HDR_SIZE + i] = (uint8_t)(0xC0u + i + (uint8_t)ver);
    }
    memset(g_image, 0, ET_BOOT_HDR_SIZE);
    g_image[0] = 0x45u; g_image[1] = 0x54u;             /* 'ETBI' */
    g_image[2] = 0x42u; g_image[3] = 0x49u;
    g_image[4] = (uint8_t)ET_BOOT_HDR_VER;
    g_image[6] = (uint8_t)ET_BOOT_HDR_SIZE;
    g_image[8]  = (uint8_t)(IMG_SIZE);
    g_image[9]  = (uint8_t)(IMG_SIZE >> 8);
    /* 头字段为小端 (与 pack_image.py / 板上 demo 同约定, 见 et_bootctl.h 布局表) */
    crc = et_crc32_update(ET_CRC32_INIT,
                          g_image + ET_BOOT_HDR_SIZE, IMG_SIZE) ^ ET_CRC32_INIT;
    EX_CHECK(et_bytes_le32_put(g_image, sizeof(g_image), 12u, crc));
    EX_CHECK(et_bytes_le32_put(g_image, sizeof(g_image), 16u, ver));
    crc = et_crc32(g_image, 28u);
    EX_CHECK(et_bytes_le32_put(g_image, sizeof(g_image), 28u, crc));
}

/* xmodem 回环传输当前 g_image → flash B 槽 */
static void transfer_image(void)
{
    et_xmodem_tx_cfg_t cfg;
    uint32_t i;

    memset(&g_rx, 0, sizeof(g_rx));
    et_xmodem_rx_init(&g_rx, g_rx_blk, sizeof(g_rx_blk), flash_sink, NULL);
    memset(&g_tx, 0, sizeof(g_tx));
    memset(&cfg, 0, sizeof(cfg));
    cfg.putc           = wire_putc;
    cfg.src            = image_src;
    cfg.user           = NULL;
    cfg.total          = sizeof(g_image);
    cfg.block_size     = 128u;
    cfg.retry_max      = 10u;
    cfg.ack_timeout_ms = 1000u;
    EX_CHECK(et_xmodem_tx_init(&g_tx, &cfg));
    g_reply_n = 0u; g_rx_now = 0u; g_tx_now = 0u; g_rx_done = false;
    g_reply[g_reply_n++] = ET_XM_NAK_BYTE;
    for (i = 0u; (i < 2000u) && !et_xmodem_tx_done(&g_tx) &&
                 !et_xmodem_tx_aborted(&g_tx); i++) {
        if (g_reply_n > 0u) {
            uint8_t b = g_reply[0];

            memmove(g_reply, g_reply + 1u, g_reply_n - 1u);
            g_reply_n--;
            (void)et_xmodem_tx_poll(&g_tx, b, g_tx_now++);
        } else {
            g_tx_now += 100u;
            (void)et_xmodem_tx_tick(&g_tx, g_tx_now);
        }
    }
    EX_CHECK(g_rx_done);
    EX_CHECK(et_xmodem_tx_done(&g_tx));
}

/* 模拟一次开机: 返回增量后的 attempt 计数 */
static uint32_t reboot(et_bootctl_t *bc)
{
    EX_CHECK(et_bootctl_init(bc, &g_bcfg));
    return et_bootctl_boot_attempt(bc, 1u);     /* staged 槽计数, 其余 0 */
}

int main(void)
{
    static et_bootctl_t bc;
    et_bootctl_state_t  st;
    uint8_t             rb[ET_BOOT_HDR_SIZE + IMG_SIZE];

    printf("== ex_upgrade_flow: xmodem + bootctl 升级流程 (host 版) ==\n");
    port_host_flash_reset();

    /* ===== 路径 A: 自检通过 → confirm ===== */
    build_image(3u);                            /* ver=3 (奇) */
    transfer_image();
    EX_CHECK(port_flash_read(SLOT_B_OFF, rb, sizeof(rb)));
    EX_CHECK(memcmp(rb, g_image, sizeof(rb)) == 0);     /* 落盘逐字节一致 */

    EX_CHECK(et_bootctl_init(&bc, &g_bcfg));
    EX_CHECK(et_bootctl_verify_image(&bc, 1u));
    EX_CHECK(et_bootctl_stage(&bc, 1u));
    et_bootctl_state(&bc, &st);
    EX_CHECK(st.staged_slot == 1);
    printf("  [A] staged slot B (ver=3)\n");

    (void)reboot(&bc);                          /* 开机 #1 */
    EX_CHECK(et_bootctl_verify_image(&bc, 1u)); /* 应用自检: CRC 过 + 奇版本 */
    EX_CHECK(et_bootctl_confirm(&bc, 1u));
    et_bootctl_state(&bc, &st);
    EX_CHECK(st.staged_slot == 1);
    EX_CHECK(st.confirmed_slot == 1);
    EX_CHECK(!et_bootctl_should_rollback(&bc, 1u));
    printf("  [A] confirmed after 1 boot (attempts=1)\n");

    /* ===== 路径 B: 自检不过 → 超次回滚 ===== */
    EX_CHECK(et_bootctl_abandon(&bc));          /* 新一轮升级: 清旧状态 */
    et_bootctl_state(&bc, &st);
    EX_CHECK((st.staged_slot < 0) && (st.confirmed_slot < 0));

    build_image(4u);                            /* ver=4 (偶) = 自检不过 */
    transfer_image();
    EX_CHECK(et_bootctl_init(&bc, &g_bcfg));
    EX_CHECK(et_bootctl_verify_image(&bc, 1u)); /* CRC 完整(库不判版本语义) */
    EX_CHECK(et_bootctl_stage(&bc, 1u));

    EX_CHECK(reboot(&bc) == 1u);                /* 开机 #1: 自检不过, 不 confirm */
    EX_CHECK(!et_bootctl_should_rollback(&bc, 1u));
    (void)reboot(&bc);                          /* 开机 #2 */
    EX_CHECK(et_bootctl_should_rollback(&bc, 1u));      /* attempts>=max */
    printf("  [B] attempts=2, self-check keeps failing\n");
    EX_CHECK(et_bootctl_abandon(&bc));          /* 回滚: 回干净态 */
    et_bootctl_state(&bc, &st);
    EX_CHECK(st.staged_slot < 0);
    EX_CHECK(st.confirmed_slot < 0);
    EX_CHECK(st.attempts == 0u);
    printf("  [B] rolled back to clean state\n");

    printf("[ex_upgrade_flow] %s\n", (g_fail == 0) ? "PASS" : "FAIL");
    return (g_fail == 0) ? 0 : 1;
}

/**
 * @file    ex_kv_backup.c
 * @brief   配方可执行载体 #2: kv 参数备份与恢复 (API_GUIDE 11.11 的 host 版)
 *
 * 流程 (11.11): kvA 写入 N 个 key → et_kv_iter 枚举导出(帧 = key(u16,BE)
 *               + len(u16,BE) + value + crc16(u16,BE)) → et_xmodem_tx 发送
 *               → 内存线路回环 → et_xmodem_rx 接收 → 解帧重灌 kvB(先 format)
 *               → 逐 key 比对一致。
 *
 * 全部使用公开 API + host 虚拟 flash(kvA={2,3}, kvB={4,5} 两套扇区互不重叠),
 * 线路为 tx.putc → rx.feed 的纯内存回环(与库测试同约定: DONE 以 ACK 收尾)。
 *
 * 自检(确定性): xmodem 回环载荷逐字节一致; kvB 逐 key 值比对一致;
 *               key 数一致; 备份帧 CRC 全部通过。
 *
 * 运行: make ex; 退出码 0=PASS。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "et_kv.h"
#include "et_bytes.h"
#include "et_crc.h"
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

#define N_KEYS          8u
#define FRAME_MAX       (4u + ET_KV_VAL_MAX)
#define BACKUP_CAP      (N_KEYS * FRAME_MAX)
#define RECV_CAP        (BACKUP_CAP + 64u)      /* xmodem 尾块填充余量 */

static const et_kv_layout_t lay_a = { 2u, 3u };     /* kvA 扇区(参数区内) */
static const et_kv_layout_t lay_b = { 4u, 5u };     /* kvB 扇区(恢复目标) */

/* ---- 导出侧状态 ---- */
static et_kv_t   g_kv_a;
static et_kv_t   g_kv_b;
static uint8_t   g_backup[BACKUP_CAP];
static uint32_t  g_backup_n;
static uint16_t  g_key_of[N_KEYS];                  /* 导出序 → key 对照 */
static uint32_t  g_exported;

/* ---- xmodem 内存回环 (tx.putc → rx; rx 应答 → tx.poll 队列) ---- */
static et_xmodem_tx_t g_tx;
static et_xmodem_t    g_rx;
static uint8_t        g_rx_blk[132];
static uint8_t        g_got[RECV_CAP];
static uint32_t       g_got_n;
static uint8_t        g_reply[16];
static uint32_t       g_reply_n;
static uint32_t       g_rx_now;
static uint32_t       g_tx_now;
static bool           g_rx_done;

static bool rx_sink(void *user, uint32_t off, const uint8_t *d, uint32_t len)
{
    (void)user;
    if ((off != g_got_n) || ((off + len) > sizeof(g_got))) {
        return false;
    }
    memcpy(g_got + off, d, len);
    g_got_n += len;
    return true;
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
        g_rx_done = true;               /* 接收侧以 ACK 收尾 */
        g_reply[g_reply_n++] = ET_XM_ACK_BYTE;
    }
}

/* tx 数据源 = 备份帧缓冲 */
static uint32_t backup_src(void *user, uint32_t off, uint8_t *dst, uint32_t want)
{
    (void)user;
    if (off >= g_backup_n) {
        return 0u;
    }
    if ((off + want) > g_backup_n) {
        want = g_backup_n - off;
    }
    memcpy(dst, g_backup + off, want);
    return want;
}

static void seed_values(uint16_t k, uint8_t *buf, uint16_t *len)
{
    uint32_t i;

    for (i = 0u; i < 20u; i++) {
        buf[i] = (uint8_t)(k * 11u + i * 7u + 1u);
    }
    *len = (uint16_t)(4u + (k % 5u) * 4u);          /* 4..20B 变长 */
}

int main(void)
{
    et_kv_stats_t st_a;
    et_kv_stats_t st_b;
    et_kv_iter_t  it;
    uint16_t      k;
    uint16_t      len;
    uint8_t       val[20];
    uint32_t      i;

    printf("== ex_kv_backup: API_GUIDE 11.11 kv 参数备份与恢复 (host 版) ==\n");
    port_host_flash_reset();

    /* ---- 1. 源库 kvA: 写入 8 个变长 key ---- */
    EX_CHECK(et_kv_init(&g_kv_a, &lay_a));
    EX_CHECK(et_kv_format(&g_kv_a, &lay_a));
    EX_CHECK(et_kv_init(&g_kv_a, &lay_a));
    for (k = 1u; k <= N_KEYS; k++) {
        seed_values(k, val, &len);
        EX_CHECK(et_kv_set(&g_kv_a, k, val, len));
    }
    et_kv_stats(&g_kv_a, &st_a);
    EX_CHECK(st_a.key_count == N_KEYS);
    printf("  kvA: %u keys seeded (records=%u)\n",
           (unsigned)st_a.key_count, (unsigned)st_a.record_count);

    /* ---- 2. 导出: iter 枚举 → 帧 = key(BE16)+len(BE16)+value+crc16(BE16) ---- */
    EX_CHECK(et_kv_iter_init(&g_kv_a, &it));
    g_backup_n = 0u;
    g_exported = 0u;
    while (et_kv_iter_next(&g_kv_a, &it, &k, &len)) {
        uint16_t crc;

        EX_CHECK(et_kv_get(&g_kv_a, k, val, sizeof(val), NULL));
        EX_CHECK((g_backup_n + 4u + (uint32_t)len) <= sizeof(g_backup));
        EX_CHECK(et_bytes_be16_put(g_backup, sizeof(g_backup), g_backup_n, k));
        EX_CHECK(et_bytes_be16_put(g_backup, sizeof(g_backup),
                                   g_backup_n + 2u, len));
        memcpy(g_backup + g_backup_n + 4u, val, len);
        crc = et_crc16_modbus(g_backup + g_backup_n, (uint32_t)len + 4u);
        EX_CHECK(et_bytes_be16_put(g_backup, sizeof(g_backup),
                                   g_backup_n + 4u + (uint32_t)len, crc));
        g_key_of[g_exported] = k;
        g_exported++;
        g_backup_n += (uint32_t)len + 6u;
    }
    EX_CHECK(g_exported == N_KEYS);
    printf("  backup: %u frames, %u bytes\n",
           (unsigned)g_exported, (unsigned)g_backup_n);

    /* ---- 3. xmodem 内存回环传输 ---- */
    memset(&g_rx, 0, sizeof(g_rx));
    et_xmodem_rx_init(&g_rx, g_rx_blk, sizeof(g_rx_blk), rx_sink, NULL);
    memset(&g_tx, 0, sizeof(g_tx));
    {
        et_xmodem_tx_cfg_t cfg;

        memset(&cfg, 0, sizeof(cfg));
        cfg.putc           = wire_putc;
        cfg.src            = backup_src;
        cfg.user           = NULL;
        cfg.total          = g_backup_n;
        cfg.block_size     = 128u;
        cfg.retry_max      = 10u;
        cfg.ack_timeout_ms = 1000u;
        EX_CHECK(et_xmodem_tx_init(&g_tx, &cfg));
    }
    g_got_n = 0u;
    g_reply_n = 0u;
    g_rx_now = 0u;
    g_tx_now = 0u;
    g_rx_done = false;
    g_reply[g_reply_n++] = ET_XM_NAK_BYTE;          /* 接收方催第一块 */
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
    EX_CHECK(g_got_n >= g_backup_n);                /* 尾块含 0x1A 填充 */
    EX_CHECK(memcmp(g_got, g_backup, g_backup_n) == 0);   /* 逐字节一致 */
    printf("  xmodem: %u bytes transferred (wire payload %u)\n",
           (unsigned)g_got_n, (unsigned)g_backup_n);

    /* ---- 4. 恢复: 解帧重灌 kvB (先 format) ---- */
    EX_CHECK(et_kv_init(&g_kv_b, &lay_b));
    EX_CHECK(et_kv_format(&g_kv_b, &lay_b));
    EX_CHECK(et_kv_init(&g_kv_b, &lay_b));
    {
        uint32_t off = 0u;
        uint32_t restored = 0u;

        while ((off + 6u) <= g_backup_n) {
            uint16_t rk = 0u;
            uint16_t rl = 0u;
            uint16_t rc = 0u;
            uint16_t crc;

            EX_CHECK(et_bytes_be16_get(g_got, g_got_n, off, &rk));
            EX_CHECK(et_bytes_be16_get(g_got, g_got_n, off + 2u, &rl));
            if (((off + 4u + (uint32_t)rl + 2u) > g_backup_n) ||
                (rl > sizeof(val))) {
                break;                              /* 帧尾填充区 */
            }
            EX_CHECK(et_bytes_be16_get(g_got, g_got_n,
                                       off + 4u + (uint32_t)rl, &rc));
            crc = et_crc16_modbus(g_got + off, (uint32_t)rl + 4u);
            EX_CHECK(crc == rc);                    /* 帧校验 */
            memcpy(val, g_got + off + 4u, rl);
            EX_CHECK(et_kv_set(&g_kv_b, rk, val, rl));
            off += (uint32_t)rl + 6u;
            restored++;
        }
        EX_CHECK(restored == N_KEYS);
        printf("  restored: %u keys into kvB\n", (unsigned)restored);
    }

    /* ---- 5. 逐 key 比对: kvA ↔ kvB ---- */
    et_kv_stats(&g_kv_b, &st_b);
    EX_CHECK(st_b.key_count == st_a.key_count);
    EX_CHECK(et_kv_iter_init(&g_kv_a, &it));
    while (et_kv_iter_next(&g_kv_a, &it, &k, &len)) {
        uint8_t va[20];
        uint8_t vb[20];
        uint16_t lb = 0u;

        EX_CHECK(et_kv_get(&g_kv_a, k, va, sizeof(va), NULL));
        EX_CHECK(et_kv_get(&g_kv_b, k, vb, sizeof(vb), &lb));
        EX_CHECK(lb == len);
        EX_CHECK(memcmp(va, vb, len) == 0);
    }
    printf("  compare: %u/%u keys identical\n",
           (unsigned)st_b.key_count, (unsigned)st_a.key_count);

    printf("[ex_kv_backup] %s\n", (g_fail == 0) ? "PASS" : "FAIL");
    return (g_fail == 0) ? 0 : 1;
}

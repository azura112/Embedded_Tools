/**
 * @file    bench.c
 * @brief   host 基准套件 (v1.7 P2, 两次顺延收编)
 *
 * 度量对象: ringbuf 吞吐 / crc 吞吐(ccitt, 查表变体另编译) / xmodem 有效
 * 吞吐 / kv 操作速率 / filter+fsm 单次耗时。
 *
 * 方法约定 (docs/bench.md 同步):
 *   - 固定迭代数 + 5 轮取中位数 (抗单次抖动);
 *   - 计时用 clock() (进程 CPU 时间);
 *   - 防 DCE: 每轮结果写入 volatile 汇聚变量 —— 否则 -O2 会把纯计算循环
 *     整体优化掉, "吞吐"变成测量空调循环 (本套件开发中实测 crc 虚高 4 倍);
 *   - 【不承诺跨机器可比性】: 数字仅用于同机版本间回归对比与量级判断;
 *   - 正式数字须以 -O2 构建并附环境注记 (CPU/编译器/flags) 后写入 docs/bench.md。
 *
 * 编译: 见 Makefile `bench` 目标; 查表 CRC 变体: 追加 -DET_CRC_TABLE=1。
 */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "et_config.h"
#include "et_ringbuf.h"
#include "et_crc.h"
#include "et_filter.h"
#include "et_fsm.h"
#include "et_xmodem.h"
#include "et_kv.h"

#define ROUNDS      5u      /* 轮数, 取中位数 */

/* 防 DCE(死代码消除): 每轮结果写入 volatile 汇聚变量 */
static volatile uint32_t g_sink;

static double rounds_sec[ROUNDS];

/* 跑 ROUNDS 轮取中位数 (秒) */
static double median_of(double (*fn)(void))
{
    uint32_t i;
    uint32_t j;

    for (i = 0u; i < ROUNDS; i++) {
        rounds_sec[i] = fn();
    }
    for (i = 1u; i < ROUNDS; i++) {         /* 插入排序 (5 元素) */
        double key = rounds_sec[i];

        for (j = i; (j > 0u) && (rounds_sec[j - 1u] > key); j--) {
            rounds_sec[j] = rounds_sec[j - 1u];
        }
        rounds_sec[j] = key;
    }
    return rounds_sec[ROUNDS / 2u];
}

/* ===================== 各基准 (单轮, 返回耗时秒) ===================== */

/* ringbuf: 单字节流 (写 1 读 1) */
static et_ringbuf_t b_rb;
static uint8_t      b_rb_mem[4096];
static uint8_t      b_rb_mem2[4000];        /* 非 POW2 */
static uint8_t      b_byte;

static double bench_rb_byte(void)
{
    uint32_t n = 2000000u;
    uint32_t i;
    clock_t  t0;

    (void)et_ringbuf_init(&b_rb, b_rb_mem, sizeof(b_rb_mem));
    b_byte = 0u;
    t0 = clock();
    for (i = 0u; i < n; i++) {
        (void)et_ringbuf_write(&b_rb, &b_byte, 1u);
        (void)et_ringbuf_read(&b_rb, &b_byte, 1u);
        b_byte++;
    }
    g_sink = b_byte;
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

/* ringbuf: 块粒度 (写 256B 读 256B) */
static uint8_t b_blk[256];

static double bench_rb_block_pow2(void)
{
    uint32_t n = 5000u;
    uint32_t i;
    clock_t  t0;

    (void)et_ringbuf_init(&b_rb, b_rb_mem, sizeof(b_rb_mem));
    t0 = clock();
    for (i = 0u; i < n; i++) {
        (void)et_ringbuf_write(&b_rb, b_blk, sizeof(b_blk));
        (void)et_ringbuf_read(&b_rb, b_blk, sizeof(b_blk));
    }
    g_sink = b_blk[0];
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

static double bench_rb_block_nonpow2(void)
{
    uint32_t n = 5000u;
    uint32_t i;
    clock_t  t0;

    (void)et_ringbuf_init(&b_rb, b_rb_mem2, sizeof(b_rb_mem2));
    t0 = clock();
    for (i = 0u; i < n; i++) {
        (void)et_ringbuf_write(&b_rb, b_blk, sizeof(b_blk));
        (void)et_ringbuf_read(&b_rb, b_blk, sizeof(b_blk));
    }
    g_sink = b_blk[1];
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

/* crc: 4KB 缓冲反复校验 (ccitt: 表加速唯一覆盖的变体) */
static uint8_t b_crc_buf[4096];

static double bench_crc16(void)
{
    uint32_t iters = 512u;                  /* 512 x 4KB = 2MB */
    uint32_t i;
    uint16_t crc = 0u;
    clock_t  t0;

    t0 = clock();
    for (i = 0u; i < iters; i++) {
        crc = et_crc16_ccitt_update(crc, b_crc_buf, sizeof(b_crc_buf));
    }
    g_sink = crc;
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

static double bench_crc32(void)
{
    uint32_t iters = 512u;
    uint32_t i;
    uint32_t crc = ET_CRC32_INIT;
    clock_t  t0;

    t0 = clock();
    for (i = 0u; i < iters; i++) {
        crc = et_crc32_update(crc, b_crc_buf, sizeof(b_crc_buf));
    }
    g_sink = crc;
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

/* xmodem: 编码块流直接喂接收器 (RAM sink), 含协议开销的净吞吐 */
static et_xmodem_t b_xm;
static uint8_t     b_xm_buf[132];
static uint8_t     b_xm_sink[256u * 128u];
static uint32_t    b_xm_len;
static uint8_t     b_xm_frame[133];

static bool bench_xm_sink(void *user, uint32_t off, const uint8_t *d, uint32_t len)
{
    (void)user;
    memcpy(b_xm_sink + off, d, len);
    b_xm_len = off + len;
    return true;
}

static double bench_xmodem(void)
{
    uint32_t blocks = 20000u;               /* 20000 x 128B = 2.56MB 载荷/轮 */
    uint32_t blk;
    uint32_t i;
    uint32_t now = 0u;
    clock_t  t0;
    uint16_t crc;

    memset(&b_xm, 0, sizeof(b_xm));
    b_xm_len = 0u;
    et_xmodem_rx_init(&b_xm, b_xm_buf, sizeof(b_xm_buf), bench_xm_sink, NULL);
    memset(b_xm_frame + 3u, 0xA5u, 128u);

    t0 = clock();
    for (blk = 0u; blk < blocks; blk++) {
        crc = et_crc16_ccitt_update(0x0000u, b_xm_frame + 3u, 128u);
        b_xm_frame[0] = ET_XM_SOH;
        b_xm_frame[1] = (uint8_t)((blk % 256u) + 1u);
        b_xm_frame[2] = (uint8_t)(~b_xm_frame[1]);
        b_xm_frame[131] = (uint8_t)(crc >> 8);
        b_xm_frame[132] = (uint8_t)(crc & 0xFFu);
        for (i = 0u; i < sizeof(b_xm_frame); i++) {
            (void)et_xmodem_rx(&b_xm, b_xm_frame[i], now++);
        }
    }
    g_sink = b_xm_len;
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

/* kv: host 虚拟 flash 上 set(追加)/get 交替 */
static et_kv_t b_kv;

static double bench_kv(void)
{
    static const et_kv_layout_t lay = { 14u, 15u };
    uint32_t ops = 2000u;
    uint32_t i;
    uint32_t val = 0u;
    uint32_t out = 0u;
    clock_t  t0;

    memset(&b_kv, 0, sizeof(b_kv));
    (void)et_kv_format(&b_kv, &lay);
    t0 = clock();
    for (i = 0u; i < ops; i++) {
        val = i * 2654435761u;
        (void)et_kv_set(&b_kv, (uint16_t)((i % 50u) + 1u), &val, sizeof(val));
        (void)et_kv_get(&b_kv, (uint16_t)((i % 50u) + 1u), &out, sizeof(out), NULL);
    }
    g_sink = out;
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

/* filter/fsm: 单次耗时 */
static et_movavg_t b_mv;
static int32_t     b_mv_mem[8];

static double bench_filter(void)
{
    uint32_t n = 1000000u;
    uint32_t i;
    int32_t  acc = 0;
    clock_t  t0;

    memset(&b_mv, 0, sizeof(b_mv));
    (void)et_movavg_init(&b_mv, b_mv_mem, 8u);
    t0 = clock();
    for (i = 0u; i < n; i++) {
        acc += et_movavg_update(&b_mv, (int32_t)i);
    }
    g_sink = (uint32_t)acc;
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

static et_fsm_t b_fsm;

static bool b_fsm_guard(void *u)
{
    (void)u;
    return true;
}

static double bench_fsm(void)
{
    static const et_fsm_trans_t tbl[] = {
        { 1u, 1u, b_fsm_guard, NULL },
        { 2u, 0u, NULL,        NULL },
    };
    uint32_t n = 1000000u;
    uint32_t i;
    uint32_t ev = 1u;
    clock_t  t0;

    memset(&b_fsm, 0, sizeof(b_fsm));
    (void)et_fsm_init(&b_fsm, 0u, tbl, 2u, NULL);
    t0 = clock();
    for (i = 0u; i < n; i++) {
        (void)et_fsm_dispatch(&b_fsm, ev);
        ev = (ev == 1u) ? 2u : 1u;
    }
    g_sink = (uint32_t)et_fsm_state(&b_fsm);
    return (double)(clock() - t0) / CLOCKS_PER_SEC;
}

/* ===================== 报告 ===================== */

static void report_mb(const char *name, double (*fn)(void),
                      double bytes_per_round)
{
    double sec = median_of(fn);

    printf("%-44s %10.1f MB/s   (median %6.1f ms)\n",
           name, bytes_per_round / sec / (1024.0 * 1024.0), sec * 1000.0);
}

static void report_ns(const char *name, double (*fn)(void), double ops)
{
    double sec = median_of(fn);

    printf("%-44s %10.1f ns/op   (median %6.1f ms)\n",
           name, sec * 1e9 / ops, sec * 1000.0);
}

static void report_ops(const char *name, double (*fn)(void), double ops)
{
    double sec = median_of(fn);

    printf("%-44s %10.0f ops/s   (median %6.1f ms)\n",
           name, ops / sec, sec * 1000.0);
}

int main(void)
{
    printf("Embedded_Tools host bench (rounds=%u, median)\n", ROUNDS);
#if ET_CRC_TABLE
    printf("build: table CRC\n");
#else
    printf("build: bitwise CRC\n");
#endif
    printf("------------------------------------------------------------\n");

    memset(b_blk, 0xA5u, sizeof(b_blk));
    memset(b_crc_buf, 0x5Au, sizeof(b_crc_buf));

    report_mb("ringbuf byte 1B (cap 4096)", bench_rb_byte, 2000000.0);
    report_mb("ringbuf block 256B (cap 4096 POW2)", bench_rb_block_pow2,
              5000.0 * 256.0);
    report_mb("ringbuf block 256B (cap 4000 non-POW2)", bench_rb_block_nonpow2,
              5000.0 * 256.0);
    report_mb("crc16-ccitt (4KB x512; table iff ET_CRC_TABLE=1)",
              bench_crc16, 512.0 * 4096.0);
    report_mb("crc32 bitwise (4KB x512)", bench_crc32, 512.0 * 4096.0);
    report_mb("xmodem eff. payload 128B blocks", bench_xmodem, 20000.0 * 128.0);
    report_ops("kv set+get (32B val, host flash)", bench_kv, 2000.0);
    report_ns("filter movavg update", bench_filter, 1000000.0);
    report_ns("fsm dispatch (guard)", bench_fsm, 1000000.0);

    printf("------------------------------------------------------------\n");
    printf("note: numbers are for same-machine version regression only,\n");
    printf("      NOT cross-machine comparable. record env in docs/bench.md\n");
    return 0;
}

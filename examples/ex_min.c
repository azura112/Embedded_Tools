/**
 * @file    ex_min.c
 * @brief   配方可执行载体 #6: 十分钟最小样机 —— ringbuf 字节流 + kv 参数持久化
 *
 * 定位（v2.29 P0-3，`REQ-18` 采纳前置 1.3）：第一次使用本库的工程师在干净机器上按
 * `docs/getting-started.md` §0.5 的一条命令跑通两件事——
 *   ① core/et_ringbuf：单生产者/单消费者字节环形缓冲；
 *   ② storage/et_kv：掉电安全的参数区 KV（host 侧 = port_host 虚拟 flash，重启即再 init）。
 * 全程只用公开 API + host port，零动态内存（句柄与存储均由本文件静态分配）。
 *
 * 流程（确定性，无随机、无时间依赖）：
 *   1) ringbuf init → 写两帧 → 回读逐字节比对 → 满态短写与空态读 0；
 *   2) kv format → set 两个变长 key → get 回读比对；
 *   3) 模拟重启 = 仅 et_kv_init（不 format）→ 读回原值，证明持久化；
 *   4) 覆盖写同一 key → 再重启读回新值（后写生效语义）。
 *
 * 预期输出（`make ex` 第 6 配方）：
 *   [ex_min] start
 *     ringbuf: cap=16 wrote=7 read=7 bytes match
 *     ringbuf: full-path short write 1 of 15 (free=1), empty read 0
 *     kv: format+init ok, keys=2
 *     kv: read-back match (k=1 len=4, k=2 len=11)
 *     kv: after re-init without format (simulated reboot) k=1 len=4 first byte=0xA0
 *     kv: overwrite persisted (k=1 len=2, first byte=0x5A)
 *   [ex_min] PASS
 *
 * 运行: make ex；或单跑 build/ex_min.exe。退出码 0=PASS、1=FAIL。
 * 本文件刻意不含反斜杠转义（行尾续行符与 "换行" 字面量一律由 puts 承担）——
 * 沿本版 D-1 勘误教训：非 raw 写盘管线会把反斜杠序列求值成裸控制符。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "et_ringbuf.h"
#include "et_kv.h"
#include "port.h"
#include "port_host.h"

static int g_fail;

static void ex_check(bool ok, const char *what, int line)
{
    if (!ok) {
        printf("  FAIL: %s (line %d)", what, line);
        puts("");
        g_fail++;
    }
}

#define EX_CHECK(cond)  ex_check((cond) ? true : false, #cond, __LINE__)

#define RB_CAP      16u                 /* ringbuf 存储字节数（调用方提供） */
#define KV_SEC_A    2u                  /* 参数区内扇区号（host 虚拟 flash） */
#define KV_SEC_B    3u

static uint8_t            g_rb_mem[RB_CAP];
static et_ringbuf_t       g_rb;
static const et_kv_layout_t g_lay = { KV_SEC_A, KV_SEC_B };
static et_kv_t            g_kv;

int main(void)
{
    uint8_t       out[RB_CAP];
    uint8_t       val[11];
    uint8_t       back[16];
    uint16_t      got;
    uint32_t      i;
    uint32_t      w_total;
    et_kv_stats_t st;

    port_host_flash_reset();
    puts("[ex_min] start");

    /* ---- 1) ringbuf：写两帧并回读比对 ---- */
    EX_CHECK(et_ringbuf_init(&g_rb, g_rb_mem, RB_CAP));
    for (i = 0u; i < 4u; i++) {
        val[i] = (uint8_t) (0x10u + i);
    }
    for (i = 0u; i < 3u; i++) {
        val[4u + i] = (uint8_t) (0xE0u + i);
    }
    w_total = et_ringbuf_write(&g_rb, val, 4u);
    w_total += et_ringbuf_write(&g_rb, &val[4u], 3u);
    EX_CHECK(w_total == 7u);
    got = et_ringbuf_read(&g_rb, out, 7u);
    EX_CHECK(got == 7u);
    EX_CHECK(memcmp(out, val, 7u) == 0);
    printf("  ringbuf: cap=%u wrote=%u read=%u bytes match",
           (unsigned) RB_CAP, (unsigned) w_total, (unsigned) got);
    puts("");

    /* ---- 2) ringbuf 边界：满态短写 + 空态读 0（无阻塞、无动态内存） ---- */
    (void) et_ringbuf_read(&g_rb, out, 7u);                 /* 排空 */
    EX_CHECK(et_ringbuf_write(&g_rb, val, 15u) == 15u);     /* 留 1 字节 */
    got = et_ringbuf_write(&g_rb, val, 9u);
    EX_CHECK(got == 1u);
    i = et_ringbuf_read(&g_rb, out, 0u);
    printf("  ringbuf: full-path short write %u of 15 (free=1), empty read %u",
           (unsigned) got, (unsigned) i);
    puts("");
    (void) et_ringbuf_read(&g_rb, out, 16u);                /* 再次排空 */
    EX_CHECK(et_ringbuf_read(&g_rb, out, 4u) == 0u);

    /* ---- 3) kv：格式化后写入两个变长 key ---- */
    EX_CHECK(et_kv_init(&g_kv, &g_lay));
    EX_CHECK(et_kv_format(&g_kv, &g_lay));
    EX_CHECK(et_kv_init(&g_kv, &g_lay));
    val[0] = 0xA0u;
    val[1] = 0xA1u;
    val[2] = 0xA2u;
    val[3] = 0xA3u;
    EX_CHECK(et_kv_set(&g_kv, 1u, val, 4u));
    for (i = 0u; i < 11u; i++) {
        val[i] = (uint8_t) (0xB0u + i);
    }
    EX_CHECK(et_kv_set(&g_kv, 2u, val, 11u));
    et_kv_stats(&g_kv, &st);
    printf("  kv: format+init ok, keys=%u", (unsigned) st.key_count);
    puts("");
    EX_CHECK(st.key_count == 2u);

    memset(back, 0, sizeof(back));
    got = 0u;
    EX_CHECK(et_kv_get(&g_kv, 1u, back, 4u, &got));
    EX_CHECK(got == 4u);
    EX_CHECK(back[0] == 0xA0u);
    EX_CHECK(back[3] == 0xA3u);
    EX_CHECK(et_kv_get(&g_kv, 2u, back, sizeof(back), &got));
    EX_CHECK(got == 11u);
    printf("  kv: read-back match (k=1 len=4, k=2 len=%u)", (unsigned) got);
    puts("");

    /* ---- 4) 模拟重启：仅 init（不 format），参数须仍在 ---- */
    EX_CHECK(et_kv_init(&g_kv, &g_lay));
    got = 0u;
    memset(back, 0, sizeof(back));
    EX_CHECK(et_kv_get(&g_kv, 1u, back, sizeof(back), &got));
    EX_CHECK(got == 4u);
    printf("  kv: after re-init without format (simulated reboot) k=1 len=%u first byte=0x%02X",
           (unsigned) got, (unsigned) back[0]);
    puts("");

    /* ---- 5) 覆盖写：后写生效，重启后读到新值 ---- */
    val[0] = 0x5Au;
    val[1] = 0x5Bu;
    EX_CHECK(et_kv_set(&g_kv, 1u, val, 2u));
    EX_CHECK(et_kv_init(&g_kv, &g_lay));
    got = 0u;
    memset(back, 0, sizeof(back));
    EX_CHECK(et_kv_get(&g_kv, 1u, back, sizeof(back), &got));
    EX_CHECK(got == 2u);
    EX_CHECK(back[0] == 0x5Au);
    printf("  kv: overwrite persisted (k=1 len=%u, first byte=0x%02X)",
           (unsigned) got, (unsigned) back[0]);
    puts("");

    printf("[ex_min] %s", (g_fail == 0) ? "PASS" : "FAIL");
    puts("");
    return (g_fail == 0) ? 0 : 1;
}

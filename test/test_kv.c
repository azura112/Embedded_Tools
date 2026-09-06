/**
 * @file    test_kv.c
 * @brief   et_kv 单元测试 (host 模拟 flash + 字节级掉电注入)
 *
 * 掉电恢复矩阵(计划要求每类 ≥2 断点):
 *   记录半写: 头 1B 截断(脏尾→自动搬迁) / 头 6B 截断(CRC 拦截) / payload 中段截断
 *   页头半写: 搬迁页头 magic 后 / crc 前 (对页 INVALID 弃用, 源页完好)
 *   搬迁中途: 首记录中段 / 末记录中段 / COMMITTED 前最后一笔
 *   擦除中断: 半扇区擦除(对页残留旧数据) → 写违约 → commit 失败 → 源页完好
 */
#include "et_test.h"
#include "et_kv.h"
#include "et_crc.h"
#include "port.h"
#include "port_host.h"
#include <string.h>

#define SZ          ((uint32_t)PORT_FLASH_SECTOR_SIZE)
#define PAGE_A      0u
#define PAGE_B      1u

static et_kv_t        g_kv;
static et_kv_layout_t g_layout = { PAGE_A, PAGE_B };
static et_kv_stats_t  used_stats;

static void fill_buf(uint8_t *buf, uint16_t n, uint8_t seed)
{
    uint16_t i;

    for (i = 0u; i < n; i++) {
        buf[i] = (uint8_t)(seed + i);
    }
}

static bool buf_is(const uint8_t *buf, uint16_t n, uint8_t seed)
{
    uint16_t i;

    for (i = 0u; i < n; i++) {
        if (buf[i] != (uint8_t)(seed + i)) {
            return false;
        }
    }
    return true;
}

static void kv_fresh(void)
{
    port_host_flash_reset();
    ET_CHECK(et_kv_format(&g_kv, &g_layout));
}

/* 模拟重启: flash 保持, 仅重建运行时状态 */
static void kv_reopen(void)
{
    ET_CHECK(et_kv_init(&g_kv, &g_layout));
}

/* 白盒: 遍历指定页记录, 断言无 tombstone 且全部 CRC 有效(压实干净) */
static void assert_page_clean(uint32_t sector)
{
    const uint8_t *mem = port_host_flash_mem(sector * SZ);
    uint32_t       off = 16u;

    ET_CHECK(mem != NULL);
    while ((off + 8u) <= SZ) {
        uint16_t enc_key = (uint16_t)(mem[off] | ((uint16_t)mem[off + 1u] << 8));
        uint16_t len     = (uint16_t)(mem[off + 2u] | ((uint16_t)mem[off + 3u] << 8));
        uint32_t vcrc;

        if ((enc_key == 0xFFFFu) && (len == 0xFFFFu) && (mem[off + 4u] == 0xFFu)) {
            break;                                      /* 未写区 */
        }
        vcrc = (uint32_t)mem[off + 4u] | ((uint32_t)mem[off + 5u] << 8) |
               ((uint32_t)mem[off + 6u] << 16) | ((uint32_t)mem[off + 7u] << 24);

        ET_CHECK((enc_key & 0x8000u) == 0u);            /* 无 tombstone */
        ET_CHECK_U32_EQ((uint32_t)et_crc32(&mem[off + 8u], len), vcrc);
        off += 8u + (((uint32_t)len + 7u) & ~7u);       /* 槽 8B 对齐 (G4 约束) */
    }
}

/* ===================== 基本功能 ===================== */

static void kv_format_init_empty(void)
{
    et_kv_stats_t st;

    kv_fresh();
    et_kv_stats(&g_kv, &st);
    ET_CHECK_U32_EQ(1u,  st.seq);
    ET_CHECK_U32_EQ(16u, st.used_bytes);
    ET_CHECK_U32_EQ(SZ - 16u, st.free_bytes);
    ET_CHECK_U32_EQ(0u,  st.record_count);
    ET_CHECK_U32_EQ(0u,  st.key_count);
    ET_CHECK(!et_kv_get(&g_kv, 1u, NULL, 0u, NULL));
}

static void kv_first_boot_autoboot(void)
{
    uint8_t v = 0xABu;
    uint8_t out = 0u;

    port_host_flash_reset();                            /* 全 FF 新芯片 */
    kv_reopen();                                        /* init 自动初始化 */
    ET_CHECK(et_kv_set(&g_kv, 1u, &v, 1u));
    ET_CHECK(et_kv_get(&g_kv, 1u, &out, 1u, NULL));
    ET_CHECK_U32_EQ(0xABu, out);
}

static void kv_set_get_roundtrip(void)
{
    static uint8_t v[128], out[128];

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, NULL, 0u));           /* 空值记录 */
    ET_CHECK(et_kv_get(&g_kv, 1u, out, 0u, NULL));      /* len=0: buf 可 NULL */

    fill_buf(v, 1u, 10u);
    ET_CHECK(et_kv_set(&g_kv, 2u, v, 1u));              /* 尾 3B 填充路径 */
    fill_buf(v, 5u, 20u);
    ET_CHECK(et_kv_set(&g_kv, 3u, v, 5u));              /* 尾 1B 填充路径 */
    fill_buf(v, 100u, 30u);
    ET_CHECK(et_kv_set(&g_kv, 4u, v, 100u));            /* 4B 整除路径 */
    fill_buf(v, 128u, 40u);
    ET_CHECK(et_kv_set(&g_kv, (uint16_t)ET_KV_KEY_MAX, v, 128u));   /* key 上界 */

    ET_CHECK(et_kv_get(&g_kv, 2u, out, sizeof(out), NULL));
    ET_CHECK(buf_is(out, 1u, 10u));
    ET_CHECK(et_kv_get(&g_kv, 3u, out, sizeof(out), NULL));
    ET_CHECK(buf_is(out, 5u, 20u));
    ET_CHECK(et_kv_get(&g_kv, 4u, out, sizeof(out), NULL));
    ET_CHECK(buf_is(out, 100u, 30u));
    ET_CHECK(et_kv_get(&g_kv, ET_KV_KEY_MAX, out, sizeof(out), NULL));
    ET_CHECK(buf_is(out, 128u, 40u));
}

static void kv_set_validation(void)
{
    static uint8_t big[ET_KV_VAL_MAX + 1u];     /* 供拒绝路径引用, 不被读取 */
    uint8_t  v = 1u;
    uint32_t before = port_host_flash_written();

    kv_fresh();
    before = port_host_flash_written();
    ET_CHECK(!et_kv_set(&g_kv, 0u, &v, 1u));            /* key 0 */
    ET_CHECK(!et_kv_set(&g_kv, 0x7FFFu, &v, 1u));       /* 保留 key(哨兵冲突) */
    ET_CHECK(!et_kv_set(&g_kv, (uint16_t)(ET_KV_KEY_MAX + 1u), &v, 1u));
    ET_CHECK(!et_kv_set(&g_kv, 1u, NULL, 5u));          /* NULL 值 */
    ET_CHECK(!et_kv_set(&g_kv, 1u, big, (uint16_t)(ET_KV_VAL_MAX + 1u)));
    ET_CHECK_U32_EQ(before, port_host_flash_written()); /* 全部拒绝, 零写入 */
}

static void kv_update_takes_latest(void)
{
    uint8_t       out[8];
    et_kv_stats_t st;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 7u, "aaaa", 4u));
    ET_CHECK(et_kv_set(&g_kv, 7u, "bbbbbb", 6u));       /* 同 key 追加新版本 */
    ET_CHECK(et_kv_get(&g_kv, 7u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "bbbbbb", 6) == 0);
    ET_CHECK_U32_EQ(6u, et_kv_size(&g_kv, 7u));
    et_kv_stats(&g_kv, &st);
    ET_CHECK_U32_EQ(2u, st.record_count);               /* 两个版本都在页上 */
    ET_CHECK_U32_EQ(1u, st.key_count);
}

static void kv_delete_tombstone(void)
{
    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 5u, "xyz", 3u));
    ET_CHECK(et_kv_del(&g_kv, 5u));
    ET_CHECK(!et_kv_get(&g_kv, 5u, NULL, 0u, NULL));
    ET_CHECK_U32_EQ(0u, et_kv_size(&g_kv, 5u));
}

static void kv_del_nonexistent_no_write(void)
{
    uint32_t before;

    kv_fresh();
    before = port_host_flash_written();
    ET_CHECK(!et_kv_del(&g_kv, 9u));                    /* 不存在 */
    ET_CHECK_U32_EQ(before, port_host_flash_written()); /* 零写入 */
    ET_CHECK(et_kv_set(&g_kv, 9u, "q", 1u));
    ET_CHECK(et_kv_del(&g_kv, 9u));
    before = port_host_flash_written();
    ET_CHECK(!et_kv_del(&g_kv, 9u));                    /* 已删: 零写入 */
    ET_CHECK_U32_EQ(before, port_host_flash_written());
}

static void kv_resurrection_after_delete(void)
{
    uint8_t out[4];

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 3u, "aaaa", 4u));
    ET_CHECK(et_kv_del(&g_kv, 3u));
    ET_CHECK(et_kv_set(&g_kv, 3u, "bbbb", 4u));         /* 删后重写 */
    ET_CHECK(et_kv_get(&g_kv, 3u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "bbbb", 4) == 0);
}

static void kv_crc_bad_record_skips(void)
{
    static uint8_t v[32], out[32];
    uint16_t len = 0u;
    uint8_t *mem;

    kv_fresh();
    fill_buf(v, 32u, 1u);
    ET_CHECK(et_kv_set(&g_kv, 10u, v, 32u));            /* 记录1: off16, 8+32 */
    fill_buf(v, 16u, 2u);
    ET_CHECK(et_kv_set(&g_kv, 10u, v, 16u));            /* 记录2: off56, 8+16 */
    ET_CHECK(et_kv_set(&g_kv, 11u, "k11", 3u));         /* 记录3: off80 */

    mem = port_host_flash_mem(0u);
    ET_CHECK(mem != NULL);
    mem[16u + 8u + 32u + 8u] ^= 0xFFu;                  /* 记录2 payload 首字节翻转 */

    ET_CHECK(et_kv_get(&g_kv, 10u, out, sizeof(out), &len));    /* 回退旧版本 */
    ET_CHECK_U32_EQ(32u, len);
    ET_CHECK(buf_is(out, 32u, 1u));
    ET_CHECK_U32_EQ(3u, et_kv_size(&g_kv, 11u));        /* 其他 key 不受影响 */
    ET_CHECK(et_kv_get(&g_kv, 11u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "k11", 3) == 0);
}

static void kv_bad_page_abandoned(void)
{
    uint8_t *mem;
    uint8_t  out[4];

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "v1", 2u));           /* A 活跃 */
    ET_CHECK(et_kv_commit(&g_kv));                      /* 搬去 B, B 活跃 */
    ET_CHECK(et_kv_set(&g_kv, 2u, "v2", 2u));           /* 写 B */

    mem = port_host_flash_mem(PAGE_B * SZ);
    ET_CHECK(mem != NULL);
    mem[0] = 0x00u;                                     /* 破坏 B 页 magic */

    kv_reopen();
    ET_CHECK_U32_EQ(1u, g_kv.act_seq);                  /* 回退到 A(seq=1) */
    ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "v1", 2) == 0);
    ET_CHECK_U32_EQ(0u, et_kv_size(&g_kv, 2u));         /* B 整页弃用 */
}

static void kv_seq_arbitration(void)
{
    uint8_t out[4];
    uint8_t *mem;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "v1", 2u));           /* A(seq=1) */
    ET_CHECK(et_kv_commit(&g_kv));                      /* B(seq=2): v1 */
    ET_CHECK(et_kv_set(&g_kv, 2u, "v2", 2u));           /* B: v1 + v2 */

    /* 互换两页 seq 字段: A=2, B=1 (同步重算页头 CRC) */
    mem = port_host_flash_mem(0u);
    ET_CHECK(mem != NULL);
    {
        uint8_t  t[4];
        uint32_t crc;

        memcpy(t, mem + 4u, 4u);
        memcpy(mem + 4u, mem + SZ + 4u, 4u);
        memcpy(mem + SZ + 4u, t, 4u);

        crc = et_crc32(mem, 8u);            /* 页头 CRC 覆盖 magic|seq */
        mem[12u] = (uint8_t)(crc & 0xFFu);
        mem[13u] = (uint8_t)((crc >> 8) & 0xFFu);
        mem[14u] = (uint8_t)((crc >> 16) & 0xFFu);
        mem[15u] = (uint8_t)((crc >> 24) & 0xFFu);
        crc = et_crc32(mem + SZ, 8u);
        mem[SZ + 12u] = (uint8_t)(crc & 0xFFu);
        mem[SZ + 13u] = (uint8_t)((crc >> 8) & 0xFFu);
        mem[SZ + 14u] = (uint8_t)((crc >> 16) & 0xFFu);
        mem[SZ + 15u] = (uint8_t)((crc >> 24) & 0xFFu);
    }

    kv_reopen();
    ET_CHECK_U32_EQ(2u, g_kv.act_seq);                  /* seq 仲裁选 A */
    ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "v1", 2) == 0);
    ET_CHECK_U32_EQ(0u, et_kv_size(&g_kv, 2u));         /* B 的数据不可见 */
}

static void kv_compact_dedup(void)
{
    et_kv_stats_t st_before, st_after;
    uint8_t out[16];

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "old-old-old", 11u));
    ET_CHECK(et_kv_set(&g_kv, 2u, "temp", 4u));
    ET_CHECK(et_kv_set(&g_kv, 1u, "new", 3u));          /* k1 两个版本 */
    ET_CHECK(et_kv_del(&g_kv, 2u));                     /* k2 死 */
    ET_CHECK(et_kv_set(&g_kv, 3u, "k3", 2u));
    et_kv_stats(&g_kv, &st_before);

    ET_CHECK(et_kv_commit(&g_kv));                      /* 压实去 B 页 */
    et_kv_stats(&g_kv, &st_after);
    ET_CHECK(st_after.used_bytes < st_before.used_bytes);       /* 死空间释放 */
    ET_CHECK_U32_EQ(2u, st_after.record_count);                 /* k1new + k3 */
    ET_CHECK_U32_EQ(2u, st_after.key_count);
    assert_page_clean(PAGE_B);                          /* 白盒: 无死记录/无坏 CRC */

    ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "new", 3) == 0);
    ET_CHECK(et_kv_get(&g_kv, 3u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "k3", 2) == 0);
    ET_CHECK_U32_EQ(0u, et_kv_size(&g_kv, 2u));         /* tombstone 不搬迁 */
}

static void kv_auto_compact_on_full(void)
{
    static uint8_t v[128], out[128];
    et_kv_stats_t st;
    uint32_t i;

    kv_fresh();
    fill_buf(v, 128u, 0u);
    for (i = 0u; i < 60u; i++) {                        /* 60x136B 必然多次自动搬迁 */
        v[0] = (uint8_t)i;
        ET_CHECK(et_kv_set(&g_kv, 1u, v, 128u));
        ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL)); /* 全程可读 */
        ET_CHECK_U32_EQ((uint32_t)(uint8_t)i, out[0]);
    }
    ET_CHECK(et_kv_commit(&g_kv));
    et_kv_stats(&g_kv, &st);
    ET_CHECK(st.seq > 1u);                              /* 发生过页切换 */
    ET_CHECK_U32_EQ(1u, st.record_count);               /* 压实后仅最新版本 */
    ET_CHECK_U32_EQ(1u, st.key_count);
    assert_page_clean(g_kv.act_sector);
}

static void kv_overlong_rejected(void)
{
    static uint8_t v[ET_KV_VAL_MAX + 1u];

    kv_fresh();
    fill_buf(v, ET_KV_VAL_MAX, 7u);
    ET_CHECK(et_kv_set(&g_kv, 1u, v, ET_KV_VAL_MAX));   /* 恰好达单记录上限 */
    ET_CHECK_U32_EQ(ET_KV_VAL_MAX, et_kv_size(&g_kv, 1u));
    kv_reopen();                                        /* 重启仍在 */
    ET_CHECK_U32_EQ(ET_KV_VAL_MAX, et_kv_size(&g_kv, 1u));

    ET_CHECK(!et_kv_set(&g_kv, 2u, v, (uint16_t)(ET_KV_VAL_MAX + 1u)));  /* 超长拒绝 */
}

static void kv_get_buffer_short(void)
{
    static uint8_t v[100], out[100];
    uint16_t len = 0u;
    uint16_t i;

    kv_fresh();
    fill_buf(v, 100u, 5u);
    ET_CHECK(et_kv_set(&g_kv, 1u, v, 100u));

    for (i = 0u; i < sizeof(out); i++) {
        out[i] = 0xAAu;
    }
    ET_CHECK(!et_kv_get(&g_kv, 1u, out, 50u, &len));    /* cap 不足 */
    ET_CHECK_U32_EQ(100u, len);                         /* 给出真实长度 */
    for (i = 0u; i < sizeof(out); i++) {
        ET_CHECK_U32_EQ(0xAAu, out[i]);                 /* buf 未被触碰 */
    }
    ET_CHECK(et_kv_get(&g_kv, 1u, out, 100u, &len));    /* 恰好足够 */
    ET_CHECK_U32_EQ(100u, len);
    ET_CHECK(buf_is(out, 100u, 5u));
}

static void kv_size_query(void)
{
    kv_fresh();
    ET_CHECK_U32_EQ(0u, et_kv_size(&g_kv, 1u));         /* 不存在 */
    ET_CHECK(et_kv_set(&g_kv, 1u, "abcd", 4u));
    ET_CHECK_U32_EQ(4u, et_kv_size(&g_kv, 1u));
    ET_CHECK(et_kv_del(&g_kv, 1u));
    ET_CHECK_U32_EQ(0u, et_kv_size(&g_kv, 1u));         /* 已删 */
}

static void kv_multi_instance(void)
{
    static et_kv_t  kv2;
    et_kv_layout_t  lay2 = { 4u, 5u };
    uint8_t         out[8];

    kv_fresh();
    ET_CHECK(et_kv_init(&kv2, &lay2));                  /* 扇区 4/5 全 FF: 自动初始化 */
    ET_CHECK(et_kv_set(&g_kv, 1u, "inst0", 5u));
    ET_CHECK(et_kv_set(&kv2, 1u, "inst1", 5u));         /* 同 key 不同实例 */
    ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "inst0", 5) == 0);
    ET_CHECK(et_kv_get(&kv2, 1u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "inst1", 5) == 0);

    /* 非法布局 */
    {
        et_kv_layout_t bad1 = { 4u, 4u };
        et_kv_layout_t bad2 = { 4u, 16u };

        ET_CHECK(!et_kv_init(&kv2, &bad1));             /* 同扇区 */
        ET_CHECK(!et_kv_init(&kv2, &bad2));             /* 越界 */
    }
}

static void kv_reopen_persistence(void)
{
    static uint8_t v[64], out[64];

    kv_fresh();
    fill_buf(v, 64u, 9u);
    ET_CHECK(et_kv_set(&g_kv, 1u, v, 64u));
    ET_CHECK(et_kv_set(&g_kv, 2u, "p2", 2u));
    kv_reopen();                                        /* 无注入重启 */
    ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
    ET_CHECK(buf_is(out, 64u, 9u));
    ET_CHECK(et_kv_get(&g_kv, 2u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "p2", 2) == 0);
    ET_CHECK_U32_EQ(2u, g_kv.record_cnt);
}

static void kv_init_fails_both_bad(void)
{
    uint8_t *mem;

    port_host_flash_reset();
    mem = port_host_flash_mem(0u);
    ET_CHECK(mem != NULL);
    mem[0] = 0x00u;                                     /* A 页 magic 坏 */
    mem = port_host_flash_mem(PAGE_B * SZ);
    ET_CHECK(mem != NULL);
    mem[0] = 0x00u;                                     /* B 页 magic 坏 */
    ET_CHECK(!et_kv_init(&g_kv, &g_layout));            /* 无有效页: init 失败 */
    ET_CHECK(et_kv_format(&g_kv, &g_layout));           /* format 恢复 */
    ET_CHECK(et_kv_set(&g_kv, 1u, "ok", 2u));
    ET_CHECK_U32_EQ(2u, et_kv_size(&g_kv, 1u));
}

static void kv_stats_fields(void)
{
    et_kv_stats_t st;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "1234", 4u));         /* 16 + slot(4)=16 = 32 */
    ET_CHECK(et_kv_set(&g_kv, 2u, NULL, 0u));           /* 32 + slot(0)=8 = 40 */
    et_kv_stats(&g_kv, &st);
    ET_CHECK_U32_EQ(1u,  st.seq);
    ET_CHECK_U32_EQ(1u,  st.erase_cnt_a);
    ET_CHECK_U32_EQ(1u,  st.erase_cnt_b);
    ET_CHECK_U32_EQ(40u, st.used_bytes);
    ET_CHECK_U32_EQ(SZ - 40u, st.free_bytes);
    ET_CHECK_U32_EQ(2u,  st.record_count);
    ET_CHECK_U32_EQ(2u,  st.key_count);

    ET_CHECK(et_kv_commit(&g_kv));                      /* 擦 B */
    et_kv_stats(&g_kv, &st);
    ET_CHECK_U32_EQ(2u, st.seq);
    ET_CHECK_U32_EQ(1u, st.erase_cnt_a);
    ET_CHECK_U32_EQ(2u, st.erase_cnt_b);
}

static void kv_erase_counters(void)
{
    et_kv_stats_t st;

    port_host_flash_reset();
    ET_CHECK(et_kv_init(&g_kv, &g_layout));             /* 首次上电自动初始化 */
    et_kv_stats(&g_kv, &st);
    ET_CHECK_U32_EQ(0u, st.erase_cnt_a);
    ET_CHECK_U32_EQ(0u, st.erase_cnt_b);

    ET_CHECK(et_kv_set(&g_kv, 1u, "a", 1u));
    ET_CHECK(et_kv_commit(&g_kv));                      /* 擦 B */
    et_kv_stats(&g_kv, &st);
    ET_CHECK_U32_EQ(0u, st.erase_cnt_a);
    ET_CHECK_U32_EQ(1u, st.erase_cnt_b);
    ET_CHECK(et_kv_commit(&g_kv));                      /* 擦 A */
    et_kv_stats(&g_kv, &st);
    ET_CHECK_U32_EQ(1u, st.erase_cnt_a);
    ET_CHECK_U32_EQ(1u, st.erase_cnt_b);
    ET_CHECK_U32_EQ(3u, st.seq);
}

/* ===================== 掉电恢复矩阵 ===================== */

static void pcut_record_header_1byte(void)
{
    uint8_t  out[8];
    uint32_t w;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "old", 3u));          /* k1 旧版本 */
    ET_CHECK(et_kv_set(&g_kv, 2u, "k2", 2u));

    w = port_host_flash_written();
    port_host_flash_fail_after(w + 1u);                 /* 记录头只写 1B → 脏尾 */
    ET_CHECK(!et_kv_set(&g_kv, 1u, "new", 3u));         /* 写入截断 */

    kv_reopen();                                        /* 脏尾 → 自动搬迁修复 */
    ET_CHECK_U32_EQ(3u, et_kv_size(&g_kv, 1u));         /* 旧版本幸存 */
    ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
    ET_CHECK(memcmp(out, "old", 3) == 0);
    ET_CHECK_U32_EQ(2u, et_kv_size(&g_kv, 2u));         /* 其他 key 完好 */

    ET_CHECK(et_kv_set(&g_kv, 1u, "new2", 4u));         /* 修复后可正常写 */
    ET_CHECK_U32_EQ(4u, et_kv_size(&g_kv, 1u));
}

static void pcut_record_header_6bytes(void)
{
    uint32_t w;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "old", 3u));

    w = port_host_flash_written();
    port_host_flash_fail_after(w + 6u);                 /* 头 6B: key/len 全, vcrc 半 */
    ET_CHECK(!et_kv_set(&g_kv, 1u, "new", 3u));

    kv_reopen();                                        /* 可解析但 CRC 失败: 跳过, 无搬迁 */
    ET_CHECK_U32_EQ(1u, g_kv.act_seq);                  /* 未触发自动搬迁 */
    ET_CHECK_U32_EQ(3u, et_kv_size(&g_kv, 1u));         /* 旧版本回退 */

    ET_CHECK(et_kv_set(&g_kv, 3u, "x", 1u));            /* 新记录落在未写区, 正常 */
    ET_CHECK_U32_EQ(1u, et_kv_size(&g_kv, 3u));
}

static void pcut_record_payload_mid(void)
{
    static uint8_t v[100], out[100];
    uint32_t w;

    kv_fresh();
    fill_buf(v, 100u, 1u);
    ET_CHECK(et_kv_set(&g_kv, 1u, v, 100u));            /* k1 旧版本 100B */
    fill_buf(v, 100u, 2u);

    w = port_host_flash_written();
    port_host_flash_fail_after(w + 8u + 40u);           /* 记录头全 + payload 40B 截断 */
    ET_CHECK(!et_kv_set(&g_kv, 1u, v, 100u));

    kv_reopen();
    ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));     /* 回退旧版本 */
    ET_CHECK(buf_is(out, 100u, 1u));

    ET_CHECK(et_kv_set(&g_kv, 2u, "ok", 2u));           /* 后续写入正常 */
    ET_CHECK_U32_EQ(2u, et_kv_size(&g_kv, 2u));
}

static void pcut_compact_header(void)
{
    uint8_t  out[8];
    uint32_t w, i;

    for (i = 0u; i < 2u; i++) {                         /* 断点: DW0 内 / DW0 后首记录中 */
        kv_fresh();
        ET_CHECK(et_kv_set(&g_kv, 1u, "safe", 4u));
        ET_CHECK(et_kv_set(&g_kv, 2u, "k2", 2u));
        w = port_host_flash_written();
        port_host_flash_fail_after(w + ((i == 0u) ? 4u : 12u));
        ET_CHECK(!et_kv_commit(&g_kv));                 /* 对页页头半写 */

        kv_reopen();                                    /* 对页 INVALID 弃用 */
        ET_CHECK_U32_EQ(1u, g_kv.act_seq);              /* 源页仍活跃 */
        ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
        ET_CHECK(memcmp(out, "safe", 4) == 0);
        ET_CHECK_U32_EQ(2u, et_kv_size(&g_kv, 2u));
    }
}

static void pcut_compact_mid_migrate(void)
{
    static uint8_t v[32];
    uint8_t  out[32];
    uint32_t w, used, i;

    fill_buf(v, 32u, 8u);
    for (i = 0u; i < 2u; i++) {                         /* 断点: 首记录中段 / 末记录中段 */
        kv_fresh();
        ET_CHECK(et_kv_set(&g_kv, 1u, v, 32u));         /* slot(32)=40 */
        ET_CHECK(et_kv_set(&g_kv, 2u, "k2", 2u));       /* slot(2)=16 */
        ET_CHECK(et_kv_set(&g_kv, 3u, "k3", 2u));       /* slot(2)=16; used=88 */
        et_kv_stats(&g_kv, &used_stats);
        used = used_stats.used_bytes;

        w = port_host_flash_written();
        /* 页头 DW0=8B + 搬迁槽; 断点 16=8+8(首记录中) / used-16=末记录中 */
        port_host_flash_fail_after(w + ((i == 0u) ? 16u : (used - 16u)));
        ET_CHECK(!et_kv_commit(&g_kv));                 /* 搬迁中断, COMMITTED 未写 */

        kv_reopen();                                    /* 对页 MOVING 弃用 */
        ET_CHECK_U32_EQ(1u, g_kv.act_seq);              /* 源页数据完好 */
        ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
        ET_CHECK(buf_is(out, 32u, 8u));
        ET_CHECK_U32_EQ(2u, et_kv_size(&g_kv, 2u));
        ET_CHECK_U32_EQ(2u, et_kv_size(&g_kv, 3u));
    }
}

static void pcut_compact_pre_commit(void)
{
    static uint8_t v[32];
    uint8_t  out[32];
    uint32_t w;

    kv_fresh();
    fill_buf(v, 32u, 3u);
    ET_CHECK(et_kv_set(&g_kv, 1u, v, 32u));             /* 存活: 8+32=40 */
    ET_CHECK(et_kv_set(&g_kv, 2u, "dead", 4u));
    ET_CHECK(et_kv_del(&g_kv, 2u));                     /* tombstone: 8(不搬) */

    w = port_host_flash_written();
    port_host_flash_fail_after(w + 8u + 40u);           /* DW0+搬迁槽写完, DW1 提交前 */
    ET_CHECK(!et_kv_commit(&g_kv));                     /* state 写不进 */

    kv_reopen();
    ET_CHECK_U32_EQ(1u, g_kv.act_seq);                  /* 对页 MOVING 弃用 */
    ET_CHECK(et_kv_get(&g_kv, 1u, out, sizeof(out), NULL));
    ET_CHECK(buf_is(out, 32u, 3u));
    ET_CHECK_U32_EQ(0u, et_kv_size(&g_kv, 2u));         /* tombstone 语义保持 */
}

static void pcut_erase_half_sector(void)
{
    static uint8_t v[16], out[16];
    uint8_t *mem;
    uint32_t i;
    uint32_t half;      /* 页半区(几何相对): 半擦只擦前半 */
    uint32_t n1;        /* A 页记录数: 记录区尾越过半区 */

    kv_fresh();
    fill_buf(v, 16u, 6u);
    half = SZ / 2u;
    n1 = ((half - 16u) / 24u) + 2u;                     /* 槽 24B(头8+值16) */
    for (i = 0u; i < n1; i++) {                         /* A: n1 条存活, 尾越过 half */
        v[0] = (uint8_t)i;
        ET_CHECK(et_kv_set(&g_kv, (uint16_t)(i + 1u), v, 16u));
    }
    ET_CHECK(et_kv_commit(&g_kv));                      /* → B 活跃, A 残留旧数据 */

    /* 残留区 [half, 16+n1*24) 篡改为全 0: 半擦后这部分未被擦除,
       搬迁写入的任何非 0 字节都触发位写违约(0 不可写成 1) */
    mem = port_host_flash_mem(PAGE_A * SZ);
    ET_CHECK(mem != NULL);
    memset(mem + half, 0x00, (16u + n1 * 24u) - half);

    for (i = n1; i < (n1 + 10u); i++) {                 /* B: +10 条 */
        v[0] = (uint8_t)i;
        ET_CHECK(et_kv_set(&g_kv, (uint16_t)(i + 1u), v, 16u));
    }

    port_host_flash_erase_fail_once();                  /* 擦 A 只擦前半 → 残留 0x00 区 */
    ET_CHECK(!et_kv_commit(&g_kv));                     /* 搬迁跨半区写违约 → 失败 */

    kv_reopen();                                        /* A 页头 MOVING/半擦 → 弃用 */
    ET_CHECK_U32_EQ(2u, g_kv.act_seq);                  /* B(seq=2) 数据完好 */
    for (i = 0u; i < (n1 + 10u); i++) {
        ET_CHECK(et_kv_get(&g_kv, (uint16_t)(i + 1u), out, sizeof(out), NULL));
        ET_CHECK_U32_EQ((uint32_t)(uint8_t)i, out[0]); /* v[0] = i */
        ET_CHECK(buf_is(out + 1u, 15u, 7u));            /* 其余 = 7..21 */
    }
}

/* ===================== 枚举迭代 (v1.4) ===================== */

static void iter_enumerate_all(void)
{
    uint8_t      v[8];
    et_kv_iter_t it;
    uint16_t     key, len;
    uint8_t      seen[4] = { 0u, 0u, 0u, 0u };
    int          total   = 0;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "aa", 2u));
    ET_CHECK(et_kv_set(&g_kv, 2u, "bbbb", 4u));
    ET_CHECK(et_kv_set(&g_kv, 3u, "cccccc", 6u));

    ET_CHECK(et_kv_iter_init(&g_kv, &it));
    while (et_kv_iter_next(&g_kv, &it, &key, &len)) {
        ET_CHECK(key <= 3u);
        ET_CHECK(seen[key] == 0u);                  /* 每 key 恰一次 */
        seen[key] = 1u;
        total++;
        switch (key) {
        case 1u: ET_CHECK_U32_EQ(2u, len); break;
        case 2u: ET_CHECK_U32_EQ(4u, len); break;
        case 3u: ET_CHECK_U32_EQ(6u, len); break;
        default: ET_FAIL("unexpected key");
        }
        /* 值与 et_kv_get 一致 */
        ET_CHECK(et_kv_get(&g_kv, key, v, sizeof(v), NULL));
    }
    ET_CHECK_U32_EQ(3, total);
    ET_CHECK(seen[1] && seen[2] && seen[3]);
}

static void iter_tombstone_skipped(void)
{
    et_kv_iter_t it;
    uint16_t     key, len;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "aa", 2u));
    ET_CHECK(et_kv_del(&g_kv, 1u));                 /* tombstone */
    ET_CHECK(et_kv_set(&g_kv, 2u, "bb", 2u));

    ET_CHECK(et_kv_iter_init(&g_kv, &it));
    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(2u, key);                       /* 1 已删不出现 */
    ET_CHECK(!et_kv_iter_next(&g_kv, &it, &key, &len));
}

static void iter_latest_only(void)
{
    et_kv_iter_t it;
    uint16_t     key, len;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 7u, "aaaa", 4u));
    ET_CHECK(et_kv_set(&g_kv, 7u, "bbbbbbbb", 8u)); /* 同 key 两个版本 */

    ET_CHECK(et_kv_iter_init(&g_kv, &it));
    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(7u, key);
    ET_CHECK_U32_EQ(8u, len);                       /* 只出最新版本 */
    ET_CHECK(!et_kv_iter_next(&g_kv, &it, &key, &len));
}

static void iter_empty_and_args(void)
{
    et_kv_iter_t it;
    et_kv_t      zero;
    uint16_t     key, len;

    kv_fresh();                                     /* 格式化后无记录 */
    ET_CHECK(et_kv_iter_init(&g_kv, &it));
    ET_CHECK(!et_kv_iter_next(&g_kv, &it, &key, &len)); /* 空库立即结束 */

    memset(&zero, 0, sizeof(zero));
    ET_CHECK(!et_kv_iter_init(&zero, &it));         /* 未初始化句柄 */
    ET_CHECK(!et_kv_iter_init(NULL, &it));
    ET_CHECK(!et_kv_iter_init(&g_kv, NULL));
    ET_CHECK(!et_kv_iter_next(&g_kv, &it, NULL, NULL)); /* key 必填 */
}

static void iter_mid_set_appends_visible(void)
{
    et_kv_iter_t it;
    uint16_t     key, len;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "a", 1u));
    ET_CHECK(et_kv_set(&g_kv, 2u, "b", 1u));

    ET_CHECK(et_kv_iter_init(&g_kv, &it));
    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(1u, key);
    ET_CHECK(et_kv_set(&g_kv, 3u, "c", 1u));        /* 迭代中追加(同活跃页) */

    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(2u, key);
    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(3u, key);                       /* 快照页上的追加对游标可见 */
    ET_CHECK(!et_kv_iter_next(&g_kv, &it, &key, &len));
}

static void iter_across_commit_snapshot(void)
{
    et_kv_iter_t it;
    uint16_t     key, len;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "a", 1u));
    ET_CHECK(et_kv_set(&g_kv, 1u, "aa", 2u));       /* 旧版本: 压实时丢弃 */
    ET_CHECK(et_kv_set(&g_kv, 2u, "b", 1u));
    ET_CHECK(et_kv_set(&g_kv, 3u, "c", 1u));

    ET_CHECK(et_kv_iter_init(&g_kv, &it));
    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(1u, key);

    ET_CHECK(et_kv_commit(&g_kv));                  /* 换页: 快照页转备用但仍可读 */

    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(2u, key);
    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(3u, key);
    ET_CHECK(!et_kv_iter_next(&g_kv, &it, &key, &len));
}

static void iter_crc_bad_skipped(void)
{
    const uint8_t *mem;
    et_kv_iter_t   it;
    uint16_t       key, len;

    kv_fresh();
    ET_CHECK(et_kv_set(&g_kv, 1u, "aa", 2u));
    ET_CHECK(et_kv_set(&g_kv, 2u, "bb", 2u));
    ET_CHECK(et_kv_set(&g_kv, 3u, "cc", 2u));

    /* 白盒: 打坏第 2 条记录 payload 首字节 → CRC 失效, 仅该条被跳过 */
    mem = port_host_flash_mem(PAGE_A * SZ + 16u + (8u + 4u) + 8u);
    ET_CHECK(mem != NULL);
    ((volatile uint8_t *)mem)[0] ^= 0xFFu;

    ET_CHECK(et_kv_iter_init(&g_kv, &it));
    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(1u, key);
    ET_CHECK(et_kv_iter_next(&g_kv, &it, &key, &len));
    ET_CHECK_U32_EQ(3u, key);                       /* 2 被跳过 */
    ET_CHECK(!et_kv_iter_next(&g_kv, &it, &key, &len));
}

const et_test_case_t *test_kv_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"kv.format_init_empty",        kv_format_init_empty},
        {"kv.first_boot_autoboot",      kv_first_boot_autoboot},
        {"kv.set_get_roundtrip",        kv_set_get_roundtrip},
        {"kv.set_validation",           kv_set_validation},
        {"kv.update_takes_latest",      kv_update_takes_latest},
        {"kv.delete_tombstone",         kv_delete_tombstone},
        {"kv.del_nonexistent_no_write", kv_del_nonexistent_no_write},
        {"kv.resurrection_after_delete", kv_resurrection_after_delete},
        {"kv.crc_bad_record_skips",     kv_crc_bad_record_skips},
        {"kv.bad_page_abandoned",       kv_bad_page_abandoned},
        {"kv.seq_arbitration",          kv_seq_arbitration},
        {"kv.compact_dedup",            kv_compact_dedup},
        {"kv.auto_compact_on_full",     kv_auto_compact_on_full},
        {"kv.overlong_rejected",        kv_overlong_rejected},
        {"kv.get_buffer_short",         kv_get_buffer_short},
        {"kv.size_query",               kv_size_query},
        {"kv.multi_instance",           kv_multi_instance},
        {"kv.reopen_persistence",       kv_reopen_persistence},
        {"kv.init_fails_both_bad",      kv_init_fails_both_bad},
        {"kv.stats_fields",             kv_stats_fields},
        {"kv.erase_counters",           kv_erase_counters},
        {"iter.enumerate_all",          iter_enumerate_all},
        {"iter.tombstone_skipped",      iter_tombstone_skipped},
        {"iter.latest_only",            iter_latest_only},
        {"iter.empty_and_args",         iter_empty_and_args},
        {"iter.mid_set_appends",        iter_mid_set_appends_visible},
        {"iter.across_commit",          iter_across_commit_snapshot},
        {"iter.crc_bad_skipped",        iter_crc_bad_skipped},
        {"pcut.record_header_1byte",    pcut_record_header_1byte},
        {"pcut.record_header_6bytes",   pcut_record_header_6bytes},
        {"pcut.record_payload_mid",     pcut_record_payload_mid},
        {"pcut.compact_header",         pcut_compact_header},
        {"pcut.compact_mid_migrate",    pcut_compact_mid_migrate},
        {"pcut.compact_pre_commit",     pcut_compact_pre_commit},
        {"pcut.erase_half_sector",      pcut_erase_half_sector},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

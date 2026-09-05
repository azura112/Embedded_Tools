/**
 * @file    test_bootctl.c
 * @brief   et_bootctl 单元测试 (host 模拟 flash + 白盒掉电矩阵)
 *
 * 掉电矩阵 (计划要求 stage/confirm/计数三类断点各 ≥2):
 *   stage   : val 落盘 inv 未写 / val 半字截断
 *   confirm : val 落盘 inv 损坏 / val 半字截断
 *   attempt : 第 2 条 ATT inv 损坏 / val 半字截断
 *   修复语义: 脏尾在 init/下次访问时重放修复, 丢最后一步, 前态完好
 */
#include <stddef.h>
#include <string.h>

#include "et_test.h"
#include "et_bootctl.h"
#include "et_crc.h"
#include "port.h"
#include "port_host.h"

#define SZ          ((uint32_t)PORT_FLASH_SECTOR_SIZE)
#define ST_SEC      2u                      /* 状态扇区 */
#define SLOT_A      0u
#define SLOT_B      1u

static et_bootctl_t      g_bc;
static et_bootctl_cfg_t  g_cfg = { ST_SEC, { SLOT_A, SLOT_B }, SZ, 2u };

static void bc_fresh(void)
{
    port_host_flash_reset();
    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));
}

/* 白盒: 构造镜像头 (写槽位首部); corrupt_* 注入损坏 */
static void write_image(uint32_t slot, uint32_t img_size, uint32_t img_ver,
                        int corrupt_img, int corrupt_hdr)
{
    uint8_t *mem = port_host_flash_mem(slot * SZ);
    uint8_t  hdr[32];
    uint32_t crc;
    uint32_t i;

    ET_CHECK(mem != NULL);
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 0x45u; hdr[1] = 0x54u; hdr[2] = 0x42u; hdr[3] = 0x49u; /* 'ETBI' LE */
    hdr[4] = (uint8_t)ET_BOOT_HDR_VER;
    hdr[6] = (uint8_t)(ET_BOOT_HDR_SIZE & 0xFFu);
    hdr[7] = (uint8_t)(ET_BOOT_HDR_SIZE >> 8);
    hdr[8]  = (uint8_t)(img_size);
    hdr[9]  = (uint8_t)(img_size >> 8);
    hdr[10] = (uint8_t)(img_size >> 16);
    hdr[11] = (uint8_t)(img_size >> 24);
    hdr[16] = (uint8_t)(img_ver);
    hdr[17] = (uint8_t)(img_ver >> 8);
    hdr[18] = (uint8_t)(img_ver >> 16);
    hdr[19] = (uint8_t)(img_ver >> 24);
    /* img_crc32 @12: 先填 0 算镜像 CRC, 再回填 */
    crc = ET_CRC32_INIT;
    for (i = 0u; i < img_size; i++) {
        uint8_t b = (uint8_t)(0xA0u + (uint8_t)i);
        crc = et_crc32_update(crc, &b, 1u);
    }
    crc ^= ET_CRC32_INIT;
    if (corrupt_img) {
        crc ^= 0xFFu;
    }
    hdr[12] = (uint8_t)(crc);
    hdr[13] = (uint8_t)(crc >> 8);
    hdr[14] = (uint8_t)(crc >> 16);
    hdr[15] = (uint8_t)(crc >> 24);
    crc = et_crc32(hdr, 28u);
    if (corrupt_hdr) {
        crc ^= 0xFFu;
    }
    hdr[28] = (uint8_t)(crc);
    hdr[29] = (uint8_t)(crc >> 8);
    hdr[30] = (uint8_t)(crc >> 16);
    hdr[31] = (uint8_t)(crc >> 24);
    memcpy(mem, hdr, sizeof(hdr));

    /* 镜像体 */
    for (i = 0u; i < img_size; i++) {
        mem[32u + i] = (uint8_t)(0xA0u + (uint8_t)i);
    }
}

/* 白盒: 直接读状态记录区第 idx 条 (0 起) 的 8B 引用 */
static uint8_t *rec_mem(uint32_t idx)
{
    uint8_t *mem = port_host_flash_mem(ST_SEC * SZ + 12u + idx * 8u);

    ET_CHECK(mem != NULL);
    return mem;
}

static uint32_t rec_count(void)
{
    uint32_t off = 12u, n = 0u;
    const uint8_t *mem = port_host_flash_mem(ST_SEC * SZ);

    while ((off + 8u) <= SZ) {
        const uint8_t *p = &mem[off];
        if ((p[0] == 0xFFu) && (p[1] == 0xFFu) && (p[2] == 0xFFu) && (p[3] == 0xFFu) &&
            (p[4] == 0xFFu) && (p[5] == 0xFFu) && (p[6] == 0xFFu) && (p[7] == 0xFFu)) {
            break;
        }
        n++;
        off += 8u;
    }
    return n;
}

/* ---- 用例 ---- */

static void init_validation(void)
{
    et_bootctl_cfg_t bad = g_cfg;

    ET_CHECK(!et_bootctl_init(NULL, &g_cfg));
    ET_CHECK(!et_bootctl_init(&g_bc, NULL));

    bad = g_cfg; bad.state_sector = PORT_FLASH_SECTOR_COUNT;
    ET_CHECK(!et_bootctl_init(&g_bc, &bad));            /* 状态扇区越界 */
    bad = g_cfg; bad.slot_sector[0] = PORT_FLASH_SECTOR_COUNT;
    ET_CHECK(!et_bootctl_init(&g_bc, &bad));            /* 槽扇区越界 */
    bad = g_cfg; bad.slot_sector[1] = bad.slot_sector[0];
    ET_CHECK(!et_bootctl_init(&g_bc, &bad));            /* 双槽同扇区 */
    bad = g_cfg; bad.slot_sector[0] = bad.state_sector;
    ET_CHECK(!et_bootctl_init(&g_bc, &bad));            /* 槽=状态扇区 */
    bad = g_cfg; bad.slot_size = 32u;
    ET_CHECK(!et_bootctl_init(&g_bc, &bad));            /* 槽容不下头+1B */
    bad = g_cfg; bad.max_attempts = 0u;
    ET_CHECK(!et_bootctl_init(&g_bc, &bad));            /* 阈值 0 */

    bc_fresh();                                         /* 合法 cfg */
    ET_CHECK(g_bc.inited);
}

static void init_fresh_selfheal(void)
{
    et_bootctl_state_t st;

    port_host_flash_reset();
    /* 全 FF 新扇区: init 自愈写状态头 */
    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK(st.state_ok);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.staged_slot);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.confirmed_slot);
    ET_CHECK_U32_EQ(0u, st.attempts);

    /* 幂等: 二次 init 不破坏 */
    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.staged_slot);
}

static void init_corrupt_selfheal(void)
{
    uint8_t *mem;
    et_bootctl_state_t st;

    bc_fresh();
    (void)et_bootctl_stage(&g_bc, SLOT_B);
    mem = port_host_flash_mem(ST_SEC * SZ);
    ET_CHECK(mem != NULL);
    ((volatile uint8_t *)mem)[0] ^= 0xFFu;              /* 打坏 magic */

    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));           /* 自愈重建 */
    et_bootctl_state(&g_bc, &st);
    ET_CHECK(st.state_ok);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.staged_slot);      /* 回初始态(安全默认) */
}

static void stage_and_state_query(void)
{
    et_bootctl_state_t st;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(1u, (uint32_t)st.staged_slot);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.confirmed_slot);
    ET_CHECK_U32_EQ(0u, st.attempts);
    ET_CHECK(!et_bootctl_should_rollback(&g_bc, SLOT_B));
}

static void stage_idempotent_same(void)
{
    uint32_t w1, w2;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    w1 = port_host_flash_written();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));          /* 同槽幂等 */
    w2 = port_host_flash_written();
    ET_CHECK_U32_EQ(w1, w2);                            /* 零写入 */
    ET_CHECK_U32_EQ(1u, rec_count());
}

static void stage_mutex_other_slot(void)
{
    et_bootctl_state_t st;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    ET_CHECK(!et_bootctl_stage(&g_bc, SLOT_A));         /* 双槽互斥 */
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(1u, (uint32_t)st.staged_slot);      /* 原 staged 不变 */
}

static void stage_after_confirm_rejected(void)
{
    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    ET_CHECK(et_bootctl_confirm(&g_bc, SLOT_B));
    ET_CHECK(!et_bootctl_stage(&g_bc, SLOT_B));         /* 已确认轮次拒绝 */
    ET_CHECK(!et_bootctl_stage(&g_bc, SLOT_A));
}

static void confirm_flow(void)
{
    et_bootctl_state_t st;

    bc_fresh();
    ET_CHECK(!et_bootctl_confirm(&g_bc, SLOT_B));       /* 未 staged */
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    ET_CHECK(!et_bootctl_confirm(&g_bc, SLOT_A));       /* 槽不符 */
    ET_CHECK(et_bootctl_confirm(&g_bc, SLOT_B));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(1u, (uint32_t)st.confirmed_slot);
    ET_CHECK(!et_bootctl_should_rollback(&g_bc, SLOT_B));
}

static void attempt_increments(void)
{
    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(1u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(2u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(3u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(0u, et_bootctl_boot_attempt(&g_bc, SLOT_A)); /* 非 staged 槽: 0 且不写 */
}

static void attempt_without_stage(void)
{
    uint32_t w1;

    bc_fresh();
    w1 = port_host_flash_written();
    ET_CHECK_U32_EQ(0u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(w1, port_host_flash_written());     /* 零写入 */
}

static void rollback_threshold(void)
{
    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    (void)et_bootctl_boot_attempt(&g_bc, SLOT_B);
    ET_CHECK(!et_bootctl_should_rollback(&g_bc, SLOT_B));   /* 1 < 2 */
    (void)et_bootctl_boot_attempt(&g_bc, SLOT_B);
    ET_CHECK(et_bootctl_should_rollback(&g_bc, SLOT_B));    /* 2 >= 2 */
}

static void no_rollback_when_confirmed(void)
{
    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    (void)et_bootctl_boot_attempt(&g_bc, SLOT_B);
    (void)et_bootctl_boot_attempt(&g_bc, SLOT_B);
    (void)et_bootctl_boot_attempt(&g_bc, SLOT_B);
    ET_CHECK(et_bootctl_should_rollback(&g_bc, SLOT_B));
    ET_CHECK(et_bootctl_confirm(&g_bc, SLOT_B));
    ET_CHECK(!et_bootctl_should_rollback(&g_bc, SLOT_B));   /* 确认后不回滚 */
}

static void verify_hdr_checks(void)
{
    uint8_t *mem;

    bc_fresh();
    write_image(SLOT_B, 64u, 1u, 0, 0);
    ET_CHECK(et_bootctl_verify_image(&g_bc, SLOT_B));

    mem = port_host_flash_mem(SLOT_B * SZ);
    ((volatile uint8_t *)mem)[0] ^= 0xFFu;              /* magic 坏 */
    ET_CHECK(!et_bootctl_verify_image(&g_bc, SLOT_B));
    ((volatile uint8_t *)mem)[0] ^= 0xFFu;

    ((volatile uint8_t *)mem)[4] = 9u;                  /* hdr_ver 未知 */
    ET_CHECK(!et_bootctl_verify_image(&g_bc, SLOT_B));
    ((volatile uint8_t *)mem)[4] = 1u;

    ((volatile uint8_t *)mem)[24] = 1u;                 /* reserved 非零 */
    ET_CHECK(!et_bootctl_verify_image(&g_bc, SLOT_B));
    ((volatile uint8_t *)mem)[24] = 0u;

    ((volatile uint8_t *)mem)[28] ^= 0xFFu;             /* 头 CRC 坏 */
    ET_CHECK(!et_bootctl_verify_image(&g_bc, SLOT_B));
}

static void verify_img_size_domain(void)
{
    bc_fresh();
    write_image(SLOT_B, 0u, 1u, 0, 0);                  /* size 0 */
    ET_CHECK(!et_bootctl_verify_image(&g_bc, SLOT_B));

    write_image(SLOT_B, SZ - 31u, 1u, 0, 0);            /* 超出槽容量 */
    ET_CHECK(!et_bootctl_verify_image(&g_bc, SLOT_B));

    write_image(SLOT_B, SZ - 32u, 1u, 0, 0);            /* 恰好占满 */
    ET_CHECK(et_bootctl_verify_image(&g_bc, SLOT_B));
}

static void verify_img_crc(void)
{
    bc_fresh();
    write_image(SLOT_B, 64u, 1u, 1, 0);                 /* 镜像 CRC 不符 */
    ET_CHECK(!et_bootctl_verify_image(&g_bc, SLOT_B));
    write_image(SLOT_B, 64u, 1u, 0, 0);
    ET_CHECK(et_bootctl_verify_image(&g_bc, SLOT_B));
}

static void abandon_resets(void)
{
    et_bootctl_state_t st;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    (void)et_bootctl_boot_attempt(&g_bc, SLOT_B);
    ET_CHECK(et_bootctl_abandon(&g_bc));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.staged_slot);
    ET_CHECK_U32_EQ(0u, st.attempts);
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_A));          /* 互斥解除 */
}

/* ---- 掉电矩阵: 断点构造辅助 (直接改写记录字节, 模拟半写落盘) ---- */

/* STG 记录位于 idx0; idx1.. 为后续记录 */
static void pcut_stage_val_inv_missing(void)
{
    et_bootctl_state_t st;
    uint8_t *r;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    r = rec_mem(0u);                                    /* val 全落盘, inv 未写 */
    ((volatile uint8_t *)r)[4] = 0xFFu;
    ((volatile uint8_t *)r)[5] = 0xFFu;
    ((volatile uint8_t *)r)[6] = 0xFFu;
    ((volatile uint8_t *)r)[7] = 0xFFu;

    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));           /* 脏尾修复重放 */
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.staged_slot);      /* 丢最后一步 */
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_A));          /* 追加能力恢复 */
}

static void pcut_stage_val_half(void)
{
    et_bootctl_state_t st;
    uint8_t *r;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    r = rec_mem(0u);
    ((volatile uint8_t *)r)[2] = 0xFFu;                 /* val 半字截断 */
    ((volatile uint8_t *)r)[3] = 0xFFu;
    ((volatile uint8_t *)r)[4] = 0xFFu;
    ((volatile uint8_t *)r)[5] = 0xFFu;
    ((volatile uint8_t *)r)[6] = 0xFFu;
    ((volatile uint8_t *)r)[7] = 0xFFu;

    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.staged_slot);
}

static void pcut_attempt_mid_corrupt(void)
{
    uint8_t *r;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(1u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(2u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    r = rec_mem(2u);                                    /* 第 2 条 ATT 损坏 */
    ((volatile uint8_t *)r)[6] = 0x00u;
    ((volatile uint8_t *)r)[7] = 0x11u;
    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));           /* 修复重放: 计数回 1 */
    ET_CHECK_U32_EQ(2u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
}

static void pcut_attempt_val_half(void)
{
    et_bootctl_state_t st;
    uint8_t *r;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(1u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    ET_CHECK_U32_EQ(2u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    r = rec_mem(2u);                                    /* 末条 ATT val 半写 */
    ((volatile uint8_t *)r)[2] = 0xFFu;
    ((volatile uint8_t *)r)[3] = 0xFFu;
    ((volatile uint8_t *)r)[4] = 0xFFu;
    ((volatile uint8_t *)r)[5] = 0xFFu;
    ((volatile uint8_t *)r)[6] = 0xFFu;
    ((volatile uint8_t *)r)[7] = 0xFFu;

    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(1u, st.attempts);                   /* 丢最后一步 */
}

static void pcut_confirm_inv_corrupt(void)
{
    et_bootctl_state_t st;
    uint8_t *r;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    ET_CHECK(et_bootctl_confirm(&g_bc, SLOT_B));
    r = rec_mem(1u);                                    /* CNF inv 损坏 */
    ((volatile uint8_t *)r)[5] = 0x00u;

    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.confirmed_slot);   /* 确认丢失 */
    ET_CHECK_U32_EQ(1u, (uint32_t)st.staged_slot);      /* staged 保留 */
}

static void pcut_confirm_val_half(void)
{
    et_bootctl_state_t st;
    uint8_t *r;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    ET_CHECK(et_bootctl_confirm(&g_bc, SLOT_B));
    r = rec_mem(1u);                                    /* CNF val 半字截断 */
    ((volatile uint8_t *)r)[0] = 0xFFu;
    ((volatile uint8_t *)r)[1] = 0xFFu;
    ((volatile uint8_t *)r)[4] = 0xFFu;
    ((volatile uint8_t *)r)[5] = 0xFFu;
    ((volatile uint8_t *)r)[6] = 0xFFu;
    ((volatile uint8_t *)r)[7] = 0xFFu;

    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.confirmed_slot);
    ET_CHECK_U32_EQ(1u, (uint32_t)st.staged_slot);
}

static void records_full_rejected(void)
{
    uint32_t i, cap;

    bc_fresh();
    cap = (SZ - 12u) / 8u;                              /* 记录区容量 */
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));          /* 1 条 */
    for (i = 1u; i < cap; i++) {
        ET_CHECK_U32_EQ(i, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    }
    /* 记录区已满: 再计数拒绝增长, 返回现值 */
    ET_CHECK_U32_EQ(cap - 1u, et_bootctl_boot_attempt(&g_bc, SLOT_B));
    ET_CHECK(!et_bootctl_stage(&g_bc, SLOT_A));
    ET_CHECK(et_bootctl_abandon(&g_bc));                /* 重建恢复 */
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
}

static void init_partial_erase_heal(void)
{
    et_bootctl_state_t st;

    bc_fresh();
    ET_CHECK(et_bootctl_stage(&g_bc, SLOT_B));
    (void)et_bootctl_boot_attempt(&g_bc, SLOT_B);

    /* 头打坏 + host 半扇区擦除注入 (前 512B): init 自愈重建,
     * 记录区前段一并被抹 → 状态安全回落初始态 */
    ((volatile uint8_t *)port_host_flash_mem(ST_SEC * SZ))[0] = 0x00u;
    port_host_flash_erase_fail_once();
    ET_CHECK(et_bootctl_init(&g_bc, &g_cfg));
    et_bootctl_state(&g_bc, &st);
    ET_CHECK(st.state_ok);
    ET_CHECK_U32_EQ(-1, (uint32_t)st.staged_slot);      /* 状态丢失但机器健康 */
    ET_CHECK_U32_EQ(0u, st.attempts);
}

static const et_test_case_t g_cases[] = {
    { "bc.init_validation",         init_validation },
    { "bc.init_fresh_selfheal",     init_fresh_selfheal },
    { "bc.init_corrupt_selfheal",   init_corrupt_selfheal },
    { "bc.stage_and_state",         stage_and_state_query },
    { "bc.stage_idempotent",        stage_idempotent_same },
    { "bc.stage_mutex",             stage_mutex_other_slot },
    { "bc.stage_after_confirm",     stage_after_confirm_rejected },
    { "bc.confirm_flow",            confirm_flow },
    { "bc.attempt_increments",      attempt_increments },
    { "bc.attempt_without_stage",   attempt_without_stage },
    { "bc.rollback_threshold",      rollback_threshold },
    { "bc.no_rollback_confirmed",   no_rollback_when_confirmed },
    { "bc.verify_hdr_checks",       verify_hdr_checks },
    { "bc.verify_img_size",         verify_img_size_domain },
    { "bc.verify_img_crc",          verify_img_crc },
    { "bc.abandon_resets",          abandon_resets },
    { "pcut.stage_inv_missing",     pcut_stage_val_inv_missing },
    { "pcut.stage_val_half",        pcut_stage_val_half },
    { "pcut.attempt_mid_corrupt",   pcut_attempt_mid_corrupt },
    { "pcut.attempt_val_half",      pcut_attempt_val_half },
    { "pcut.confirm_inv_corrupt",   pcut_confirm_inv_corrupt },
    { "pcut.confirm_val_half",      pcut_confirm_val_half },
    { "bc.records_full",            records_full_rejected },
    { "bc.init_partial_erase",      init_partial_erase_heal },
};

const et_test_case_t *test_bootctl_cases(size_t *count)
{
    *count = sizeof(g_cases) / sizeof(g_cases[0]);
    return g_cases;
}

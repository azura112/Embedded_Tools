/**
 * @file    test_map.c
 * @brief   et_map 单元测试 (插查删改/碰撞链/探测上限退化/遍历安全)
 *
 * 桶推导: hash = (k * 2654435761) % cap, 该常量 ≡ 1 (mod 8),
 * 故 cap=8 时键 1/9/17/25/... 全部落桶 1 —— 线性探测链可精确推演。
 */
#include <string.h>
#include "et_test.h"
#include "et_map.h"

#define CAP         8u
#define LIMIT       8u

static et_map_t      g_m;
static et_map_slot_t g_mem[CAP];
static uint32_t      g_visited;

static void setup(uint32_t probe_limit)
{
    memset(g_mem, 0xA5, sizeof(g_mem));     /* 预填垃圾: 验证 init 归零 */
    ET_CHECK(et_map_init(&g_m, g_mem, CAP, probe_limit));
}

static void visit_collect(et_map_slot_t *slot, void *user);
static void visit_del_current(et_map_slot_t *slot, void *user);

static void map_init_rejects_bad(void)
{
    et_map_slot_t mem[CAP];

    ET_CHECK(!et_map_init(NULL, mem, CAP, LIMIT));
    ET_CHECK(!et_map_init(&g_m, NULL, CAP, LIMIT));
    ET_CHECK(!et_map_init(&g_m, mem, 0u, LIMIT));
    ET_CHECK(!et_map_init(&g_m, mem, CAP, 0u));     /* 探测上限 ≥1 */
}

static void map_put_get_basic(void)
{
    uint32_t v = 0u;

    setup(LIMIT);
    ET_CHECK(et_map_put(&g_m, 1u, 100u));
    ET_CHECK(et_map_put(&g_m, 2u, 200u));
    ET_CHECK(et_map_put(&g_m, 3u, 300u));
    ET_CHECK(et_map_get(&g_m, 2u, &v));
    ET_CHECK_U32_EQ(200u, v);
    ET_CHECK(!et_map_get(&g_m, 9u, &v));            /* 未命中 */
    ET_CHECK(et_map_get(&g_m, 2u, NULL));           /* val 可空 */
}

static void map_reserved_keys_rejected(void)
{
    setup(LIMIT);
    ET_CHECK(!et_map_put(&g_m, 0u, 1u));            /* 键 0 = 空槽标记 */
    ET_CHECK(!et_map_put(&g_m, 0xFFFFFFFFu, 1u));   /* 键 ~0 = 墓碑 */
    ET_CHECK(!et_map_get(&g_m, 0u, NULL));
    ET_CHECK(!et_map_get(&g_m, 0xFFFFFFFFu, NULL));
    ET_CHECK(!et_map_del(&g_m, 0u));
    ET_CHECK_U32_EQ(0u, et_map_count(&g_m));
}

static void map_duplicate_overwrites(void)
{
    uint32_t v = 0u;

    setup(LIMIT);
    ET_CHECK(et_map_put(&g_m, 7u, 1u));
    ET_CHECK(et_map_put(&g_m, 7u, 2u));             /* 覆盖 */
    ET_CHECK_U32_EQ(1u, et_map_count(&g_m));        /* 计数不涨 */
    ET_CHECK(et_map_get(&g_m, 7u, &v));
    ET_CHECK_U32_EQ(2u, v);
}

static void map_count_and_clear(void)
{
    setup(LIMIT);
    (void)et_map_put(&g_m, 1u, 11u);
    (void)et_map_put(&g_m, 2u, 22u);
    ET_CHECK_U32_EQ(2u, et_map_count(&g_m));

    et_map_clear(&g_m);
    ET_CHECK_U32_EQ(0u, et_map_count(&g_m));
    ET_CHECK(!et_map_get(&g_m, 1u, NULL));
    ET_CHECK(et_map_put(&g_m, 3u, 33u));            /* 清空后可复用 */
    ET_CHECK(et_map_get(&g_m, 3u, NULL));
}

static void map_collide_linear_probe(void)
{
    uint32_t i;
    uint32_t v;

    setup(LIMIT);
    for (i = 0u; i < 4u; i++) {                     /* 1/9/17/25 同桶成链 */
        ET_CHECK(et_map_put(&g_m, 1u + i * 8u, 100u + i));
    }
    ET_CHECK_U32_EQ(4u, et_map_count(&g_m));
    for (i = 0u; i < 4u; i++) {
        ET_CHECK(et_map_get(&g_m, 1u + i * 8u, &v));
        ET_CHECK_U32_EQ(100u + i, v);
    }
    ET_CHECK(!et_map_get(&g_m, 33u, &v));           /* 同桶第 5 个不存在 */
}

static void map_delete_reuses_hole(void)
{
    uint32_t v = 0u;

    setup(LIMIT);
    (void)et_map_put(&g_m, 1u, 11u);
    (void)et_map_put(&g_m, 9u, 22u);
    (void)et_map_put(&g_m, 17u, 33u);
    ET_CHECK(et_map_del(&g_m, 9u));                 /* 链中间删 → 墓碑 */
    ET_CHECK_U32_EQ(2u, et_map_count(&g_m));
    ET_CHECK(!et_map_get(&g_m, 9u, &v));
    ET_CHECK(et_map_get(&g_m, 17u, &v));            /* 链尾不受墓碑影响 */
    ET_CHECK_U32_EQ(33u, v);

    ET_CHECK(et_map_put(&g_m, 25u, 44u));           /* 新键复用墓碑位 */
    ET_CHECK_U32_EQ(3u, et_map_count(&g_m));
    ET_CHECK(et_map_get(&g_m, 25u, &v));
    ET_CHECK_U32_EQ(44u, v);
    ET_CHECK(et_map_get(&g_m, 17u, &v));            /* 原链仍可查 */
}

static void map_full_table_rejects(void)
{
    uint32_t i;
    uint32_t v = 0u;

    setup(16u);                                     /* 上限 ≥ cap: 满表可填满 */
    for (i = 1u; i <= CAP; i++) {
        ET_CHECK(et_map_put(&g_m, i * 8u + 1u, i)); /* 同桶链填满全表 */
    }
    ET_CHECK_U32_EQ(CAP, et_map_count(&g_m));
    ET_CHECK(!et_map_put(&g_m, 999u, 1u));          /* 满表拒绝 */
    for (i = 1u; i <= CAP; i++) {
        ET_CHECK(et_map_get(&g_m, i * 8u + 1u, &v));
        ET_CHECK_U32_EQ(i, v);
    }
    ET_CHECK(et_map_del(&g_m, 9u));
    ET_CHECK(!et_map_put(&g_m, 999u, 7u));  /* 全表无空槽: 有墓碑也拒(经典语义) */
    ET_CHECK(et_map_del(&g_m, 17u));
    ET_CHECK(!et_map_put(&g_m, 999u, 7u));
    et_map_clear(&g_m);
    ET_CHECK(et_map_put(&g_m, 999u, 7u));   /* 清空后恢复 */
    ET_CHECK(et_map_get(&g_m, 999u, &v));
    ET_CHECK_U32_EQ(7u, v);
}

static void map_probe_limit_degrades(void)
{
    uint32_t v = 0u;

    setup(2u);                                      /* 探测上限 2 */
    ET_CHECK(et_map_put(&g_m, 1u, 1u));             /* 1 次探测 */
    ET_CHECK(et_map_put(&g_m, 9u, 2u));             /* 2 次 = 上限内 */
    ET_CHECK(!et_map_put(&g_m, 17u, 3u));           /* 需 3 次 → 超限拒绝 */
    ET_CHECK(et_map_get(&g_m, 9u, &v));             /* 已入键不受限影响 */
    ET_CHECK_U32_EQ(2u, v);
    ET_CHECK(!et_map_get(&g_m, 17u, &v));
}

static void map_probe_limit_edge(void)
{
    uint32_t v = 0u;

    setup(3u);                                      /* 探测上限 3 */
    (void)et_map_put(&g_m, 1u, 1u);
    (void)et_map_put(&g_m, 9u, 2u);
    ET_CHECK(et_map_put(&g_m, 17u, 3u));            /* 恰 3 次 = 上限, 允许 */
    ET_CHECK(!et_map_put(&g_m, 25u, 4u));           /* 需 4 次 → 拒绝 */
    ET_CHECK(et_map_get(&g_m, 17u, &v));
    ET_CHECK(et_map_del(&g_m, 1u));                 /* 链头删 → 墓碑 */
    ET_CHECK(et_map_get(&g_m, 17u, &v));            /* 墓碑计入探测但仍 ≤ 上限 */
    ET_CHECK(et_map_del(&g_m, 9u));
    ET_CHECK(et_map_get(&g_m, 17u, &v));            /* 双墓碑仍 3 次可达 */
}

static void visit_collect(et_map_slot_t *slot, void *user)
{
    g_visited++;
    *(uint32_t *)user |= slot->key;
}

static void visit_del_current(et_map_slot_t *slot, void *user)
{
    g_visited++;
    ET_CHECK(et_map_del((et_map_t *)user, slot->key));
}

static void map_foreach_and_delete(void)
{
    static et_map_t     m2;
    static et_map_slot_t mem2[CAP];
    uint32_t            keys_seen = 0u;

    ET_CHECK(et_map_init(&m2, mem2, CAP, LIMIT));
    (void)et_map_put(&m2, 1u, 11u);
    (void)et_map_put(&m2, 9u, 22u);
    (void)et_map_put(&m2, 17u, 33u);

    g_visited = 0u;                                 /* 遍历中删当前槽 */
    ET_CHECK(et_map_foreach(&m2, visit_del_current, &m2));
    ET_CHECK_U32_EQ(3u, g_visited);
    ET_CHECK_U32_EQ(0u, et_map_count(&m2));

    (void)et_map_put(&m2, 2u, 22u);                 /* 遍历安全后仍可正常用 */
    (void)et_map_put(&m2, 3u, 33u);
    g_visited = 0u;
    keys_seen = 0u;
    ET_CHECK(et_map_foreach(&m2, visit_collect, &keys_seen));
    ET_CHECK_U32_EQ(2u, g_visited);
    ET_CHECK_U32_EQ(2u | 3u, keys_seen);
}

static void map_multi_instance(void)
{
    static et_map_t      m2;
    static et_map_slot_t mem2[4];
    uint32_t             v = 0u;

    setup(LIMIT);
    (void)et_map_put(&g_m, 1u, 111u);
    ET_CHECK(et_map_init(&m2, mem2, 4u, 4u));
    (void)et_map_put(&m2, 1u, 999u);
    ET_CHECK(et_map_get(&g_m, 1u, &v));             /* 实例隔离 */
    ET_CHECK_U32_EQ(111u, v);
    ET_CHECK(et_map_get(&m2, 1u, &v));
    ET_CHECK_U32_EQ(999u, v);
}

static void map_extreme_values(void)
{
    uint32_t v = 0u;

    setup(LIMIT);
    ET_CHECK(et_map_put(&g_m, 0xFFFFFFFEu, 0xDEADBEEFu));   /* 最大可用键 */
    ET_CHECK(et_map_get(&g_m, 0xFFFFFFFEu, &v));
    ET_CHECK_U32_EQ(0xDEADBEEFu, v);
    ET_CHECK(et_map_put(&g_m, 0xFFFFFFFDu, 0xCAFEBABEu));
    ET_CHECK(et_map_get(&g_m, 0xFFFFFFFDu, &v));
}

const et_test_case_t *test_map_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"map.init_rejects_bad",    map_init_rejects_bad},
        {"map.put_get_basic",       map_put_get_basic},
        {"map.reserved_keys",       map_reserved_keys_rejected},
        {"map.duplicate_overwrite", map_duplicate_overwrites},
        {"map.count_clear",         map_count_and_clear},
        {"map.collide_probe",       map_collide_linear_probe},
        {"map.delete_reuse_hole",   map_delete_reuses_hole},
        {"map.full_table_reject",   map_full_table_rejects},
        {"map.probe_limit_degrade", map_probe_limit_degrades},
        {"map.probe_limit_edge",    map_probe_limit_edge},
        {"map.foreach_del_safe",    map_foreach_and_delete},
        {"map.multi_instance",      map_multi_instance},
        {"map.extreme_values",      map_extreme_values},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

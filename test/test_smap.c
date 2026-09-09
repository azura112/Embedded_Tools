/**
 * @file    test_smap.c
 * @brief   et_smap 单元测试 (插查删改/拷入语义/池治理/碰撞链/退化/遍历安全)
 *
 * 探测与墓碑语义与 et_map 对齐 (见 et_smap.h 交叉引用注), 用例侧重
 * 字符串键特有行为: 键长校验、池预留-提交、大小写敏感、NUL 拷入。
 */
#include <string.h>
#include "et_test.h"
#include "et_smap.h"

#define CAP         8u
#define LIMIT       8u
#define POOL_SZ     160u            /* 宽松池: 最多 8 条最坏键 (8×20) */

static et_smap_t      g_m;
static et_smap_slot_t g_mem[CAP];
static uint8_t        g_pool[POOL_SZ];
static uint32_t       g_visited;
static uint32_t       g_vsum;

static void setup(uint32_t probe_limit, uint32_t pool_sz)
{
    memset(g_mem, 0xA5, sizeof(g_mem));     /* 预填垃圾: 验证 init 归零 */
    memset(g_pool, 0x5A, sizeof(g_pool));
    ET_CHECK(et_smap_init(&g_m, g_mem, CAP, g_pool, pool_sz, probe_limit));
}

static void visit_sum(void *user, const char *key, uint32_t *val)
{
    (void)user;
    (void)key;
    g_visited++;
    g_vsum += *val;
}

static void visit_bump(void *user, const char *key, uint32_t *val)
{
    (void)user;
    (void)key;
    g_visited++;
    (*val)++;                               /* 遍历中改值即生效 */
}

static void visit_del_one(void *user, const char *key, uint32_t *val)
{
    (void)user;
    (void)val;
    g_visited++;
    if (strcmp(key, "b") == 0) {
        (void)et_smap_del(&g_m, "b");       /* 遍历中删当前槽安全 */
    }
}

static void smap_init_rejects_bad(void)
{
    et_smap_slot_t mem[CAP];
    uint8_t        pool[POOL_SZ];

    ET_CHECK(!et_smap_init(NULL, mem, CAP, pool, POOL_SZ, LIMIT));
    ET_CHECK(!et_smap_init(&g_m, NULL, CAP, pool, POOL_SZ, LIMIT));
    ET_CHECK(!et_smap_init(&g_m, mem, CAP, NULL, POOL_SZ, LIMIT));
    ET_CHECK(!et_smap_init(&g_m, mem, 0u, pool, POOL_SZ, LIMIT));
    ET_CHECK(!et_smap_init(&g_m, mem, CAP, pool, POOL_SZ, 0u));
    /* 池装不下一条最坏键: 4 保留 + ((16+1)+3)&~3=20 → 需 ≥24 */
    ET_CHECK(!et_smap_init(&g_m, mem, CAP, pool, 23u, LIMIT));
    ET_CHECK(et_smap_init(&g_m, mem, CAP, pool, 24u, LIMIT));
}

static void smap_put_get_basic(void)
{
    uint32_t v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, "led", 100u));
    ET_CHECK(et_smap_put(&g_m, "btn", 200u));
    ET_CHECK(et_smap_put(&g_m, "uart1", 300u));
    ET_CHECK(et_smap_get(&g_m, "btn", &v));
    ET_CHECK_U32_EQ(200u, v);
    ET_CHECK(!et_smap_get(&g_m, "btn2", &v));           /* 前缀相同不误命中 */
    ET_CHECK(et_smap_get(&g_m, "led", NULL));           /* 只测存在 */
    ET_CHECK_U32_EQ(3u, et_smap_count(&g_m));
}

static void smap_key_invalid_rejected(void)
{
    setup(LIMIT, POOL_SZ);
    ET_CHECK(!et_smap_put(&g_m, "", 1u));               /* 空键 */
    ET_CHECK(!et_smap_put(&g_m, NULL, 1u));
    ET_CHECK(!et_smap_put(&g_m, "abcdefghijklmnopq", 1u));  /* 17B > KEY_MAX 16 */
    ET_CHECK(et_smap_put(&g_m, "abcdefghijklmnop", 1u));    /* 恰 16B 合法 */
    ET_CHECK(!et_smap_get(&g_m, "", NULL));
    ET_CHECK(!et_smap_del(&g_m, NULL));
    ET_CHECK_U32_EQ(1u, et_smap_count(&g_m));
}

static void smap_duplicate_overwrites(void)
{
    uint32_t v = 0u;
    uint32_t pf;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, "k", 1u));
    pf = et_smap_pool_free(&g_m);
    ET_CHECK(et_smap_put(&g_m, "k", 2u));               /* 覆盖不重复占池 */
    ET_CHECK_U32_EQ(pf, et_smap_pool_free(&g_m));
    ET_CHECK(et_smap_get(&g_m, "k", &v));
    ET_CHECK_U32_EQ(2u, v);
    ET_CHECK_U32_EQ(1u, et_smap_count(&g_m));
}

static void smap_copy_semantics(void)
{
    char     buf[8] = "abc";
    uint32_t v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, buf, 7u));
    buf[0] = 'X';                                         /* 原串被调用方改写 */
    ET_CHECK(et_smap_get(&g_m, "abc", &v));               /* 池内键不受影响 */
    ET_CHECK_U32_EQ(7u, v);
    ET_CHECK(!et_smap_get(&g_m, "Xbc", &v));
}

static void smap_delete_reuses_hole(void)
{
    uint32_t v = 0u;
    uint32_t pf_before;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, "a", 1u));
    ET_CHECK(et_smap_put(&g_m, "b", 2u));
    ET_CHECK(et_smap_del(&g_m, "a"));
    ET_CHECK(!et_smap_get(&g_m, "a", &v));
    ET_CHECK_U32_EQ(1u, et_smap_count(&g_m));
    pf_before = et_smap_pool_free(&g_m);
    ET_CHECK(et_smap_put(&g_m, "a", 3u));                 /* 墓碑位复用 */
    /* 槽复用但键在池内新拷一份 ("a"=1B+NUL→4B 对齐): 高水位 +4 */
    ET_CHECK_U32_EQ(pf_before - 4u, et_smap_pool_free(&g_m));
    ET_CHECK(et_smap_get(&g_m, "a", &v));
    ET_CHECK_U32_EQ(3u, v);
    ET_CHECK_U32_EQ(2u, et_smap_count(&g_m));
    ET_CHECK(!et_smap_del(&g_m, "zz"));                   /* 不存在的删除 */
}

static void smap_pool_full_reject(void)
{
    uint32_t v = 0u;

    /* 池恰够 2 条 16B 最坏键: 4 保留 + 2×20 = 44B (init 最低线为 24) */
    setup(LIMIT, 44u);
    ET_CHECK(et_smap_put(&g_m, "abcdefghijklmnop", 1u));
    ET_CHECK(et_smap_put(&g_m, "bbbbbbbbbbbbbbbb", 2u));
    ET_CHECK_U32_EQ(0u, et_smap_pool_free(&g_m));
    ET_CHECK(!et_smap_put(&g_m, "cccccccccccccccc", 3u)); /* 池满拒绝 */
    ET_CHECK(et_smap_put(&g_m, "abcdefghijklmnop", 9u));  /* 覆盖不受池限 */
    ET_CHECK(et_smap_get(&g_m, "abcdefghijklmnop", &v));
    ET_CHECK_U32_EQ(9u, v);
    ET_CHECK(!et_smap_get(&g_m, "cccccccccccccccc", &v)); /* 拒绝无副作用 */
    ET_CHECK(et_smap_del(&g_m, "abcdefghijklmnop"));      /* 删也不能腾池: */
    ET_CHECK(!et_smap_put(&g_m, "dddddddddddddddd", 4u)); /* 墓碑不回收池 */
}

static void smap_full_table_rejects(void)
{
    /* 填满 4 槽 (cap=4, 池充足): 第 5 条必被拒 (无 EMPTY 终止, 经典语义) */
    et_smap_slot_t mem[4];
    uint8_t        pool[128];
    et_smap_t      m;

    ET_CHECK(et_smap_init(&m, mem, 4u, pool, sizeof(pool), 4u));
    ET_CHECK(et_smap_put(&m, "w", 1u));
    ET_CHECK(et_smap_put(&m, "x", 2u));
    ET_CHECK(et_smap_put(&m, "y", 3u));
    ET_CHECK(et_smap_put(&m, "z", 4u));
    ET_CHECK(et_smap_del(&m, "w"));                       /* 留墓碑但无空槽 */
    ET_CHECK(!et_smap_put(&m, "new", 5u));                /* 满表: 有墓碑也拒 */
}

static uint32_t t_fnv(const char *k)
{
    uint32_t h = 2166136261u;
    while (*k != '\0') {
        h ^= (uint8_t)*k++;
        h *= 16777619u;
    }
    return h;
}

static void smap_probe_limit_degrades(void)
{
    et_smap_slot_t mem[8];
    uint8_t        pool[128];
    et_smap_t      m;
    char           k1[4] = "aa";
    char           k2[4] = "ab";
    uint32_t       i;
    uint32_t       v = 0u;

    /* 搜索同桶键对 (cap=8): limit=1 时第二条 put/get 在探测上限内被拒 */
    for (i = 0u; i < 255u; i++) {
        k2[1] = (char)('a' + (i / 26u));
        k2[2] = (char)('a' + (i % 26u));
        if ((t_fnv(k1) % 8u) == (t_fnv(k2) % 8u) && (strcmp(k1, k2) != 0)) {
            break;
        }
    }
    ET_CHECK(i < 255u);                                   /* 碰撞对必须找到 */
    ET_CHECK(et_smap_init(&m, mem, 8u, pool, sizeof(pool), 1u));
    ET_CHECK(et_smap_put(&m, k1, 1u));
    ET_CHECK(!et_smap_put(&m, k2, 2u));                   /* 同桶, limit=1 拒 */
    ET_CHECK(!et_smap_get(&m, k2, &v));
    ET_CHECK(et_smap_get(&m, k1, &v));
    ET_CHECK_U32_EQ(1u, v);
}

static void smap_case_sensitive(void)
{
    uint32_t v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, "led", 1u));
    ET_CHECK(et_smap_put(&g_m, "LED", 2u));
    ET_CHECK(et_smap_get(&g_m, "led", &v));
    ET_CHECK_U32_EQ(1u, v);
    ET_CHECK(et_smap_get(&g_m, "LED", &v));
    ET_CHECK_U32_EQ(2u, v);
    ET_CHECK_U32_EQ(2u, et_smap_count(&g_m));
}

static void smap_foreach_collect_and_modify(void)
{
    uint32_t v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, "p", 10u));
    ET_CHECK(et_smap_put(&g_m, "q", 20u));
    g_visited = 0u; g_vsum = 0u;
    ET_CHECK(et_smap_foreach(&g_m, visit_sum, NULL));
    ET_CHECK_U32_EQ(2u, g_visited);
    ET_CHECK_U32_EQ(30u, g_vsum);
    ET_CHECK(et_smap_foreach(&g_m, visit_bump, NULL));   /* 遍历中改值 */
    ET_CHECK(et_smap_get(&g_m, "p", &v));
    ET_CHECK_U32_EQ(11u, v);
    ET_CHECK(!et_smap_foreach(&g_m, NULL, NULL));
}

static void smap_foreach_del_safe(void)
{
    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, "a", 1u));
    ET_CHECK(et_smap_put(&g_m, "b", 2u));
    ET_CHECK(et_smap_put(&g_m, "c", 3u));
    g_visited = 0u;
    ET_CHECK(et_smap_foreach(&g_m, visit_del_one, NULL));
    ET_CHECK_U32_EQ(3u, g_visited);                       /* 删当前不中断遍历 */
    ET_CHECK_U32_EQ(2u, et_smap_count(&g_m));
}

static void smap_multi_instance(void)
{
    et_smap_slot_t mem2[CAP];
    uint8_t        pool2[POOL_SZ];
    et_smap_t      m2;
    uint32_t       v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_init(&m2, mem2, CAP, pool2, POOL_SZ, LIMIT));
    ET_CHECK(et_smap_put(&g_m, "x", 1u));
    ET_CHECK(et_smap_put(&m2, "x", 2u));
    ET_CHECK(et_smap_get(&g_m, "x", &v));
    ET_CHECK_U32_EQ(1u, v);
    ET_CHECK(et_smap_get(&m2, "x", &v));
    ET_CHECK_U32_EQ(2u, v);
    et_smap_clear(&g_m);
    ET_CHECK_U32_EQ(0u, et_smap_count(&g_m));
    ET_CHECK_U32_EQ(1u, et_smap_count(&m2));
    ET_CHECK_U32_EQ(POOL_SZ - 4u, et_smap_pool_free(&g_m));      /* 池水位复位 */
}

static void smap_long_key_boundary(void)
{
    char     k16[17] = "0123456789abcdef";             /* 恰 16 */
    char     k17[18] = "0123456789abcdefg";            /* 17 超界 */
    uint32_t v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, k16, 1u));
    ET_CHECK(et_smap_get(&g_m, k16, &v));
    ET_CHECK_U32_EQ(1u, v);
    ET_CHECK(!et_smap_put(&g_m, k17, 2u));
    /* 最坏键占池 20B: 16 字符键拷入 17B → 对齐后 20 */
    ET_CHECK_U32_EQ(POOL_SZ - 4u - 20u, et_smap_pool_free(&g_m));
}

static void smap_ci_basic(void)
{
    uint32_t v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put_ci(&g_m, "LED", 7u));          /* 折叠入表面 */
    ET_CHECK(et_smap_get_ci(&g_m, "lEd", &v));
    ET_CHECK_U32_EQ(7u, v);
    ET_CHECK(!et_smap_get(&g_m, "LED", &v));            /* 敏感面看不见 */
    ET_CHECK(et_smap_get(&g_m, "led", &v));             /* 折叠形式可见 */
    ET_CHECK_U32_EQ(7u, v);
    ET_CHECK_U32_EQ(1u, et_smap_count(&g_m));
}

static void smap_ci_cs_coexist(void)
{
    uint32_t v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put(&g_m, "LED", 1u));             /* 敏感条目 */
    ET_CHECK(et_smap_put_ci(&g_m, "LED", 2u));          /* 折叠成 "led": 新条目 */
    ET_CHECK_U32_EQ(2u, et_smap_count(&g_m));
    ET_CHECK(et_smap_get(&g_m, "LED", &v));
    ET_CHECK_U32_EQ(1u, v);
    ET_CHECK(et_smap_get_ci(&g_m, "LED", &v));
    ET_CHECK_U32_EQ(2u, v);
    ET_CHECK(et_smap_get(&g_m, "led", &v));             /* 敏感面查折叠键 */
    ET_CHECK_U32_EQ(2u, v);
}

static void smap_ci_boundary_and_bad(void)
{
    uint32_t v = 0u;

    setup(LIMIT, POOL_SZ);
    ET_CHECK(!et_smap_put_ci(&g_m, "", 1u));            /* 空键 */
    ET_CHECK(!et_smap_put_ci(&g_m, NULL, 1u));
    ET_CHECK(!et_smap_put_ci(&g_m, "abcdefghijklmnopq", 1u)); /* 17 超界 */
    ET_CHECK(et_smap_put_ci(&g_m, "AbCdEfGhIjKlMnOp", 9u));   /* 恰 16 混合大小写 */
    ET_CHECK(et_smap_get_ci(&g_m, "aBcDeFgHiJkLmNoP", &v));
    ET_CHECK_U32_EQ(9u, v);
    ET_CHECK_U32_EQ(POOL_SZ - 4u - 20u, et_smap_pool_free(&g_m)); /* 占 20B 对齐 */
}

static void smap_ci_delete_path(void)
{
    setup(LIMIT, POOL_SZ);
    ET_CHECK(et_smap_put_ci(&g_m, "Fan", 1u));
    ET_CHECK(et_smap_del_ci(&g_m, "FAN"));              /* 折叠面命中 */
    ET_CHECK(!et_smap_get_ci(&g_m, "fan", NULL));
    ET_CHECK(!et_smap_del_ci(&g_m, "fan"));             /* 二次删: 未命中 */
    ET_CHECK(!et_smap_get(&g_m, "Fan", NULL));         /* 敏感面本就无此键 */
}

const et_test_case_t *test_smap_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"smap.init_rejects_bad",       smap_init_rejects_bad},
        {"smap.put_get_basic",          smap_put_get_basic},
        {"smap.key_invalid_rejected",   smap_key_invalid_rejected},
        {"smap.duplicate_overwrites",   smap_duplicate_overwrites},
        {"smap.copy_semantics",         smap_copy_semantics},
        {"smap.delete_reuses_hole",     smap_delete_reuses_hole},
        {"smap.pool_full_reject",       smap_pool_full_reject},
        {"smap.full_table_rejects",     smap_full_table_rejects},
        {"smap.probe_limit_degrades",   smap_probe_limit_degrades},
        {"smap.case_sensitive",         smap_case_sensitive},
        {"smap.foreach_collect_modify", smap_foreach_collect_and_modify},
        {"smap.foreach_del_safe",       smap_foreach_del_safe},
        {"smap.multi_instance",         smap_multi_instance},
        {"smap.long_key_boundary",      smap_long_key_boundary},
        {"smap.ci_basic",               smap_ci_basic},
        {"smap.ci_cs_coexist",          smap_ci_cs_coexist},
        {"smap.ci_boundary_and_bad",    smap_ci_boundary_and_bad},
        {"smap.ci_delete_path",         smap_ci_delete_path},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

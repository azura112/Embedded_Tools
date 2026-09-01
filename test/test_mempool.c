/**
 * @file    test_mempool.c
 * @brief   et_mempool 单元测试
 */
#include "et_test.h"
#include "et_mempool.h"
#include <string.h>

#define BLK_SIZE   12u
#define BLK_COUNT  6u

static uint8_t      g_storage[128];
static et_mempool_t g_mp;

static void mp_bytes_needed_exact(void)
{
    /* 宿主机 x64: 对齐 8; 块 12->16, 位图 1 字节 -> 块区偏移 8 */
    size_t need = et_mempool_bytes_needed(BLK_SIZE, BLK_COUNT);

    ET_CHECK_U32_EQ((uint32_t)(8u + 16u * BLK_COUNT), (uint32_t)need);
    ET_CHECK(need >= (size_t)BLK_SIZE * BLK_COUNT);  /* 恒不小于裸块区大小 */
}

static void mp_init_rejects_bad(void)
{
    size_t need = et_mempool_bytes_needed(BLK_SIZE, BLK_COUNT);

    ET_CHECK(et_mempool_init(&g_mp, g_storage, sizeof(g_storage), BLK_SIZE, BLK_COUNT));
    ET_CHECK(!et_mempool_init(NULL, g_storage, 64u, 4u, 4u));
    ET_CHECK(!et_mempool_init(&g_mp, NULL, 64u, 4u, 4u));
    ET_CHECK(!et_mempool_init(&g_mp, g_storage, 64u, 0u, 4u));
    ET_CHECK(!et_mempool_init(&g_mp, g_storage, 64u, 4u, 0u));
    ET_CHECK(!et_mempool_init(&g_mp, g_storage, need - 1u, BLK_SIZE, BLK_COUNT)); /* 差一个字节也不行 */
}

static void mp_exhaust_then_null(void)
{
    void   *ptrs[BLK_COUNT];
    uint32_t i;

    ET_CHECK(et_mempool_init(&g_mp, g_storage, sizeof(g_storage), BLK_SIZE, BLK_COUNT));
    for (i = 0u; i < BLK_COUNT; i++) {
        ptrs[i] = et_mempool_alloc(&g_mp);
        ET_CHECK(ptrs[i] != NULL);
        memset(ptrs[i], 0xAB, 16u);                 /* 块对齐后为 16 字节, 可整块写 */
    }
    ET_CHECK_U32_EQ(0u, et_mempool_free_count(&g_mp));
    ET_CHECK(et_mempool_alloc(&g_mp) == NULL);      /* 耗尽返回 NULL */

    for (i = 0u; i < BLK_COUNT; i++) {
        ET_CHECK(et_mempool_free(&g_mp, ptrs[i]));
    }
    ET_CHECK_U32_EQ(BLK_COUNT, et_mempool_free_count(&g_mp));
}

static void mp_addresses_distinct(void)
{
    void   *ptrs[BLK_COUNT];
    uint32_t i;
    uint32_t j;

    ET_CHECK(et_mempool_init(&g_mp, g_storage, sizeof(g_storage), BLK_SIZE, BLK_COUNT));
    for (i = 0u; i < BLK_COUNT; i++) {
        ptrs[i] = et_mempool_alloc(&g_mp);
        ET_CHECK(ptrs[i] != NULL);
    }
    for (i = 0u; i < BLK_COUNT; i++) {
        for (j = (uint32_t)(i + 1u); j < BLK_COUNT; j++) {
            ET_CHECK(ptrs[i] != ptrs[j]);           /* 分配地址两两不同 */
        }
    }
}

static void mp_alignment(void)
{
    uint32_t i;

    ET_CHECK(et_mempool_init(&g_mp, g_storage, sizeof(g_storage), 5u, BLK_COUNT));
    for (i = 0u; i < BLK_COUNT; i++) {
        void *p = et_mempool_alloc(&g_mp);

        ET_CHECK(p != NULL);
        ET_CHECK(((uintptr_t)p % (uintptr_t)ET_MEMPOOL_ALIGN) == 0u);
    }
}

static void mp_contains_check(void)
{
    void *p;

    ET_CHECK(et_mempool_init(&g_mp, g_storage, sizeof(g_storage), BLK_SIZE, BLK_COUNT));
    p = et_mempool_alloc(&g_mp);
    ET_CHECK(p != NULL);

    ET_CHECK(et_mempool_contains(&g_mp, p));                    /* 合法块     */
    ET_CHECK(!et_mempool_contains(&g_mp, &g_storage[1]));       /* 位图区(低于块区) */
    ET_CHECK(!et_mempool_contains(&g_mp, &g_storage[sizeof(g_storage) - 1u])); /* 尾部未对齐 */
    ET_CHECK(!et_mempool_contains(&g_mp, NULL));                /* 空指针     */
}

static void mp_strict_double_free(void)
{
#if ET_MEMPOOL_STRICT
    void *p;

    ET_CHECK(et_mempool_init(&g_mp, g_storage, sizeof(g_storage), BLK_SIZE, BLK_COUNT));
    p = et_mempool_alloc(&g_mp);
    ET_CHECK(p != NULL);
    ET_CHECK_U32_EQ(BLK_COUNT - 1u, et_mempool_free_count(&g_mp));

    ET_CHECK(et_mempool_free(&g_mp, p));            /* 正常释放 */
    ET_CHECK(!et_mempool_free(&g_mp, p));           /* 重复释放被拒绝 */
    ET_CHECK(!et_mempool_free(&g_mp, &g_storage[2])); /* 越界指针被拒绝 */
    ET_CHECK_U32_EQ(BLK_COUNT, et_mempool_free_count(&g_mp)); /* 计数未被破坏 */
#else
    ET_CHECK(1);
#endif
}

static void mp_free_then_realloc_same_block(void)
{
    uint8_t *p1;
    uint8_t *p2;
    uint32_t i;

    ET_CHECK(et_mempool_init(&g_mp, g_storage, sizeof(g_storage), BLK_SIZE, BLK_COUNT));

    /* 先耗尽再释放首块, 重新申请应命中同一块(扫描提示优化路径) */
    for (i = 0u; i < BLK_COUNT; i++) {
        ET_CHECK(et_mempool_alloc(&g_mp) != NULL);
    }
    ET_CHECK_U32_EQ(0u, et_mempool_free_count(&g_mp));
    p1 = &g_storage[8];                             /* 首块的确定地址(位图占 8 字节) */
    ET_CHECK(et_mempool_free(&g_mp, p1));
    p2 = (uint8_t *)et_mempool_alloc(&g_mp);
    ET_CHECK(p2 == p1);
    ET_CHECK_U32_EQ(0u, et_mempool_free_count(&g_mp));
}

static void mp_interleave_pattern(void)
{
    void   *a;
    void   *b;
    void   *c;
    uint32_t i;

    ET_CHECK(et_mempool_init(&g_mp, g_storage, sizeof(g_storage), BLK_SIZE, BLK_COUNT));
    a = et_mempool_alloc(&g_mp);
    b = et_mempool_alloc(&g_mp);
    ET_CHECK((a != NULL) && (b != NULL) && (a != b));
    ET_CHECK_U32_EQ(BLK_COUNT - 2u, et_mempool_free_count(&g_mp));

    ET_CHECK(et_mempool_free(&g_mp, a));
    c = et_mempool_alloc(&g_mp);                    /* 复用刚释放的低位块 */
    ET_CHECK(c == a);
    ET_CHECK_U32_EQ(BLK_COUNT - 2u, et_mempool_free_count(&g_mp));

    ET_CHECK(et_mempool_free(&g_mp, b));
    ET_CHECK(et_mempool_free(&g_mp, c));
    for (i = 0u; i < BLK_COUNT; i++) {              /* 最终应可再次全额分配 */
        ET_CHECK(et_mempool_alloc(&g_mp) != NULL);
    }
}

const et_test_case_t *test_mempool_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"mempool.bytes_needed",       mp_bytes_needed_exact},
        {"mempool.init_rejects_bad",   mp_init_rejects_bad},
        {"mempool.exhaust_then_null",  mp_exhaust_then_null},
        {"mempool.distinct_addresses", mp_addresses_distinct},
        {"mempool.alignment",          mp_alignment},
        {"mempool.contains",           mp_contains_check},
        {"mempool.strict_double_free", mp_strict_double_free},
        {"mempool.realloc_same_block", mp_free_then_realloc_same_block},
        {"mempool.interleave_pattern", mp_interleave_pattern},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

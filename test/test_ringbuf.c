/**
 * @file    test_ringbuf.c
 * @brief   et_ringbuf 单元测试
 */
#include "et_test.h"
#include "et_ringbuf.h"
#include <string.h>

static uint8_t  g_storage[16];
static et_ringbuf_t g_rb;

static void rb_init_state(void)
{
    ET_CHECK(et_ringbuf_init(&g_rb, g_storage, sizeof(g_storage)));
    ET_CHECK_U32_EQ(0u, et_ringbuf_used(&g_rb));
    ET_CHECK_U32_EQ(16u, et_ringbuf_free_space(&g_rb));
    ET_CHECK(et_ringbuf_is_empty(&g_rb));
    ET_CHECK(!et_ringbuf_is_full(&g_rb));
}

static void rb_init_rejects_bad(void)
{
    ET_CHECK(!et_ringbuf_init(NULL, g_storage, 8u));
    ET_CHECK(!et_ringbuf_init(&g_rb, NULL, 8u));
    ET_CHECK(!et_ringbuf_init(&g_rb, g_storage, 0u));
}

static void rb_write_read_basic(void)
{
    uint8_t out[8];
    uint8_t in[] = {1u, 2u, 3u, 4u, 5u};

    (void)et_ringbuf_init(&g_rb, g_storage, sizeof(g_storage));
    ET_CHECK_U32_EQ(5u, et_ringbuf_write(&g_rb, in, sizeof(in)));
    ET_CHECK_U32_EQ(5u, et_ringbuf_used(&g_rb));
    memset(out, 0, sizeof(out));
    ET_CHECK_U32_EQ(5u, et_ringbuf_read(&g_rb, out, sizeof(out)));
    ET_CHECK(memcmp(in, out, 5u) == 0);
    ET_CHECK(et_ringbuf_is_empty(&g_rb));
}

static void rb_write_until_full(void)
{
    uint8_t big[10];
    uint8_t i;

    for (i = 0; i < 10u; i++) {
        big[i] = i;
    }
    (void)et_ringbuf_init(&g_rb, g_storage, 8u);
    ET_CHECK_U32_EQ(8u, et_ringbuf_write(&g_rb, big, sizeof(big))); /* 部分写入 */
    ET_CHECK(et_ringbuf_is_full(&g_rb));
    ET_CHECK_U32_EQ(0u, et_ringbuf_free_space(&g_rb));
}

static void rb_read_empty_returns_zero(void)
{
    uint8_t out[4] = {9u, 9u, 9u, 9u};

    (void)et_ringbuf_init(&g_rb, g_storage, sizeof(g_storage));
    ET_CHECK_U32_EQ(0u, et_ringbuf_read(&g_rb, out, 4u));
    ET_CHECK_U32_EQ(0u, et_ringbuf_peek(&g_rb, out, 4u));
    ET_CHECK_U32_EQ(9u, out[0]);                    /* 空读不得改写输出 */
    et_ringbuf_drop(&g_rb, 3u);                     /* 空丢弃应无副作用 */
    ET_CHECK_U32_EQ(0u, et_ringbuf_used(&g_rb));
}

static void rb_peek_no_consume(void)
{
    const char *s = "ABCD";
    char        out[8];

    (void)et_ringbuf_init(&g_rb, g_storage, 8u);
    (void)et_ringbuf_write(&g_rb, s, 4u);
    memset(out, 0, sizeof(out));
    ET_CHECK_U32_EQ(2u, et_ringbuf_peek(&g_rb, out, 2u));
    ET_CHECK(out[0] == 'A' && out[1] == 'B');
    ET_CHECK_U32_EQ(4u, et_ringbuf_used(&g_rb));    /* peek 不消费 */
    ET_CHECK_U32_EQ(4u, et_ringbuf_peek(&g_rb, out, 100u)); /* 上限截断 */
    ET_CHECK(strncmp(out, "ABCD", 4) == 0);
}

static void rb_drop_basic(void)
{
    const char *s = "ABCDEF";
    char        out[4];

    (void)et_ringbuf_init(&g_rb, g_storage, 8u);
    (void)et_ringbuf_write(&g_rb, s, 6u);
    et_ringbuf_drop(&g_rb, 2u);
    ET_CHECK_U32_EQ(4u, et_ringbuf_used(&g_rb));
    ET_CHECK_U32_EQ(4u, et_ringbuf_read(&g_rb, out, 4u));
    ET_CHECK(strncmp(out, "CDEF", 4) == 0);
}

/* 反复小规模读写跨越回绕点, 校验数据流完整性(容量取非 2 的幂以测试 % 路径) */
static void rb_wraparound_stream(void)
{
    uint8_t  storage[13];
    uint8_t  buf[20];
    uint8_t  produced = 0u;
    uint8_t  consumed = 0u;
    uint32_t total_in = 0u;
    uint32_t total_out = 0u;
    uint32_t round;

    (void)et_ringbuf_init(&g_rb, storage, sizeof(storage));
    for (round = 0u; round < 5000u; round++) {
        uint32_t k;
        uint32_t n = (uint32_t)(round % 7u);        /* 每轮写入 0~6 字节 */

        for (k = 0u; k < n; k++) {
            uint8_t one = produced++;

            ET_CHECK_U32_EQ(1u, et_ringbuf_write(&g_rb, &one, 1u));
            total_in++;
        }
        while (!et_ringbuf_is_empty(&g_rb)) {
            uint8_t one = 0u;

            ET_CHECK_U32_EQ(1u, et_ringbuf_read(&g_rb, &one, 1u));
            ET_CHECK_U32_EQ(consumed, one);         /* 读到的必须与写入顺序一致 */
            consumed++;
            total_out++;
        }
    }
    ET_CHECK_U32_EQ(total_in, total_out);
    ET_CHECK(total_out > 10000u);                   /* 足够多次跨回绕 */
    (void)buf;
}

/* 回归: 多字节拷贝跨越回绕点(曾因剩余段长度计算错误而写坏数据) */
static void rb_multibyte_across_wrap(void)
{
    uint8_t out[16];

    /* ---- 写路径: 在物理位置 6 处写入 6 字节, 必然跨回绕 ---- */
    (void)et_ringbuf_init(&g_rb, g_storage, 8u);
    (void)et_ringbuf_write(&g_rb, "abcdef", 6u);
    et_ringbuf_drop(&g_rb, 4u);                 /* head=6, tail=4 */
    ET_CHECK_U32_EQ(6u, et_ringbuf_write(&g_rb, "ghijkl", 6u));
    {
        char expect[8];

        memcpy(expect, "efghijkl", 8u);
        memset(out, 0, sizeof(out));
        ET_CHECK_U32_EQ(8u, et_ringbuf_peek(&g_rb, out, 8u));
        ET_CHECK(memcmp(out, "efghijkl", 8u) == 0);
    }

    /* ---- 读路径: 跨回绕多字节读出 ---- */
    memset(out, 0, sizeof(out));
    ET_CHECK_U32_EQ(5u, et_ringbuf_read(&g_rb, out, 5u));   /* efghi */
    ET_CHECK(memcmp(out, "efghi", 5u) == 0);

    /* ---- 多字节 peek 同样覆盖两段拼接 ---- */
    memset(out, 0, sizeof(out));
    ET_CHECK_U32_EQ(3u, et_ringbuf_peek(&g_rb, out, 3u));   /* jkl */
    ET_CHECK(memcmp(out, "jkl", 3u) == 0);
}

/* 回归: 随机长度的读写块交错, 大量跨越回绕(块粒度 > 1 字节) */
static void rb_block_stress(void)
{
    uint8_t  storage[13];
    uint32_t seed = 777u;
    uint8_t  produced = 0u;
    uint8_t  consumed = 0u;
    int      iter;

    (void)et_ringbuf_init(&g_rb, storage, sizeof(storage));
#define LCG() (seed = seed * 1664525u + 1013904223u)
    for (iter = 0; iter < 40000; iter++) {
        uint8_t  buf[24];
        uint32_t want = (LCG() % 10u) + 1u;     /* 块长 1~10 字节 */
        uint32_t got;
        uint32_t i;

        if ((LCG() & 1u) != 0u) {               /* 生产一块 */
            for (i = 0u; i < want; i++) {
                buf[i] = produced + (uint8_t)i; /* 待写入的连续序列 */
            }
            got = et_ringbuf_write(&g_rb, buf, want);
            produced += (uint8_t)got;           /* 只推进实际写入量 */
        } else {                                /* 消费一块 */
            got = et_ringbuf_read(&g_rb, buf, want);
            for (i = 0u; i < got; i++) {
                ET_CHECK_U32_EQ(consumed, buf[i]);
                consumed++;
            }
        }
    }
#undef LCG
    while (!et_ringbuf_is_empty(&g_rb)) {
        uint8_t one = 0u;

        ET_CHECK_U32_EQ(1u, et_ringbuf_read(&g_rb, &one, 1u));
        ET_CHECK_U32_EQ(consumed, one);
        consumed++;
    }
    ET_CHECK_U32_EQ(produced, consumed);
}

/* 白盒: 用 reserve/commit 构造回绕布局后验证连续段切分 */
static void rb_contig_across_wrap(void)
{
    uint32_t       got = 0xFFFFFFFFu;
    uint8_t       *pw;
    const uint8_t *p;

    (void)et_ringbuf_init(&g_rb, g_storage, 8u);
    (void)et_ringbuf_write(&g_rb, "abcdef", 6u);
    et_ringbuf_drop(&g_rb, 4u);

    /* 写指针位于物理位置 6, 只剩 2 字节到末尾 */
    pw = et_ringbuf_write_reserve(&g_rb, 6u, &got);
    ET_CHECK(pw == &g_storage[6]);
    ET_CHECK_U32_EQ(2u, got);
    pw[0] = 'g';
    pw[1] = 'h';
    et_ringbuf_write_commit(&g_rb, 2u);

    /* 回绕后从物理头部继续写 */
    pw = et_ringbuf_write_reserve(&g_rb, 6u, &got);
    ET_CHECK(pw == &g_storage[0]);
    ET_CHECK_U32_EQ(4u, got);
    pw[0] = 'i'; pw[1] = 'j'; pw[2] = 'k'; pw[3] = 'l';
    et_ringbuf_write_commit(&g_rb, 4u);

    ET_CHECK(et_ringbuf_is_full(&g_rb));
    {
        char out[9];

        ET_CHECK_U32_EQ(8u, et_ringbuf_peek(&g_rb, out, 8u));
        ET_CHECK(strncmp(out, "efghijkl", 8) == 0);
    }

    /* 读侧连续段同样在末尾截断 */
    p = et_ringbuf_read_peek(&g_rb, 8u, &got);
    ET_CHECK(p == &g_storage[4]);
    ET_CHECK_U32_EQ(4u, got);
    ET_CHECK(p[0] == 'e' && p[1] == 'f' && p[2] == 'g' && p[3] == 'h');
    et_ringbuf_drop(&g_rb, got);

    p = et_ringbuf_read_peek(&g_rb, 8u, &got);
    ET_CHECK(p == &g_storage[0]);
    ET_CHECK_U32_EQ(4u, got);
    ET_CHECK(p[0] == 'i' && p[3] == 'l');
    et_ringbuf_drop(&g_rb, got);
    ET_CHECK(et_ringbuf_is_empty(&g_rb));
}

static void rb_reserve_commit_semantics(void)
{
    uint32_t got = 0u;
    uint8_t *p;

    (void)et_ringbuf_init(&g_rb, g_storage, 8u);
    p = et_ringbuf_write_reserve(&g_rb, 4u, &got);
    ET_CHECK(p != NULL);
    ET_CHECK_U32_EQ(4u, got);

    /* commit 前读者不可见 */
    ET_CHECK(et_ringbuf_is_empty(&g_rb));
    p[0] = 0x11u; p[1] = 0x22u;
    et_ringbuf_write_commit(&g_rb, 2u);             /* 允许只发布部分 */
    ET_CHECK_U32_EQ(2u, et_ringbuf_used(&g_rb));

    /* 腾满后再 reserve 应返回 NULL */
    (void)et_ringbuf_write(&g_rb, "\xAA\xAA\xAA\xAA\xAA\xAA", 6u);
    ET_CHECK(et_ringbuf_is_full(&g_rb));
    p = et_ringbuf_write_reserve(&g_rb, 4u, &got);
    ET_CHECK(p == NULL);
    ET_CHECK_U32_EQ(0u, got);
}

static void rb_zero_len_ops(void)
{
    uint8_t x = 0xEEu;

    (void)et_ringbuf_init(&g_rb, g_storage, 8u);
    ET_CHECK_U32_EQ(0u, et_ringbuf_write(&g_rb, &x, 0u));
    ET_CHECK_U32_EQ(0u, et_ringbuf_read(&g_rb, &x, 0u));
    ET_CHECK(et_ringbuf_is_empty(&g_rb));
    ET_CHECK_U32_EQ(0xEEu, x);
}

/* SPSC 交错压测: 生产/消费随机交错, 校验字节流一致(容量非 2 的幂) */
static void rb_spsc_stress(void)
{
    uint8_t  storage[13];
    uint32_t seed = 12345u;
    uint8_t  produced = 0u;
    uint8_t  consumed = 0u;
    int      iter;

    (void)et_ringbuf_init(&g_rb, storage, sizeof(storage));
#define LCG_NEXT() (seed = seed * 1664525u + 1013904223u)
    for (iter = 0; iter < 20000; iter++) {
        if ((LCG_NEXT() & 3u) != 0u) {              /* 75% 概率生产 */
            uint32_t k;

            for (k = 0u; k <= (LCG_NEXT() % 4u); k++) {
                uint8_t one = produced++;

                if (et_ringbuf_write(&g_rb, &one, 1u) != 1u) {
                    produced--;                     /* 满: 回滚该字节 */
                    break;
                }
            }
        } else {                                    /* 25% 概率消费 */
            uint32_t k;

            for (k = 0u; k <= (LCG_NEXT() % 6u); k++) {
                uint8_t one = 0u;

                if (et_ringbuf_read(&g_rb, &one, 1u) == 1u) {
                    ET_CHECK_U32_EQ(consumed, one);
                    consumed++;
                } else {
                    break;
                }
            }
        }
    }
#undef LCG_NEXT
    /* 收尾: 排空并核对剩余流 */
    while (!et_ringbuf_is_empty(&g_rb)) {
        uint8_t one = 0u;

        ET_CHECK_U32_EQ(1u, et_ringbuf_read(&g_rb, &one, 1u));
        ET_CHECK_U32_EQ(consumed, one);
        consumed++;
    }
    ET_CHECK_U32_EQ(produced, consumed);
    ET_CHECK((produced - consumed) == 0u);
}

/* 白盒: 直接设置 head/tail 至 uint32 即将回绕处, 验证无符号减法数学正确性 */
static void rb_u32_index_wrap(void)
{
    (void)et_ringbuf_init(&g_rb, g_storage, 16u);
    g_rb.tail = 0xFFFFFFF0u;                        /* mod 16 == 0  */
    g_rb.head = g_rb.tail + 3u;                     /* used == 3    */
    g_storage[0] = 0x11u;
    g_storage[1] = 0x22u;
    g_storage[2] = 0x33u;

    ET_CHECK_U32_EQ(3u, et_ringbuf_used(&g_rb));
    ET_CHECK_U32_EQ(13u, et_ringbuf_free_space(&g_rb));

    {
        uint8_t out[4];

        ET_CHECK_U32_EQ(3u, et_ringbuf_read(&g_rb, out, 4u));
        ET_CHECK(out[0] == 0x11u && out[1] == 0x22u && out[2] == 0x33u);
    }

    /* head 位于 0xFFFFFFF3, 写入将跨越 uint32 回绕 */
    {
        const uint8_t *w = (const uint8_t *)"XYZ";

        ET_CHECK_U32_EQ(3u, et_ringbuf_write(&g_rb, w, 3u));
        ET_CHECK_U32_EQ(3u, et_ringbuf_used(&g_rb));
    }
    {
        uint8_t out[3] = {0u, 0u, 0u};

        ET_CHECK_U32_EQ(3u, et_ringbuf_peek(&g_rb, out, 3u));
        ET_CHECK(out[0] == 'X' && out[1] == 'Y' && out[2] == 'Z');
    }
}

const et_test_case_t *test_ringbuf_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"ringbuf.init_state",           rb_init_state},
        {"ringbuf.init_rejects_bad",     rb_init_rejects_bad},
        {"ringbuf.write_read_basic",     rb_write_read_basic},
        {"ringbuf.write_until_full",     rb_write_until_full},
        {"ringbuf.read_empty_returns_zero", rb_read_empty_returns_zero},
        {"ringbuf.peek_no_consume",      rb_peek_no_consume},
        {"ringbuf.drop_basic",           rb_drop_basic},
        {"ringbuf.wraparound_stream",    rb_wraparound_stream},
        {"ringbuf.multibyte_wrap",       rb_multibyte_across_wrap},
        {"ringbuf.block_stress",         rb_block_stress},
        {"ringbuf.contig_across_wrap",   rb_contig_across_wrap},
        {"ringbuf.reserve_commit",       rb_reserve_commit_semantics},
        {"ringbuf.zero_len_ops",         rb_zero_len_ops},
        {"ringbuf.spsc_stress",          rb_spsc_stress},
        {"ringbuf.u32_index_wrap",       rb_u32_index_wrap},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

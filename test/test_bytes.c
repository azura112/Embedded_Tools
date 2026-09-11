/**
 * @file    test_bytes.c
 * @brief   et_bytes 单元测试 (字节序向量 + 边界检查)
 */
#include "et_test.h"
#include "et_bytes.h"

static uint8_t g_buf[16];

static void bytes_roundtrip_u16(void)
{
    uint16_t v = 0u;

    ET_CHECK(et_bytes_be16_put(g_buf, sizeof(g_buf), 3u, 0x1234u));
    ET_CHECK(et_bytes_be16_get(g_buf, sizeof(g_buf), 3u, &v));
    ET_CHECK_U32_EQ(0x1234u, v);

    ET_CHECK(et_bytes_le16_put(g_buf, sizeof(g_buf), 5u, 0xABCDu));
    ET_CHECK(et_bytes_le16_get(g_buf, sizeof(g_buf), 5u, &v));
    ET_CHECK_U32_EQ(0xABCDu, v);
}

static void bytes_roundtrip_u32(void)
{
    uint32_t v = 0u;

    ET_CHECK(et_bytes_be32_put(g_buf, sizeof(g_buf), 0u, 0xDEADBEEFu));
    ET_CHECK(et_bytes_be32_get(g_buf, sizeof(g_buf), 0u, &v));
    ET_CHECK_U32_EQ(0xDEADBEEFu, v);

    ET_CHECK(et_bytes_le32_put(g_buf, sizeof(g_buf), 8u, 0x01020304u));
    ET_CHECK(et_bytes_le32_get(g_buf, sizeof(g_buf), 8u, &v));
    ET_CHECK_U32_EQ(0x01020304u, v);
}

static void bytes_byte_order_vectors(void)
{
    uint16_t v16 = 0u;
    uint32_t v32 = 0u;

    ET_CHECK(et_bytes_be16_put(g_buf, sizeof(g_buf), 0u, 0x1234u));
    ET_CHECK_U32_EQ(0x12u, g_buf[0]);
    ET_CHECK_U32_EQ(0x34u, g_buf[1]);

    ET_CHECK(et_bytes_le16_put(g_buf, sizeof(g_buf), 0u, 0x1234u));
    ET_CHECK_U32_EQ(0x34u, g_buf[0]);
    ET_CHECK_U32_EQ(0x12u, g_buf[1]);

    ET_CHECK(et_bytes_be32_put(g_buf, sizeof(g_buf), 0u, 0xDEADBEEFu));
    ET_CHECK_U32_EQ(0xDEu, g_buf[0]);
    ET_CHECK_U32_EQ(0xADu, g_buf[1]);
    ET_CHECK_U32_EQ(0xBEu, g_buf[2]);
    ET_CHECK_U32_EQ(0xEFu, g_buf[3]);

    ET_CHECK(et_bytes_le32_put(g_buf, sizeof(g_buf), 0u, 0xDEADBEEFu));
    ET_CHECK_U32_EQ(0xEFu, g_buf[0]);
    ET_CHECK_U32_EQ(0xBEu, g_buf[1]);
    ET_CHECK_U32_EQ(0xADu, g_buf[2]);
    ET_CHECK_U32_EQ(0xDEu, g_buf[3]);

    /* 由已知字节读回 */
    g_buf[0] = 0x12u; g_buf[1] = 0x34u;
    ET_CHECK(et_bytes_be16_get(g_buf, sizeof(g_buf), 0u, &v16));
    ET_CHECK_U32_EQ(0x1234u, v16);
    ET_CHECK(et_bytes_le16_get(g_buf, sizeof(g_buf), 0u, &v16));
    ET_CHECK_U32_EQ(0x3412u, v16);

    g_buf[0] = 0x01u; g_buf[1] = 0x02u; g_buf[2] = 0x03u; g_buf[3] = 0x04u;
    ET_CHECK(et_bytes_be32_get(g_buf, sizeof(g_buf), 0u, &v32));
    ET_CHECK_U32_EQ(0x01020304u, v32);
    ET_CHECK(et_bytes_le32_get(g_buf, sizeof(g_buf), 0u, &v32));
    ET_CHECK_U32_EQ(0x04030201u, v32);
}

static void bytes_bounds_read(void)
{
    uint16_t v16 = 0xAAAAu;
    uint32_t v32 = 0xAAAAAAAAu;

    /* len 不足 */
    ET_CHECK(!et_bytes_be16_get(g_buf, 1u, 0u, &v16));
    ET_CHECK(!et_bytes_be32_get(g_buf, 3u, 0u, &v32));
    /* off 越界 */
    ET_CHECK(!et_bytes_be16_get(g_buf, 4u, 4u, &v16));
    ET_CHECK(!et_bytes_le32_get(g_buf, 4u, 1u, &v32));
    /* off 回绕 (off+size 会溢出 uint32) */
    ET_CHECK(!et_bytes_be32_get(g_buf, 16u, UINT32_MAX - 1u, &v32));
    /* 失败不写 *out */
    ET_CHECK_U32_EQ(0xAAAAu, (uint32_t)v16);
    ET_CHECK_U32_EQ(0xAAAAAAAAu, (uint32_t)v32);
    /* NULL */
    ET_CHECK(!et_bytes_be16_get(NULL, 4u, 0u, &v16));
    ET_CHECK(!et_bytes_be16_get(g_buf, 4u, 0u, NULL));
}

static void bytes_bounds_write(void)
{
    uint32_t i;

    for (i = 0u; i < sizeof(g_buf); i++) {
        g_buf[i] = 0x5Au;
    }
    ET_CHECK(!et_bytes_be16_put(g_buf, 1u, 0u, 0xFFFFu));
    ET_CHECK(!et_bytes_le32_put(g_buf, 4u, 1u, 0xFFFFFFFFu));
    ET_CHECK(!et_bytes_be32_put(g_buf, 16u, UINT32_MAX - 1u, 0xFFFFFFFFu));
    ET_CHECK(!et_bytes_le16_put(NULL, 4u, 0u, 0xFFFFu));
    /* 失败不写 buf 一个字节 */
    for (i = 0u; i < sizeof(g_buf); i++) {
        ET_CHECK_U32_EQ(0x5Au, g_buf[i]);
    }
}

static void bytes_zero_len(void)
{
    uint16_t v16 = 0u;

    ET_CHECK(!et_bytes_be16_get(g_buf, 0u, 0u, &v16));
    ET_CHECK(!et_bytes_le32_put(g_buf, 0u, 0u, 1u));
}

static void bytes_offset_edges(void)
{
    uint32_t v = 0u;
    uint16_t t = 0u;

    /* 恰好放下: off=len-4 */
    ET_CHECK(et_bytes_be32_put(g_buf, 4u, 0u, 0x11223344u));
    ET_CHECK(et_bytes_be32_get(g_buf, 4u, 0u, &v));
    ET_CHECK_U32_EQ(0x11223344u, v);
    /* off=len 起始必失败; off=len-2 对 u16 成功 */
    ET_CHECK(!et_bytes_be16_get(g_buf, 4u, 4u, &t));
    ET_CHECK(et_bytes_be16_get(g_buf, 4u, 2u, &t));
    ET_CHECK_U32_EQ(0x3344u, t);
}

static void bytes_mixed_at_offsets(void)
{
    uint16_t a = 0u;
    uint32_t b = 0u;
    uint16_t c = 0u;

    ET_CHECK(et_bytes_be16_put(g_buf, sizeof(g_buf), 0u, 0x0102u));
    ET_CHECK(et_bytes_le32_put(g_buf, sizeof(g_buf), 2u, 0xAABBCCDDu));
    ET_CHECK(et_bytes_be16_put(g_buf, sizeof(g_buf), 6u, 0x0F10u));
    ET_CHECK(et_bytes_be16_get(g_buf, sizeof(g_buf), 0u, &a));
    ET_CHECK(et_bytes_le32_get(g_buf, sizeof(g_buf), 2u, &b));
    ET_CHECK(et_bytes_be16_get(g_buf, sizeof(g_buf), 6u, &c));
    ET_CHECK_U32_EQ(0x0102u, a);
    ET_CHECK_U32_EQ(0xAABBCCDDu, b);
    ET_CHECK_U32_EQ(0x0F10u, c);
}

const et_test_case_t *test_bytes_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"bytes.roundtrip_u16",   bytes_roundtrip_u16},
        {"bytes.roundtrip_u32",   bytes_roundtrip_u32},
        {"bytes.order_vectors",   bytes_byte_order_vectors},
        {"bytes.bounds_read",     bytes_bounds_read},
        {"bytes.bounds_write",    bytes_bounds_write},
        {"bytes.zero_len",        bytes_zero_len},
        {"bytes.offset_edges",    bytes_offset_edges},
        {"bytes.mixed_offsets",   bytes_mixed_at_offsets},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

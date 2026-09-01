/**
 * @file    test_crc.c
 * @brief   et_crc 单元测试 (标准校验向量 + 流式一致性)
 */
#include "et_test.h"
#include "et_crc.h"
#include <string.h>

#define CHECK_STR "123456789"

static void crc_standard_vectors(void)
{
    ET_CHECK_U32_EQ(0xF4u,        et_crc8(CHECK_STR, 9u));
    ET_CHECK_U32_EQ(0x4B37u,      et_crc16_modbus(CHECK_STR, 9u));
    ET_CHECK_U32_EQ(0x29B1u,      et_crc16_ccitt(CHECK_STR, 9u));
    ET_CHECK_U32_EQ(0xCBF43926u,  et_crc32(CHECK_STR, 9u));
}

static void crc_empty_and_single(void)
{
    uint8_t zero = 0u;

    ET_CHECK_U32_EQ(0x00u,       et_crc8(NULL, 0u));
    ET_CHECK_U32_EQ(0xFFFFu,     et_crc16_modbus(NULL, 0u));
    ET_CHECK_U32_EQ(0xFFFFu,     et_crc16_ccitt(NULL, 0u));
    ET_CHECK_U32_EQ(0x00000000u, et_crc32(NULL, 0u));

    ET_CHECK_U32_EQ(0xD202EF8Du, et_crc32(&zero, 1u)); /* 已知单字节向量 */
}

static void crc_streaming_equivalence(void)
{
    uint8_t  buf[257];
    uint32_t i;
    uint32_t seed = 99u;

    for (i = 0u; i < sizeof(buf); i++) {
        seed   = seed * 1664525u + 1013904223u;
        buf[i] = (uint8_t)(seed >> 24);
    }

    /* 各种切分方式下流式结果必须等于一次性计算 */
    {
        const uint32_t splits[] = { 1u, 2u, 7u, 64u, 256u };
        size_t s;

        for (s = 0; s < sizeof(splits) / sizeof(splits[0]); s++) {
            uint32_t step = splits[s];
            uint8_t  c8   = ET_CRC8_INIT;
            uint16_t m16  = ET_CRC16_MODBUS_INIT;
            uint16_t c16  = ET_CRC16_CCITT_INIT;
            uint32_t c32  = ET_CRC32_INIT;
            uint32_t off;

            for (off = 0u; off < sizeof(buf); off += step) {
                uint32_t n = ((off + step) <= sizeof(buf)) ?
                             step : (uint32_t)(sizeof(buf) - off);

                c8  = et_crc8_update(c8,  &buf[off], n);
                m16 = et_crc16_modbus_update(m16, &buf[off], n);
                c16 = et_crc16_ccitt_update(c16, &buf[off], n);
                c32 = et_crc32_update(c32, &buf[off], n);
            }
            ET_CHECK_U32_EQ(et_crc8(buf, sizeof(buf)), c8);
            ET_CHECK_U32_EQ(et_crc16_modbus(buf, sizeof(buf)), m16);
            ET_CHECK_U32_EQ(et_crc16_ccitt(buf, sizeof(buf)), c16);
            ET_CHECK_U32_EQ(et_crc32(buf, sizeof(buf)),
                            c32 ^ 0xFFFFFFFFu);
        }
    }
}

static void crc_null_data_safe(void)
{
    /* len=0 或 data=NULL 时应原样返回种子 */
    ET_CHECK_U32_EQ(0xABu, et_crc8_update(0xABu, NULL, 100u));
    ET_CHECK_U32_EQ(0x1234u, et_crc16_modbus_update(0x1234u, NULL, 100u));
}

const et_test_case_t *test_crc_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"crc.standard_vectors", crc_standard_vectors},
        {"crc.empty_and_single", crc_empty_and_single},
        {"crc.streaming_equiv",  crc_streaming_equivalence},
        {"crc.null_safety",      crc_null_data_safe},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

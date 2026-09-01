/**
 * @file    test_frame.c
 * @brief   et_frame 单元测试: 收发回环 / 抗噪 / 容错恢复
 */
#include "et_test.h"
#include "et_frame.h"
#include <string.h>

static uint8_t  g_rxbuf[64];
static uint16_t g_last_len;
static uint32_t g_cb_calls;
static uint8_t  g_last_payload[64];
static void    *g_last_user;

static void capture_cb(struct et_frame_parser *p, uint16_t len, void *user)
{
    g_cb_calls++;
    g_last_len = len;
    g_last_user = user;
    if (len <= sizeof(g_last_payload)) {
        (void)memcpy(g_last_payload, p->rx_buf, len);
    }
}

static void frame_reset_capture(void)
{
    g_cb_calls = 0u;
    g_last_len = 0u;
    g_last_user = NULL;
    (void)memset(g_last_payload, 0, sizeof(g_last_payload));
}

static void fr_roundtrip_all_crc_types(void)
{
    static const et_frame_crc_t types[] = {
        ET_FRAME_CRC_NONE,
        ET_FRAME_CRC_XOR,
        ET_FRAME_CRC_SUM8,
        ET_FRAME_CRC_CRC8,
        ET_FRAME_CRC_CRC16_MODBUS,
        ET_FRAME_CRC_CCITT,
    };
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    const char          *payload = "hello";
    size_t               t;

    for (t = 0; t < sizeof(types) / sizeof(types[0]); t++) {
        et_frame_cfg_t     cfg;
        et_frame_parser_t  parser;
        uint8_t            tx[80];
        uint16_t           n;

        (void)memset(&cfg, 0, sizeof(cfg));
        cfg.header     = hdr;
        cfg.header_len = 2u;
        cfg.use_len    = true;
        cfg.crc        = types[t];
        cfg.rx_buf     = g_rxbuf;
        cfg.rx_cap     = sizeof(g_rxbuf);
        cfg.on_frame   = capture_cb;

        frame_reset_capture();
        ET_CHECK(et_frame_parser_init(&parser, &cfg));

        n = et_frame_pack(&cfg, (const uint8_t *)payload, 5u, tx, sizeof(tx));
        ET_CHECK(n != 0u);                          /* 每种配置都能组帧 */
        ET_CHECK_U32_EQ(1u, et_frame_write(&parser, tx, n));

        ET_CHECK_U32_EQ(1u, g_cb_calls);
        ET_CHECK_U32_EQ(5u, g_last_len);
        ET_CHECK(memcmp(g_last_payload, payload, 5u) == 0);
        ET_CHECK_U32_EQ(1u, parser.frame_count);
        ET_CHECK_U32_EQ(0u, parser.err_count);
    }
}

static void fr_roundtrip_with_tail(void)
{
    static const uint8_t hdr[1] = { 0x7Eu };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;
    uint8_t              tx[40];

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 1u;
    cfg.use_len = true;
    cfg.crc = ET_FRAME_CRC_CRC16_MODBUS;
    cfg.use_tail = true; cfg.tail = 0x0Du;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = sizeof(g_rxbuf);
    cfg.on_frame = capture_cb;
    frame_reset_capture();

    ET_CHECK(et_frame_parser_init(&parser, &cfg));
    {
        uint16_t n = et_frame_pack(&cfg, (const uint8_t *)"OK", 2u,
                                   tx, sizeof(tx));

        ET_CHECK(n != 0u);
        ET_CHECK_U32_EQ(1u, et_frame_write(&parser, tx, n));
        ET_CHECK_U32_EQ(1u, g_cb_calls);
        ET_CHECK(memcmp(g_last_payload, "OK", 2u) == 0);
    }
}

static void fr_fixed_len_mode(void)
{
    static const uint8_t hdr[2] = { 0xC1u, 0xC2u };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;
    uint8_t              tx[32];

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 2u;
    cfg.use_len = false; cfg.fixed_len = 4u;
    cfg.crc = ET_FRAME_CRC_CRC8;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = sizeof(g_rxbuf);
    cfg.on_frame = capture_cb;
    frame_reset_capture();

    ET_CHECK(et_frame_parser_init(&parser, &cfg));

    /* 定长模式下长度不符的组帧被拒绝 */
    ET_CHECK_U32_EQ(0u, et_frame_pack(&cfg, (const uint8_t *)"abc", 3u, tx, sizeof(tx)));
    {
        uint16_t n = et_frame_pack(&cfg, (const uint8_t *)"wxyz", 4u,
                                   tx, sizeof(tx));

        ET_CHECK(n != 0u);
        ET_CHECK_U32_EQ(1u, et_frame_write(&parser, tx, n));
        ET_CHECK(memcmp(g_last_payload, "wxyz", 4u) == 0);
    }
}

static void fr_noise_then_sync(void)
{
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;
    uint8_t              tx[40];
    uint16_t             n;
    /* 噪声流: 含"半个帧头"与假帧头组合 */
    static const uint8_t noise[] =
        { 0x01u, 0xAAu, 0x02u, 0xAAu, 0xAAu, 0x00u, 0x13u };

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 2u;
    cfg.use_len = true; cfg.crc = ET_FRAME_CRC_SUM8;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = sizeof(g_rxbuf);
    cfg.on_frame = capture_cb;
    frame_reset_capture();
    ET_CHECK(et_frame_parser_init(&parser, &cfg));

    n = et_frame_pack(&cfg, (const uint8_t *)"Z", 1u, tx, sizeof(tx));
    ET_CHECK(n != 0u);

    /* 噪声 + 帧头前缀交错 + 正式帧, 应正确同步且不误触发 */
    {
        uint8_t stream[64];
        uint8_t k = 0u;
        uint16_t i;

        (void)memcpy(stream, noise, sizeof(noise));
        k = (uint8_t)sizeof(noise);
        stream[k++] = 0xAAu;                    /* 又一个孤立前缀 */
        for (i = 0u; i < n; i++) {
            stream[k++] = tx[i];
        }
        ET_CHECK_U32_EQ(1u, et_frame_write(&parser, stream, k));
        ET_CHECK_U32_EQ(1u, g_cb_calls);
        ET_CHECK(g_last_payload[0] == 'Z');
        ET_CHECK_U32_EQ(0u, parser.err_count);  /* 纯噪声不算错误 */
    }
}

static void fr_bad_crc_rejects_and_recovers(void)
{
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;
    uint8_t              bad[40];
    uint16_t             nb;
    uint8_t              good[40];
    uint16_t             ng;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 2u;
    cfg.use_len = true; cfg.crc = ET_FRAME_CRC_CRC8;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = sizeof(g_rxbuf);
    cfg.on_frame = capture_cb;
    frame_reset_capture();
    ET_CHECK(et_frame_parser_init(&parser, &cfg));

    nb = et_frame_pack(&cfg, (const uint8_t *)"DATA", 4u, bad, sizeof(bad));
    ng = et_frame_pack(&cfg, (const uint8_t *)"OK!", 3u, good, sizeof(good));
    ET_CHECK((nb != 0u) && (ng != 0u));

    bad[nb - 1u] ^= 0xFFu;                      /* 破坏校验字节 */

    ET_CHECK_U32_EQ(0u, et_frame_write(&parser, bad, nb));
    ET_CHECK_U32_EQ(1u, parser.err_count);
    ET_CHECK_U32_EQ(0u, g_cb_calls);

    ET_CHECK_U32_EQ(1u, et_frame_write(&parser, good, ng)); /* 随后恢复同步 */
    ET_CHECK_U32_EQ(1u, g_cb_calls);
    ET_CHECK(memcmp(g_last_payload, "OK!", 3u) == 0);
}

static void fr_oversize_rejected(void)
{
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;
    uint8_t              big[40];
    uint8_t              ok[20];
    uint16_t             i;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 2u;
    cfg.use_len = true; cfg.crc = ET_FRAME_CRC_NONE;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = 8u;      /* 小容量缓冲 */
    cfg.on_frame = capture_cb;
    frame_reset_capture();
    ET_CHECK(et_frame_parser_init(&parser, &cfg));

    /* 手工构造 len=10 的超长帧 */
    big[0] = 0xAAu; big[1] = 0x55u; big[2] = 10u;
    for (i = 0u; i < 10u; i++) {
        big[3u + i] = 0x20u;
    }
    ET_CHECK_U32_EQ(0u, et_frame_write(&parser, big, 13u));
    ET_CHECK_U32_EQ(1u, parser.err_count);

    {
        uint16_t n = et_frame_pack(&cfg, (const uint8_t *)"tiny", 4u,
                                   ok, sizeof(ok));

        ET_CHECK(n != 0u);
        ET_CHECK_U32_EQ(1u, et_frame_write(&parser, ok, n));    /* 可恢复 */
        ET_CHECK_U32_EQ(1u, g_cb_calls);
    }
}

static void fr_tail_mismatch_rejected(void)
{
    static const uint8_t hdr[1] = { 0x7Eu };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;
    uint8_t              tx[40];
    uint16_t             n;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 1u;
    cfg.use_len = true; cfg.crc = ET_FRAME_CRC_NONE;
    cfg.use_tail = true; cfg.tail = 0x0Du;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = sizeof(g_rxbuf);
    cfg.on_frame = capture_cb;
    frame_reset_capture();
    ET_CHECK(et_frame_parser_init(&parser, &cfg));

    n = et_frame_pack(&cfg, (const uint8_t *)"A", 1u, tx, sizeof(tx));
    ET_CHECK(n != 0u);
    tx[n - 1u] = 0xEEu;                         /* 错误帧尾 */

    ET_CHECK_U32_EQ(0u, et_frame_write(&parser, tx, n));
    ET_CHECK_U32_EQ(1u, parser.err_count);
    ET_CHECK_U32_EQ(0u, g_cb_calls);
}

static void fr_zero_len_payload(void)
{
    static const uint8_t hdr[2] = { 0xA5u, 0x5Au };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;
    uint8_t              tx[16];
    uint16_t             n;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 2u;
    cfg.use_len = true; cfg.crc = ET_FRAME_CRC_NONE;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = sizeof(g_rxbuf);
    cfg.on_frame = capture_cb;
    frame_reset_capture();
    ET_CHECK(et_frame_parser_init(&parser, &cfg));

    n = et_frame_pack(&cfg, NULL, 0u, tx, sizeof(tx));
    ET_CHECK(n != 0u);                          /* 心跳/探活类空载荷帧 */
    ET_CHECK_U32_EQ(1u, et_frame_write(&parser, tx, n));
    ET_CHECK_U32_EQ(1u, g_cb_calls);
    ET_CHECK_U32_EQ(0u, g_last_len);
}

static void fr_multi_frames_one_write(void)
{
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;
    uint8_t              f1[20];
    uint8_t              f2[20];
    uint8_t              all[48];
    uint16_t             n1;
    uint16_t             n2;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 2u;
    cfg.use_len = true; cfg.crc = ET_FRAME_CRC_XOR;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = sizeof(g_rxbuf);
    cfg.on_frame = capture_cb;
    frame_reset_capture();
    ET_CHECK(et_frame_parser_init(&parser, &cfg));

    n1 = et_frame_pack(&cfg, (const uint8_t *)"AB", 2u, f1, sizeof(f1));
    n2 = et_frame_pack(&cfg, (const uint8_t *)"CDE", 3u, f2, sizeof(f2));
    ET_CHECK((n1 != 0u) && (n2 != 0u));
    (void)memcpy(all, f1, n1);
    (void)memcpy(&all[n1], f2, n2);

    ET_CHECK_U32_EQ(2u, et_frame_write(&parser, all, (uint16_t)(n1 + n2)));
    ET_CHECK_U32_EQ(2u, g_cb_calls);
}

static void fr_byte_by_byte_bulk_equal(void)
{
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    pa;
    et_frame_parser_t    pb;
    uint8_t              tx[40];
    uint16_t             n;
    uint16_t             i;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.header = hdr; cfg.header_len = 2u;
    cfg.use_len = true; cfg.crc = ET_FRAME_CRC_CRC16_MODBUS;
    cfg.rx_buf = g_rxbuf; cfg.rx_cap = sizeof(g_rxbuf);
    cfg.on_frame = capture_cb;
    ET_CHECK(et_frame_parser_init(&pa, &cfg));
    ET_CHECK(et_frame_parser_init(&pb, &cfg));

    n = et_frame_pack(&cfg, (const uint8_t *)"chunked", 7u, tx, sizeof(tx));
    ET_CHECK(n != 0u);

    frame_reset_capture();
    for (i = 0u; i < n; i++) {                  /* ISR 场景: 逐字节喂入 */
        if (et_frame_feed(&pa, tx[i])) {
            break;
        }
    }
    ET_CHECK_U32_EQ(1u, g_cb_calls);
    ET_CHECK(memcmp(g_last_payload, "chunked", 7u) == 0);

    frame_reset_capture();
    (void)et_frame_write(&pb, tx, n);           /* 批量喂入结果一致 */
    ET_CHECK_U32_EQ(1u, g_cb_calls);
    ET_CHECK(memcmp(g_last_payload, "chunked", 7u) == 0);
}

static void fr_init_rejects_bad(void)
{
    static const uint8_t hdr[2] = { 0xAAu, 0x55u };
    et_frame_cfg_t       cfg;
    et_frame_parser_t    parser;

    (void)memset(&cfg, 0, sizeof(cfg));
    ET_CHECK(!et_frame_parser_init(&parser, &cfg));         /* 无帧头 */

    cfg.header = hdr;
    ET_CHECK(!et_frame_parser_init(&parser, &cfg));         /* 帧头长度 0 */
    cfg.header_len = 5u;
    ET_CHECK(!et_frame_parser_init(&parser, &cfg));         /* 超过上限 4 */
    cfg.header_len = 2u;
    ET_CHECK(et_frame_parser_init(&parser, &cfg));
}

const et_test_case_t *test_frame_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"frame.roundtrip_all_crc",  fr_roundtrip_all_crc_types},
        {"frame.roundtrip_tail",     fr_roundtrip_with_tail},
        {"frame.fixed_len_mode",     fr_fixed_len_mode},
        {"frame.noise_then_sync",    fr_noise_then_sync},
        {"frame.bad_crc_recover",    fr_bad_crc_rejects_and_recovers},
        {"frame.oversize_recover",   fr_oversize_rejected},
        {"frame.tail_mismatch",      fr_tail_mismatch_rejected},
        {"frame.zero_len_payload",   fr_zero_len_payload},
        {"frame.multi_frames",       fr_multi_frames_one_write},
        {"frame.feed_modes_equal",   fr_byte_by_byte_bulk_equal},
        {"frame.init_rejects_bad",   fr_init_rejects_bad},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

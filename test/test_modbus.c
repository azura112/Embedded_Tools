/**
 * @file    test_modbus.c
 * @brief   et_modbus 单元测试 (脚本化主站帧 → 从站应答逐字节比对)
 *
 * 主站侧帧由本文件的 mk() 构造(CRC16-MODBUS, 线上低字节在前); 从站侧用
 * 16 保持寄存器 + 计算型输入寄存器(值 = idx*100+7)作映射, 钩子越界返回 0x02。
 */
#include <string.h>
#include "et_test.h"
#include "et_modbus.h"
#include "et_crc.h"

#define SLAVE       0x11u
#define SILENCE_MS  5u

static uint8_t   g_rx[64];
static uint8_t   g_tx[256];
static et_modbus_t g_mb;
static uint16_t  g_hold[16];

/* 边界用例用: 128 寄存器大区间从站 */
static uint8_t   g_rx2[256];
static uint8_t   g_tx2[256];
static uint16_t  g_big[128];

static uint16_t in_val(uint16_t idx)
{
    return (uint16_t)(idx * 100u + 7u);
}

static uint8_t rd_hook(void *user, uint8_t func, uint16_t addr, uint16_t qty,
                       uint8_t *dst)
{
    uint16_t i;

    (void)user;
    if (((uint32_t)addr + qty) > 16u) {
        return ET_MODBUS_EXC_ILLEGAL_ADDR;
    }
    for (i = 0u; i < qty; i++) {
        uint16_t v = (func == ET_MODBUS_FC_READ_HOLDING) ? g_hold[addr + i]
                                                         : in_val((uint16_t)(addr + i));

        dst[2u * i]      = (uint8_t)(v >> 8);
        dst[2u * i + 1u] = (uint8_t)(v & 0xFFu);
    }
    return 0u;
}

static uint8_t wr_hook(void *user, uint8_t func, uint16_t addr, uint16_t qty,
                       const uint8_t *src)
{
    uint16_t i;

    (void)user;
    (void)func;
    if (((uint32_t)addr + qty) > 16u) {
        return ET_MODBUS_EXC_ILLEGAL_ADDR;
    }
    for (i = 0u; i < qty; i++) {
        g_hold[addr + i] = (uint16_t)(((uint16_t)src[2u * i] << 8) |
                                      (uint16_t)src[2u * i + 1u]);
    }
    return 0u;
}

static uint8_t rd_big(void *user, uint8_t func, uint16_t addr, uint16_t qty,
                      uint8_t *dst)
{
    uint16_t i;

    (void)user;
    (void)func;
    if (((uint32_t)addr + qty) > 128u) {
        return ET_MODBUS_EXC_ILLEGAL_ADDR;
    }
    for (i = 0u; i < qty; i++) {
        dst[2u * i]      = (uint8_t)(g_big[addr + i] >> 8);
        dst[2u * i + 1u] = (uint8_t)(g_big[addr + i] & 0xFFu);
    }
    return 0u;
}

static uint8_t wr_big(void *user, uint8_t func, uint16_t addr, uint16_t qty,
                      const uint8_t *src)
{
    uint16_t i;

    (void)user;
    (void)func;
    if (((uint32_t)addr + qty) > 128u) {
        return ET_MODBUS_EXC_ILLEGAL_ADDR;
    }
    for (i = 0u; i < qty; i++) {
        g_big[addr + i] = (uint16_t)(((uint16_t)src[2u * i] << 8) |
                                     (uint16_t)src[2u * i + 1u]);
    }
    return 0u;
}

/* 主站侧组帧: [addr, func, payload..., crc_lo, crc_hi] */
static uint32_t mk(uint8_t *f, uint8_t addr, uint8_t func,
                   const uint8_t *payload, uint32_t plen)
{
    uint16_t crc;

    f[0] = addr;
    f[1] = func;
    if (plen > 0u) {
        memcpy(f + 2u, payload, plen);
    }
    crc = et_crc16_modbus(f, 2u + plen);
    f[2u + plen]      = (uint8_t)(crc & 0xFFu);     /* 低字节在前 */
    f[3u + plen]      = (uint8_t)(crc >> 8);
    return 4u + plen;
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static void setup(void)
{
    et_modbus_cfg_t cfg;
    uint32_t i;

    for (i = 0u; i < 16u; i++) {
        g_hold[i] = (uint16_t)(i + 1u);
    }
    memset(&g_mb, 0, sizeof(g_mb));
    memset(&cfg, 0, sizeof(cfg));
    cfg.slave_addr = SLAVE;
    cfg.silence_ms = SILENCE_MS;
    cfg.user       = NULL;
    cfg.rd         = rd_hook;
    cfg.wr         = wr_hook;
    ET_CHECK(et_modbus_init(&g_mb, &cfg, g_rx, sizeof(g_rx),
                            g_tx, sizeof(g_tx)));
    memset(g_rx, 0, sizeof(g_rx));
    memset(g_tx, 0, sizeof(g_tx));
}

/* 取应答并与期望逐字节比对; 无应答时 exp_len 传 0 */
static void expect_resp(const uint8_t *exp, uint32_t exp_len)
{
    uint32_t      len = 0u;
    const uint8_t *r  = et_modbus_response(&g_mb, &len);

    ET_CHECK_U32_EQ(exp_len, len);
    if (exp_len > 0u) {
        ET_CHECK(r != NULL);
        ET_CHECK(memcmp(r, exp, exp_len) == 0);
    } else {
        ET_CHECK(r == NULL);
    }
}

/* ---------- 初始化 ---------- */

static void mb_init_validation(void)
{
    et_modbus_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.slave_addr = SLAVE;
    cfg.silence_ms = SILENCE_MS;
    ET_CHECK(!et_modbus_init(NULL, &cfg, g_rx, sizeof(g_rx), g_tx, sizeof(g_tx)));
    ET_CHECK(!et_modbus_init(&g_mb, NULL, g_rx, sizeof(g_rx), g_tx, sizeof(g_tx)));
    ET_CHECK(!et_modbus_init(&g_mb, &cfg, NULL, sizeof(g_rx), g_tx, sizeof(g_tx)));
    ET_CHECK(!et_modbus_init(&g_mb, &cfg, g_rx, sizeof(g_rx), NULL, sizeof(g_tx)));
    cfg.slave_addr = 0u;                    /* 0 保留给广播 */
    ET_CHECK(!et_modbus_init(&g_mb, &cfg, g_rx, sizeof(g_rx), g_tx, sizeof(g_tx)));
    cfg.slave_addr = 248u;                  /* 248~255 保留 */
    ET_CHECK(!et_modbus_init(&g_mb, &cfg, g_rx, sizeof(g_rx), g_tx, sizeof(g_tx)));
    cfg.slave_addr = SLAVE;
    ET_CHECK(!et_modbus_init(&g_mb, &cfg, g_rx, 7u, g_tx, sizeof(g_tx)));
    ET_CHECK(!et_modbus_init(&g_mb, &cfg, g_rx, sizeof(g_rx), g_tx, 7u));
    cfg.rd = NULL; cfg.wr = NULL;           /* 钩子可空 */
    ET_CHECK(et_modbus_init(&g_mb, &cfg, g_rx, sizeof(g_rx), g_tx, sizeof(g_tx)));
    {
        uint32_t len = 99u;

        ET_CHECK(et_modbus_response(&g_mb, &len) == NULL);
        ET_CHECK_U32_EQ(0u, len);
    }
}

/* ---------- 正常流 ---------- */

static void mb_read_holding_normal(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    uint8_t d[16];                          /* 应答 13B: [0..12] —— 原 d[8] 越界写 d[9..12]
                                             * (v2.5 新增 ASan 门 make test-asan 抓到) */

    setup();
    put16(pay, 0u); put16(pay + 2u, 4u);
    ET_CHECK_U32_EQ(8u, mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u));
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req, 8u));

    /* 期望: [11,03,08, hold0..hold3, crc] */
    d[0] = SLAVE; d[1] = ET_MODBUS_FC_READ_HOLDING; d[2] = 8u;
    put16(d + 3u, 1u); put16(d + 5u, 2u);
    put16(d + 7u, 3u); put16(d + 9u, 4u);
    {
        uint16_t crc = et_crc16_modbus(d, 11u);

        d[11] = (uint8_t)(crc & 0xFFu);
        d[12] = (uint8_t)(crc >> 8);
    }
    expect_resp(d, 13u);
}

static void mb_read_input_normal(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    uint8_t d[16];

    setup();
    put16(pay, 4u); put16(pay + 2u, 2u);
    ET_CHECK_U32_EQ(8u, mk(req, SLAVE, ET_MODBUS_FC_READ_INPUT, pay, 4u));
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req, 8u));

    d[0] = SLAVE; d[1] = ET_MODBUS_FC_READ_INPUT; d[2] = 4u;
    put16(d + 3u, in_val(4u));
    put16(d + 5u, in_val(5u));
    {
        uint16_t crc = et_crc16_modbus(d, 7u);

        d[7] = (uint8_t)(crc & 0xFFu);
        d[8] = (uint8_t)(crc >> 8);
    }
    expect_resp(d, 9u);
}

static void mb_write_single_echo(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    uint8_t exp[8];

    setup();
    put16(pay, 2u); put16(pay + 2u, 0x1234u);
    ET_CHECK_U32_EQ(8u, mk(req, SLAVE, ET_MODBUS_FC_WRITE_SINGLE, pay, 4u));
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req, 8u));
    ET_CHECK_U32_EQ(0x1234u, g_hold[2]);
    memcpy(exp, req, 8u);                   /* 应答 = 请求回显 */
    expect_resp(exp, 8u);
}

static void mb_write_multiple_normal(void)
{
    uint8_t req[16];
    uint8_t pay[11];                        /* addr2 + qty2 + bc1 + 3*2 */
    uint8_t exp[8];

    setup();
    put16(pay, 1u); put16(pay + 2u, 3u); pay[4] = 6u;
    put16(pay + 5u, 0x0011u);
    put16(pay + 7u, 0x0022u);
    put16(pay + 9u, 0x0033u);
    ET_CHECK_U32_EQ(15u, mk(req, SLAVE, ET_MODBUS_FC_WRITE_MULTIPLE, pay, 11u));
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req, 15u));
    ET_CHECK_U32_EQ(0x0011u, g_hold[1]);
    ET_CHECK_U32_EQ(0x0022u, g_hold[2]);
    ET_CHECK_U32_EQ(0x0033u, g_hold[3]);

    exp[0] = SLAVE; exp[1] = ET_MODBUS_FC_WRITE_MULTIPLE;
    exp[2] = 0u; exp[3] = 1u; exp[4] = 0u; exp[5] = 3u;
    {
        uint16_t crc = et_crc16_modbus(exp, 6u);

        exp[6] = (uint8_t)(crc & 0xFFu);
        exp[7] = (uint8_t)(crc >> 8);
    }
    expect_resp(exp, 8u);
}

/* ---------- 异常路径 ---------- */

static void mb_exc_illegal_func(void)
{
    uint8_t req[4];
    uint8_t exp[5];

    setup();
    /* 未知功能码 0x63: 快路径不判长, 由静默路径界定 */
    req[0] = SLAVE; req[1] = 0x63u;
    {
        uint16_t crc = et_crc16_modbus(req, 2u);

        req[2] = (uint8_t)(crc & 0xFFu);
        req[3] = (uint8_t)(crc >> 8);
    }
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 4u));
    expect_resp(NULL, 0u);
    et_modbus_tick(&g_mb, 0u);
    et_modbus_tick(&g_mb, SILENCE_MS + 1u);
    exp[0] = SLAVE; exp[1] = 0x63u | 0x80u; exp[2] = ET_MODBUS_EXC_ILLEGAL_FUNC;
    {
        uint16_t crc = et_crc16_modbus(exp, 3u);

        exp[3] = (uint8_t)(crc & 0xFFu);
        exp[4] = (uint8_t)(crc >> 8);
    }
    expect_resp(exp, 5u);
}

static void mb_exc_illegal_addr(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    uint8_t exp[5];

    setup();
    put16(pay, 15u); put16(pay + 2u, 4u);    /* 15+4 > 16 → 钩子 0x02 */
    ET_CHECK_U32_EQ(8u, mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u));
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req, 8u));
    exp[0] = SLAVE; exp[1] = ET_MODBUS_FC_READ_HOLDING | 0x80u;
    exp[2] = ET_MODBUS_EXC_ILLEGAL_ADDR;
    {
        uint16_t crc = et_crc16_modbus(exp, 3u);

        exp[3] = (uint8_t)(crc & 0xFFu);
        exp[4] = (uint8_t)(crc >> 8);
    }
    expect_resp(exp, 5u);
}

static void mb_exc_illegal_value_qty(void)
{
    uint8_t req[8];
    uint8_t pay[4];

    setup();
    put16(pay, 0u); put16(pay + 2u, 0u);     /* qty = 0 */
    ET_CHECK_U32_EQ(8u, mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u));
    (void)et_modbus_feed(&g_mb, req, 8u);
    {
        uint32_t len = 0u;
        const uint8_t *r = et_modbus_response(&g_mb, &len);

        ET_CHECK_U32_EQ(5u, len);
        ET_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_VALUE, r[2]);
    }

    setup();
    put16(pay, 0u); put16(pay + 2u, 126u);   /* qty > 125 */
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    (void)et_modbus_feed(&g_mb, req, 8u);
    {
        uint32_t len = 0u;
        const uint8_t *r = et_modbus_response(&g_mb, &len);

        ET_CHECK_U32_EQ(5u, len);
        ET_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_VALUE, r[2]);
    }
}

static void mb_exc_illegal_value_wr_multiple(void)
{
    uint8_t req[16];
    uint8_t pay[9];

    /* 字节数与 qty 不符 (bc=4 但 qty=3) */
    setup();
    put16(pay, 0u); put16(pay + 2u, 3u); pay[4] = 4u;
    put16(pay + 5u, 1u); put16(pay + 7u, 2u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_WRITE_MULTIPLE, pay, 9u);
    (void)et_modbus_feed(&g_mb, req, 13u);
    {
        uint32_t len = 0u;
        const uint8_t *r = et_modbus_response(&g_mb, &len);

        ET_CHECK_U32_EQ(5u, len);
        ET_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_VALUE, r[2]);
    }

    /* qty > 123: 该帧长 257B 已超 RTU 上限(256), 属防御性分支 ——
     * 用 300B 容量实例直接把帧喂全, 验证框架 qty 界限判定 */
    {
        static uint8_t   rxb[300];
        static uint8_t   txb[300];
        static uint8_t   big[300];
        static uint8_t   reqbig[300];
        et_modbus_cfg_t  cfg;
        et_modbus_t      mb;

        memset(&cfg, 0, sizeof(cfg));
        cfg.slave_addr = SLAVE;
        cfg.silence_ms = SILENCE_MS;
        cfg.rd = rd_hook;
        cfg.wr = wr_hook;
        ET_CHECK(et_modbus_init(&mb, &cfg, rxb, sizeof(rxb), txb, sizeof(txb)));
        memset(big, 0, sizeof(big));
        put16(big, 0u); put16(big + 2u, 124u); big[4] = 248u;
        ET_CHECK_U32_EQ(257u, mk(reqbig, SLAVE, ET_MODBUS_FC_WRITE_MULTIPLE,
                                  big, 253u));
        (void)et_modbus_feed(&mb, reqbig, 257u);
        {
            uint32_t len = 0u;
            const uint8_t *r = et_modbus_response(&mb, &len);

            ET_CHECK_U32_EQ(5u, len);
            ET_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_VALUE, r[2]);
        }
    }
}

static void mb_exc_null_hooks(void)
{
    et_modbus_cfg_t cfg;
    uint8_t req[8];
    uint8_t pay[4];
    et_modbus_t mb;

    memset(&cfg, 0, sizeof(cfg));
    cfg.slave_addr = SLAVE;
    cfg.silence_ms = SILENCE_MS;
    cfg.rd = NULL;
    cfg.wr = NULL;
    ET_CHECK(et_modbus_init(&mb, &cfg, g_rx, sizeof(g_rx), g_tx, sizeof(g_tx)));
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    (void)et_modbus_feed(&mb, req, 8u);
    {
        uint32_t len = 0u;
        const uint8_t *r = et_modbus_response(&mb, &len);

        ET_CHECK_U32_EQ(5u, len);
        ET_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_ADDR, r[2]);
    }
}

/* ---------- 静默丢弃 ---------- */

static void mb_crc_bad_silent(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    et_modbus_stats_t st;

    setup();
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    req[6] ^= 0xFFu;                        /* 破坏 CRC */
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 8u));
    expect_resp(NULL, 0u);
    et_modbus_stats(&g_mb, &st);
    ET_CHECK_U32_EQ(1u, st.crc_err);
    ET_CHECK_U32_EQ(0u, st.frames);
}

static void mb_addr_mismatch_silent(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    et_modbus_stats_t st;

    setup();
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, 0x22u, ET_MODBUS_FC_READ_HOLDING, pay, 4u);   /* 别的从站 */
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 8u));
    expect_resp(NULL, 0u);
    et_modbus_stats(&g_mb, &st);
    ET_CHECK_U32_EQ(1u, st.addr_mismatch);
}

static void mb_tick_discards_partial(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    et_modbus_stats_t st;

    setup();
    put16(pay, 0u); put16(pay + 2u, 2u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 3u));   /* 半截帧 */
    et_modbus_tick(&g_mb, 0u);
    et_modbus_tick(&g_mb, SILENCE_MS + 1u);                /* 静默到点: 残帧丢弃 */
    et_modbus_stats(&g_mb, &st);
    ET_CHECK_U32_EQ(1u, st.discarded);
    /* 丢弃后重新同步: 完整帧仍正常 */
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req, 8u));
    {
        uint32_t len = 0u;

        ET_CHECK(et_modbus_response(&g_mb, &len) != NULL);
        ET_CHECK_U32_EQ(9u, len);
    }
}

/* ---------- 广播 ---------- */

static void mb_broadcast_write_no_reply(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    et_modbus_stats_t st;

    setup();
    put16(pay, 5u); put16(pay + 2u, 0xABCDu);
    (void)mk(req, 0u, ET_MODBUS_FC_WRITE_SINGLE, pay, 4u);
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 8u));
    expect_resp(NULL, 0u);
    ET_CHECK_U32_EQ(0xABCDu, g_hold[5]);    /* 写已执行 */
    et_modbus_stats(&g_mb, &st);
    ET_CHECK_U32_EQ(1u, st.frames);
    ET_CHECK_U32_EQ(0u, st.responses);
}

static void mb_broadcast_read_ignored(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    et_modbus_stats_t st;

    setup();
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, 0u, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 8u));
    expect_resp(NULL, 0u);
    et_modbus_stats(&g_mb, &st);
    ET_CHECK_U32_EQ(1u, st.frames);         /* 记账, 但无应答无动作 */
    ET_CHECK_U32_EQ(0u, st.responses);
}

/* ---------- 分片 / 粘包 ---------- */

static void mb_fragmented_feed(void)
{
    uint8_t req[8];
    uint8_t pay[4];

    setup();
    put16(pay, 0u); put16(pay + 2u, 2u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 3u));    /* 分三段喂入 */
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req + 3u, 3u));
    expect_resp(NULL, 0u);
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req + 6u, 2u));
    {
        uint32_t len = 0u;

        ET_CHECK(et_modbus_response(&g_mb, &len) != NULL);
        ET_CHECK_U32_EQ(9u, len);
    }
}

static void mb_sticky_two_frames(void)
{
    uint8_t a[8];
    uint8_t b[8];
    uint8_t pay[4];
    uint8_t buf[16];

    setup();
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(a, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    put16(pay, 2u); put16(pay + 2u, 1u);
    (void)mk(b, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    memcpy(buf, a, 8u);
    memcpy(buf + 8u, b, 8u);

    /* 一次 feed 两帧: 先出第 1 帧应答(粘包留待排空) */
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, buf, 16u));
    {
        uint32_t len = 0u;
        const uint8_t *r = et_modbus_response(&g_mb, &len);

        ET_CHECK_U32_EQ(7u, len);           /* qty=1 → 5+2 */
        ET_CHECK_U32_EQ(1u, (uint32_t)((r[3] << 8) | r[4]));    /* hold0 = 1 */
    }
    /* 应答取走后 feed(NULL,0) 继续: 第 2 帧 */
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, NULL, 0u));
    {
        uint32_t len = 0u;
        const uint8_t *r = et_modbus_response(&g_mb, &len);

        ET_CHECK_U32_EQ(7u, len);
        ET_CHECK_U32_EQ(3u, (uint32_t)((r[3] << 8) | r[4]));    /* hold2 = 3 */
    }
}

/* ---------- 边界与统计 ---------- */

static void mb_read_qty_boundary_125(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    et_modbus_cfg_t cfg;
    et_modbus_t mb;
    uint32_t len = 0u;
    const uint8_t *r;

    memset(&g_big, 0, sizeof(g_big));
    memset(&cfg, 0, sizeof(cfg));
    cfg.slave_addr = SLAVE;
    cfg.silence_ms = SILENCE_MS;
    cfg.rd = rd_big;
    cfg.wr = wr_big;
    ET_CHECK(et_modbus_init(&mb, &cfg, g_rx2, sizeof(g_rx2),
                            g_tx2, sizeof(g_tx2)));
    put16(pay, 0u); put16(pay + 2u, ET_MODBUS_RD_QTY_MAX);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&mb, req, 8u));
    r = et_modbus_response(&mb, &len);
    ET_CHECK_U32_EQ(255u, len);             /* 5 + 250 */
    ET_CHECK_U32_EQ(250u, r[2]);
}

static void mb_write_qty_boundary_123(void)
{
    uint8_t req[256];
    uint8_t pay[256];
    et_modbus_cfg_t cfg;
    et_modbus_t mb;
    uint32_t len = 0u;

    memset(&g_big, 0, sizeof(g_big));
    memset(&cfg, 0, sizeof(cfg));
    cfg.slave_addr = SLAVE;
    cfg.silence_ms = SILENCE_MS;
    cfg.rd = rd_big;
    cfg.wr = wr_big;
    ET_CHECK(et_modbus_init(&mb, &cfg, g_rx2, sizeof(g_rx2),
                            g_tx2, sizeof(g_tx2)));
    put16(pay, 0u);
    put16(pay + 2u, ET_MODBUS_WR_QTY_MAX);
    pay[4] = (uint8_t)(2u * ET_MODBUS_WR_QTY_MAX);      /* 246 */
    {
        uint32_t i;

        for (i = 0u; i < ET_MODBUS_WR_QTY_MAX; i++) {
            put16(pay + 5u + (2u * i), (uint16_t)(0x1000u + i));
        }
    }
    ET_CHECK_U32_EQ(255u, mk(req, SLAVE, ET_MODBUS_FC_WRITE_MULTIPLE, pay,
                             5u + (2u * ET_MODBUS_WR_QTY_MAX)));
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&mb, req, 255u));
    ET_CHECK(et_modbus_response(&mb, &len) != NULL);
    ET_CHECK_U32_EQ(8u, len);
    ET_CHECK_U32_EQ(0x1000u, g_big[0]);
    ET_CHECK_U32_EQ((uint16_t)(0x1000u + 122u), g_big[122]);
}

static void mb_txcap_small_value_error(void)
{
    et_modbus_cfg_t cfg;
    et_modbus_t mb;
    uint8_t req[8];
    uint8_t pay[4];
    uint8_t tx[8];                          /* 只能放最小帧 */
    uint32_t len = 0u;
    const uint8_t *r;

    memset(&cfg, 0, sizeof(cfg));
    cfg.slave_addr = SLAVE;
    cfg.silence_ms = SILENCE_MS;
    cfg.rd = rd_hook;
    cfg.wr = wr_hook;
    ET_CHECK(et_modbus_init(&mb, &cfg, g_rx, sizeof(g_rx), tx, sizeof(tx)));
    put16(pay, 0u); put16(pay + 2u, 4u);    /* 应答需 5+8=13 > 8 */
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    (void)et_modbus_feed(&mb, req, 8u);
    r = et_modbus_response(&mb, &len);
    ET_CHECK_U32_EQ(5u, len);
    ET_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_VALUE, r[2]);
}

static void mb_stats_accumulate(void)
{
    uint8_t req[8];
    uint8_t pay[4];
    et_modbus_stats_t st;

    setup();
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    (void)et_modbus_feed(&g_mb, req, 8u);       /* 正常 */
    {
        uint8_t bad[8];

        memcpy(bad, req, 8u);
        bad[6] ^= 0x55u;                        /* CRC 错 */
        (void)et_modbus_feed(&g_mb, bad, 8u);
        (void)mk(bad, 0x33u, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
        (void)et_modbus_feed(&g_mb, bad, 8u);   /* 地址不符 */
    }
    put16(pay, 14u); put16(pay + 2u, 4u);       /* 14+4 > 16 → 异常 0x02 */
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    (void)et_modbus_feed(&g_mb, req, 8u);
    et_modbus_stats(&g_mb, &st);
    ET_CHECK_U32_EQ(2u, st.frames);
    ET_CHECK_U32_EQ(2u, st.responses);
    ET_CHECK_U32_EQ(1u, st.exceptions);
    ET_CHECK_U32_EQ(1u, st.crc_err);
    ET_CHECK_U32_EQ(1u, st.addr_mismatch);
}

static void mb_multi_instance(void)
{
    static et_modbus_t mb_a;
    static et_modbus_t mb_b;
    static uint8_t rxa[64], txa[64], rxb[64], txb[64];
    et_modbus_cfg_t ca;
    et_modbus_cfg_t cb;
    uint8_t req[8];
    uint8_t pay[4];
    uint32_t la = 0u, lb = 0u;

    memset(&ca, 0, sizeof(ca));
    ca.slave_addr = 0x11u; ca.silence_ms = SILENCE_MS;
    ca.rd = rd_hook; ca.wr = wr_hook;
    cb = ca;
    cb.slave_addr = 0x22u;
    setup();
    ET_CHECK(et_modbus_init(&mb_a, &ca, rxa, sizeof(rxa), txa, sizeof(txa)));
    ET_CHECK(et_modbus_init(&mb_b, &cb, rxb, sizeof(rxb), txb, sizeof(txb)));

    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, 0x11u, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_feed(&mb_a, req, 8u));
    ET_CHECK_U32_EQ(0u, et_modbus_feed(&mb_b, req, 8u));    /* B 地址不符 */
    ET_CHECK(et_modbus_response(&mb_a, &la) != NULL);
    ET_CHECK_U32_EQ(7u, la);                /* qty=1 → 5+2 字节 */
    ET_CHECK(et_modbus_response(&mb_b, &lb) == NULL);
    ET_CHECK_U32_EQ(0u, lb);
    {
        et_modbus_stats_t sa;
        et_modbus_stats_t sb;

        et_modbus_stats(&mb_a, &sa);
        et_modbus_stats(&mb_b, &sb);
        ET_CHECK_U32_EQ(1u, sa.frames);
        ET_CHECK_U32_EQ(1u, sb.addr_mismatch);  /* 独立统计 */
    }
}

static void mb_rx_pending(void)
{
    uint8_t req[8];
    uint8_t pay[4];

    setup();
    ET_CHECK_U32_EQ(0u, et_modbus_rx_pending(&g_mb));
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    (void)et_modbus_feed(&g_mb, req, 3u);           /* 半帧在缓冲 */
    ET_CHECK_U32_EQ(3u, et_modbus_rx_pending(&g_mb));
    (void)et_modbus_feed(&g_mb, req + 3u, 5u);      /* 收齐 → 出应答, 缓冲清空 */
    ET_CHECK_U32_EQ(0u, et_modbus_rx_pending(&g_mb));
    ET_CHECK(et_modbus_response(&g_mb, NULL) != NULL);
}

const et_test_case_t *test_modbus_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"mb.init_validation",        mb_init_validation},
        {"mb.read_holding_normal",    mb_read_holding_normal},
        {"mb.read_input_normal",      mb_read_input_normal},
        {"mb.write_single_echo",      mb_write_single_echo},
        {"mb.write_multiple_normal",  mb_write_multiple_normal},
        {"mb.exc_illegal_func",       mb_exc_illegal_func},
        {"mb.exc_illegal_addr",       mb_exc_illegal_addr},
        {"mb.exc_illegal_value_qty",  mb_exc_illegal_value_qty},
        {"mb.exc_illegal_value_wr",   mb_exc_illegal_value_wr_multiple},
        {"mb.exc_null_hooks",         mb_exc_null_hooks},
        {"mb.crc_bad_silent",         mb_crc_bad_silent},
        {"mb.addr_mismatch_silent",   mb_addr_mismatch_silent},
        {"mb.tick_discards_partial",  mb_tick_discards_partial},
        {"mb.broadcast_write",        mb_broadcast_write_no_reply},
        {"mb.broadcast_read_ignored", mb_broadcast_read_ignored},
        {"mb.fragmented_feed",        mb_fragmented_feed},
        {"mb.sticky_two_frames",      mb_sticky_two_frames},
        {"mb.read_qty_125",           mb_read_qty_boundary_125},
        {"mb.write_qty_123",          mb_write_qty_boundary_123},
        {"mb.txcap_small",            mb_txcap_small_value_error},
        {"mb.stats_accumulate",       mb_stats_accumulate},
        {"mb.multi_instance",         mb_multi_instance},
        {"mb.rx_pending",             mb_rx_pending},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

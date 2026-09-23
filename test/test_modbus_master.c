/**
 * @file    test_modbus_master.c
 * @brief   et_modbus_master 单元测试 (脚本化从站应答 + 虚拟时间驱动状态机)
 *
 * 手法: 从站侧应答帧由本文件按协议构造(CRC16-MODBUS 线上低字节在前), 与主站
 * 请求帧做**逐字节**比对; 时间由 g_now 虚拟推进, 超时/重发/迟到路径全速可测。
 * 功能码一律经 ET_MODBUS_FC_* 引用(HC-5)。
 */
#include <string.h>
#include "et_test.h"
#include "et_modbus.h"
#include "et_modbus_master.h"
#include "et_crc.h"

#define ADDR        0x11u
#define TIMEOUT_MS  50u

static uint8_t           g_mrx[ET_MODBUS_ADU_MAX];
static uint8_t           g_mtx[ET_MODBUS_ADU_MAX];
static et_modbus_master_t g_m;
static uint32_t          g_now;

/* ---- 夹具 ---- */

static void setup(uint8_t retry_max, uint16_t addr)
{
    et_modbus_master_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.addr            = addr;
    cfg.resp_timeout_ms = TIMEOUT_MS;
    cfg.retry_max       = retry_max;
    g_now = 1000u;
    ET_CHECK(et_modbus_master_init(&g_m, &cfg, g_mrx, sizeof(g_mrx),
                                   g_mtx, sizeof(g_mtx)));
}

/* 上线: 取待发请求并 sent(); 返回请求字节数(无待发则 0) */
static uint32_t push_tx(void)
{
    uint32_t       len = 0u;
    const uint8_t *req = et_modbus_master_tx(&g_m, &len);

    if (req == NULL) {
        return 0u;
    }
    ET_CHECK(req == g_mtx);                 /* 请求取自调用方缓冲 */
    ET_CHECK(len > 0u);
    et_modbus_master_sent(&g_m, g_now);
    return len;
}

static et_mb_status_t poll_after(uint32_t ms)
{
    g_now += ms;
    return et_modbus_master_poll(&g_m, g_now);
}

/* ---- 从站侧应答构造 ---- */

static uint32_t seal(uint8_t *f, uint32_t len)
{
    uint16_t crc = et_crc16_modbus(f, len);

    f[len]      = (uint8_t)(crc & 0xFFu);
    f[len + 1u] = (uint8_t)(crc >> 8);
    return len + 2u;
}

static uint32_t mk_read_resp(uint8_t *f, uint8_t addr, uint8_t fc,
                             const uint16_t *vals, uint16_t qty)
{
    uint16_t i;

    f[0] = addr;
    f[1] = fc;
    f[2] = (uint8_t)(2u * qty);
    for (i = 0u; i < qty; i++) {
        f[3u + (2u * i)]      = (uint8_t)(vals[i] >> 8);
        f[3u + (2u * i) + 1u] = (uint8_t)(vals[i] & 0xFFu);
    }
    return seal(f, 3u + (2u * (uint32_t)qty));
}

/* 写应答回显: 末两字节 = 0x06 的"值" / 0x10 的"数量"(协议本身如此) */
static uint32_t mk_write_resp(uint8_t *f, uint8_t addr, uint8_t fc,
                              uint16_t reg, uint16_t last)
{
    f[0] = addr;
    f[1] = fc;
    f[2] = (uint8_t)(reg >> 8);
    f[3] = (uint8_t)(reg & 0xFFu);
    f[4] = (uint8_t)(last >> 8);
    f[5] = (uint8_t)(last & 0xFFu);
    return seal(f, 6u);
}

static uint32_t mk_exc_resp(uint8_t *f, uint8_t addr, uint8_t fc, uint8_t exc)
{
    f[0] = addr;
    f[1] = (uint8_t)(fc | 0x80u);
    f[2] = exc;
    return seal(f, 3u);
}

/* 逐字节比对主站请求与期望帧 */
static void check_req(const uint8_t *want, uint32_t wantlen)
{
    uint32_t len = 0u;
    const uint8_t *req = et_modbus_master_tx(&g_m, &len);

    ET_CHECK(req != NULL);
    ET_CHECK_U32_EQ(wantlen, len);
    ET_CHECK(memcmp(req, want, wantlen) == 0);
}

/* =====================================================================
 * 用例
 * ===================================================================== */

static void mbm_init_validation(void)
{
    et_modbus_master_cfg_t cfg;
    et_modbus_master_t     m;
    uint8_t rx[ET_MODBUS_ADU_MAX], tx[ET_MODBUS_ADU_MAX];

    memset(&cfg, 0, sizeof(cfg));
    cfg.addr            = ADDR;
    cfg.resp_timeout_ms = TIMEOUT_MS;

    ET_CHECK(!et_modbus_master_init(NULL, &cfg, rx, sizeof(rx), tx, sizeof(tx)));
    ET_CHECK(!et_modbus_master_init(&m, NULL, rx, sizeof(rx), tx, sizeof(tx)));
    ET_CHECK(!et_modbus_master_init(&m, &cfg, NULL, sizeof(rx), tx, sizeof(tx)));
    ET_CHECK(!et_modbus_master_init(&m, &cfg, rx, sizeof(rx), NULL, sizeof(tx)));

    cfg.addr = 248u;                        /* 248~255 保留 */
    ET_CHECK(!et_modbus_master_init(&m, &cfg, rx, sizeof(rx), tx, sizeof(tx)));
    cfg.addr = 247u;
    ET_CHECK(et_modbus_master_init(&m, &cfg, rx, sizeof(rx), tx, sizeof(tx)));

    cfg.resp_timeout_ms = 0u;               /* 超时须由调用方给出 */
    ET_CHECK(!et_modbus_master_init(&m, &cfg, rx, sizeof(rx), tx, sizeof(tx)));
    cfg.resp_timeout_ms = TIMEOUT_MS;

    cfg.addr = 0u;                          /* 广播地址合法(仅写) */
    ET_CHECK(et_modbus_master_init(&m, &cfg, rx, sizeof(rx), tx, sizeof(tx)));

    /* 缓冲硬底线: ≥ ET_MODBUS_ADU_MAX */
    cfg.addr = ADDR;
    ET_CHECK(!et_modbus_master_init(&m, &cfg, rx, ET_MODBUS_ADU_MAX - 1u,
                                    tx, sizeof(tx)));
    ET_CHECK(!et_modbus_master_init(&m, &cfg, rx, sizeof(rx),
                                    tx, ET_MODBUS_ADU_MAX - 1u));
    ET_CHECK(et_modbus_master_init(&m, &cfg, rx, ET_MODBUS_ADU_MAX,
                                   tx, ET_MODBUS_ADU_MAX));
}

/* AC: 0x03 读正常流 + 请求帧逐字节(与 v2.4 走单记录同一向量) */
static void mbm_read_holding_normal(void)
{
    static const uint8_t want[8] = { 0x11u, 0x03u, 0x00u, 0x00u,
                                     0x00u, 0x04u, 0x46u, 0x99u };
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 1u, 2u, 3u, 4u };
    uint16_t qty = 0u;
    uint32_t dlen = 0u;
    const uint8_t *d;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    check_req(want, sizeof(want));           /* 11 03 00 00 00 04 46 99 */
    ET_CHECK_U32_EQ(8u, push_tx());
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);

    ET_CHECK_U32_EQ(1u, et_modbus_master_result(&g_m, &qty));   /* 首值 */
    ET_CHECK_U32_EQ(4u, qty);
    d = et_modbus_master_data(&g_m, &dlen);
    ET_CHECK(d != NULL);
    ET_CHECK_U32_EQ(8u, dlen);
    ET_CHECK(d[0] == 0u && d[1] == 1u && d[2] == 0u && d[3] == 2u &&
             d[4] == 0u && d[5] == 3u && d[6] == 0u && d[7] == 4u);   /* 逐值 */
}

static void mbm_read_input_normal(void)
{
    uint8_t  want[8];
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[2] = { 407u, 507u };
    uint16_t qty = 0u;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_INPUT, 4u, 2u));
    want[0] = ADDR; want[1] = ET_MODBUS_FC_READ_INPUT;
    want[2] = 0u;   want[3] = 4u;            /* reg = 4 */
    want[4] = 0u;   want[5] = 2u;            /* qty = 2 */
    (void)seal(want, 6u);
    check_req(want, sizeof(want));           /* 逐字节(含 CRC) */
    ET_CHECK_U32_EQ(8u, push_tx());

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_INPUT, vals, 2u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(407u, et_modbus_master_result(&g_m, &qty));
    ET_CHECK_U32_EQ(2u, qty);
}

static void mbm_write_single_normal(void)
{
    uint8_t  resp[16];
    uint32_t resplen;
    uint16_t v = 0x1234u;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_SINGLE, 2u, &v, 1u));
    {
        static const uint8_t want[8] = { 0x11u, 0x06u, 0x00u, 0x02u,
                                         0x12u, 0x34u, 0x27u, 0xEDu };

        check_req(want, sizeof(want));       /* 与 v2.4 走单记录同向量 */
    }
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_write_resp(resp, ADDR, ET_MODBUS_FC_WRITE_SINGLE, 2u, 0x1234u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    {
        et_modbus_master_stats_t st;

        et_modbus_master_stats(&g_m, &st);
        ET_CHECK_U32_EQ(1u, st.responses);
        ET_CHECK_U32_EQ(0u, st.discarded);
    }
}

static void mbm_write_multiple_normal(void)
{
    uint8_t  resp[16];
    uint32_t resplen;
    uint16_t vals[3] = { 111u, 222u, 333u };

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_MULTIPLE, 4u, vals, 3u));
    {
        uint32_t len = 0u;
        const uint8_t *req = et_modbus_master_tx(&g_m, &len);

        ET_CHECK_U32_EQ(15u, len);           /* 9 + 2*3 */
        ET_CHECK(req[1] == ET_MODBUS_FC_WRITE_MULTIPLE);
        ET_CHECK(req[6] == 6u);              /* bytecount */
        ET_CHECK(req[7] == 0u && req[8] == 111u);
        ET_CHECK(req[11] == 0x01u && req[12] == 0x4Du);      /* 333 = 0x014D */
        ET_CHECK(et_crc16_modbus(req, 13u) ==
                 (uint16_t)((uint16_t)req[13] | ((uint16_t)req[14] << 8)));
    }
    ET_CHECK_U32_EQ(15u, push_tx());
    resplen = mk_write_resp(resp, ADDR, ET_MODBUS_FC_WRITE_MULTIPLE, 4u, 3u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
}

/* 分片应答: 逐段喂入, 收齐才解析 */
static void mbm_fragmented_response(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 9u, 8u, 7u, 6u };
    uint16_t qty = 0u;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(13u, resplen);

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp, 3u));       /* 3 */
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp + 3u, 3u));  /* +3 */
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp + 6u, 4u));  /* +4 */
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp + 10u, 3u)); /* +3 收齐 */
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(9u, et_modbus_master_result(&g_m, &qty));
    ET_CHECK_U32_EQ(4u, qty);
}

/* AC-14: 超时重发 → 重发字节与首帧逐字节相同 → 耗尽 → TIMEOUT, 请求数 = 3 */
static void mbm_timeout_retry_exhaust(void)
{
    uint8_t  first[16];
    uint32_t firstlen;
    et_modbus_master_stats_t st;

    setup(2u, ADDR);                         /* retry_max = 2 */
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    firstlen = push_tx();
    ET_CHECK_U32_EQ(8u, firstlen);
    memcpy(first, g_mtx, firstlen);

    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);   /* 未到超时 */
    ET_CHECK(et_modbus_master_poll(&g_m, g_now + TIMEOUT_MS - 1u) == ET_MB_BUSY);

    /* 第 1 次超时 → 置重发 */
    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_BUSY);
    ET_CHECK(et_modbus_master_tx(&g_m, NULL) != NULL);
    {
        uint32_t len = 0u;
        const uint8_t *req = et_modbus_master_tx(&g_m, &len);

        ET_CHECK_U32_EQ(firstlen, len);
        ET_CHECK(memcmp(req, first, firstlen) == 0);   /* 含 CRC 逐字节相同 */
    }
    ET_CHECK_U32_EQ(8u, push_tx());

    /* 第 2 次超时 → 置重发 */
    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_BUSY);
    ET_CHECK_U32_EQ(8u, push_tx());

    /* 第 3 次超时 → 重发耗尽, 终态 */
    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_TIMEOUT);
    ET_CHECK(et_modbus_master_tx(&g_m, NULL) == NULL);
    ET_CHECK(et_modbus_master_poll(&g_m, g_now + 10000u) == ET_MB_TIMEOUT);  /* 终态保持 */

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(3u, st.requests);        /* 首帧 + 2 次重发 */
    ET_CHECK_U32_EQ(2u, st.retries);
    ET_CHECK_U32_EQ(1u, st.timeouts);
    ET_CHECK_U32_EQ(0u, st.responses);
}

/* 重发后成功: retry_max=2, 第 1 次超时重发后收到应答 */
static void mbm_retry_then_success(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[2] = { 5u, 6u };
    uint16_t qty = 0u;
    et_modbus_master_stats_t st;

    setup(2u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 2u));
    ET_CHECK_U32_EQ(8u, push_tx());

    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_BUSY);      /* 超时 → 重发 */
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 2u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(5u, et_modbus_master_result(&g_m, &qty));
    ET_CHECK_U32_EQ(2u, qty);

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(2u, st.requests);
    ET_CHECK_U32_EQ(1u, st.retries);
    ET_CHECK_U32_EQ(0u, st.timeouts);
}

/* retry_max = 0: 一次超时即终态, 不重发 */
static void mbm_timeout_no_retry(void)
{
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 1u));
    ET_CHECK_U32_EQ(8u, push_tx());
    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_TIMEOUT);
    ET_CHECK(et_modbus_master_tx(&g_m, NULL) == NULL);
    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(1u, st.requests);
    ET_CHECK_U32_EQ(0u, st.retries);
}

/* AC-16: 异常应答 → ET_MB_EXC, 异常码一致, 不重试 */
static void mbm_exception_no_retry(void)
{
    uint8_t  resp[16];
    uint32_t resplen;
    et_modbus_master_stats_t st;

    setup(2u, ADDR);                         /* 即使允许重发也不重试 */
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 100u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_exc_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING,
                          ET_MODBUS_EXC_ILLEGAL_ADDR);
    ET_CHECK_U32_EQ(5u, resplen);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_EXC);
    ET_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_ADDR, et_modbus_master_exc(&g_m));
    ET_CHECK(et_modbus_master_tx(&g_m, NULL) == NULL);   /* 无重发 */

    /* 时间推进后仍是 EXC(不因超时转 TIMEOUT) */
    ET_CHECK(poll_after(TIMEOUT_MS * 10u) == ET_MB_EXC);

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(1u, st.requests);
    ET_CHECK_U32_EQ(0u, st.retries);
    ET_CHECK_U32_EQ(1u, st.exceptions);
    ET_CHECK_U32_EQ(0u, st.timeouts);

    /* 其它异常码 */
    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_INPUT, 0u, 1u));
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_exc_resp(resp, ADDR, ET_MODBUS_FC_READ_INPUT,
                          ET_MODBUS_EXC_ILLEGAL_VALUE);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_EXC);
    ET_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_VALUE, et_modbus_master_exc(&g_m));
}

/* AC-17: 广播写(addr=0) → sent() 后立即 OK, 不等应答 */
static void mbm_broadcast_write(void)
{
    uint16_t v = 0xBEEFu;
    et_modbus_master_stats_t st;

    setup(2u, 0u);                           /* 广播地址 */
    ET_CHECK(et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_SINGLE, 5u, &v, 1u));
    {
        static const uint8_t want[8] = { 0x00u, 0x06u, 0x00u, 0x05u,
                                         0xBEu, 0xEFu, 0x00u, 0x00u };
        uint32_t len = 0u;
        const uint8_t *req = et_modbus_master_tx(&g_m, &len);

        ET_CHECK_U32_EQ(8u, len);
        ET_CHECK(memcmp(req, want, 6u) == 0);        /* addr=0 + 载荷 */
        ET_CHECK(et_crc16_modbus(req, 6u) ==
                 (uint16_t)((uint16_t)req[6] | ((uint16_t)req[7] << 8)));
    }
    ET_CHECK_U32_EQ(8u, push_tx());

    /* 时间**未推进**: 立即终态 OK(未进入等应答) */
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK(et_modbus_master_tx(&g_m, NULL) == NULL);
    ET_CHECK(poll_after(0u) == ET_MB_OK);

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(1u, st.requests);
    ET_CHECK_U32_EQ(0u, st.timeouts);
    ET_CHECK_U32_EQ(0u, st.responses);
}

/* 广播读: 主站不允许(无应答) */
static void mbm_broadcast_read_rejected(void)
{
    setup(0u, 0u);
    ET_CHECK(!et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 1u));
    ET_CHECK(et_modbus_master_tx(&g_m, NULL) == NULL);
}

/* AC-15: 终态后的迟到/陈旧字节 → 计数丢弃, 且不污染下一事务 */
static void mbm_late_response_no_pollution(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 1u, 2u, 3u, 4u };
    uint16_t qty = 0u;
    uint32_t dlen = 0u;
    const uint8_t *d;
    et_modbus_master_stats_t st;

    setup(0u, ADDR);

    /* 事务 1: 读 qty=2 → OK */
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 2u));
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 2u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);

    /* 终态后喂入上一事务的应答字节 → 丢弃计数 +1 */
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp, resplen));
    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(1u, st.late);

    /* 事务 2: 读 qty=4 → 读回值必须与期望逐值相等(不被上一事务污染) */
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(13u, resplen);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(1u, et_modbus_master_result(&g_m, &qty));
    ET_CHECK_U32_EQ(4u, qty);
    d = et_modbus_master_data(&g_m, &dlen);
    ET_CHECK(d != NULL && dlen == 8u);
    ET_CHECK(d[0] == 0u && d[1] == 1u && d[2] == 0u && d[3] == 2u &&
             d[4] == 0u && d[5] == 3u && d[6] == 0u && d[7] == 4u);

    /* 半帧残字节也不得跨事务残留: 终态期喂入的半帧被丢弃, 下一事务照常解析 */
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp, 4u));    /* 陈旧半帧 */
    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(2u, st.late);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 1u));
    ET_CHECK_U32_EQ(8u, push_tx());
    {
        uint16_t one = 0x4321u;
        uint8_t  r1[16];
        uint32_t l1 = mk_read_resp(r1, ADDR, ET_MODBUS_FC_READ_HOLDING, &one, 1u);

        ET_CHECK_U32_EQ(7u, l1);
        ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, r1, l1));
        ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
        ET_CHECK_U32_EQ(0x4321u, et_modbus_master_result(&g_m, NULL));
    }
}

/* CRC 坏帧: 丢弃计数并继续等真应答 */
static void mbm_crc_bad_discarded(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 7u, 7u, 7u, 7u };
    uint16_t qty = 0u;
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    resp[4] ^= 0xFFu;                        /* 破坏数据域(CRC 失配) */
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp, resplen));
    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(1u, st.crc_err);
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);   /* 继续等 */

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(7u, et_modbus_master_result(&g_m, &qty));
    ET_CHECK_U32_EQ(4u, qty);
}

/* 地址不符: 丢弃计数并继续等 */
static void mbm_addr_mismatch_discarded(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[2] = { 3u, 4u };
    uint16_t qty = 0u;
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 2u));
    ET_CHECK_U32_EQ(8u, push_tx());

    resplen = mk_read_resp(resp, 0x22u, ET_MODBUS_FC_READ_HOLDING, vals, 2u);
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp, resplen));
    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(1u, st.addr_mismatch);

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 2u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(3u, et_modbus_master_result(&g_m, &qty));
}

/* 垃圾字节在前: 逐字节重同步后仍能取到真应答 */
static void mbm_garbage_resync(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 1u, 2u, 3u, 4u };
    static const uint8_t junk[2] = { 0xFFu, 0x00u };
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, junk, sizeof(junk)));
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(1u, et_modbus_master_result(&g_m, NULL));

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(2u, st.discarded);       /* 0xFF 与错位的 0x11 各一次 */
    ET_CHECK_U32_EQ(0u, st.crc_err);
}

/* 读应答字节数不符: 丢弃并继续等 */
static void mbm_read_bc_mismatch(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 1u, 2u, 3u, 4u };
    uint16_t qty = 0u;
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 2u);  /* 少 2 个 */
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp, resplen));
    et_modbus_master_stats(&g_m, &st);
    ET_CHECK(st.discarded > 0u);

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(4u, (et_modbus_master_result(&g_m, &qty), qty));
}

/* 写应答回显不符: 丢弃并继续等 */
static void mbm_write_echo_mismatch(void)
{
    uint8_t  resp[16];
    uint32_t resplen;
    uint16_t v = 0x1234u;
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_SINGLE, 2u, &v, 1u));
    ET_CHECK_U32_EQ(8u, push_tx());

    resplen = mk_write_resp(resp, ADDR, ET_MODBUS_FC_WRITE_SINGLE, 5u, 0x1234u);  /* 回显地址错 */
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, resp, resplen));
    et_modbus_master_stats(&g_m, &st);
    ET_CHECK(st.discarded > 0u);
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);

    resplen = mk_write_resp(resp, ADDR, ET_MODBUS_FC_WRITE_SINGLE, 2u, 0x1234u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
}

/* =====================================================================
 * v2.6 P1-2 (CO-6 / HC-2 / AC-10): 噪声不得伪造帧长
 * 噪声形态: ① 半双工单线上主站自身请求的回显(CO-6 的主形态)
 *           ② 只有 [addr][读 fc] 的裸前缀; ③ 字节数域为奇数/超上限的假前缀。
 * 断言要点: 真应答仍被正确解析(不退化到"只能靠超时收场"),
 *           且从未成帧的噪声字节 **不得** 计入 crc_err(HC-2)。
 * ===================================================================== */

/* ① 自身请求回显: [addr][0x03][reg_hi=00] —— 第三字节被旧实现当作字节数域 */
static void mbm_noise_self_echo(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 1u, 2u, 3u, 4u };
    uint16_t qty = 0u;
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());         /* 请求 [11 03 00 00 00 04 crc] 在 g_mtx */

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, g_mtx, 8u));
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(1u, et_modbus_master_result(&g_m, &qty));
    ET_CHECK_U32_EQ(4u, qty);

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(0u, st.crc_err);        /* 噪声从未成帧 → 不得污染 crc_err */
    ET_CHECK(st.discarded > 0u);
    ET_CHECK_U32_EQ(0u, st.timeouts);
}

/* ② 裸前缀 [addr][0x03]: 字节数域尚未到达, 随后真应答的地址字节会被误当字节数域 */
static void mbm_noise_bare_head(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 9u, 8u, 7u, 6u };
    static const uint8_t head[2] = { ADDR, ET_MODBUS_FC_READ_HOLDING };
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, head, sizeof(head)));
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(9u, et_modbus_master_result(&g_m, NULL));

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(0u, st.crc_err);
}

/* ③ 字节数域为奇数(非法): 逐字节重同步, 不按噪声定长 */
static void mbm_noise_odd_bc(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 3u, 1u, 4u, 1u };
    static const uint8_t junk[4] = { ADDR, ET_MODBUS_FC_READ_HOLDING, 0x07u, 0xAAu };
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, junk, sizeof(junk)));
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(3u, et_modbus_master_result(&g_m, NULL));

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(0u, st.crc_err);
}

/* ④ 字节数域超上限(0xFF > 2*RD_QTY_MAX): 同样逐字节重同步 */
static void mbm_noise_oversize_bc(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[2] = { 0x1234u, 0x5678u };
    static const uint8_t junk[4] = { ADDR, ET_MODBUS_FC_READ_HOLDING, 0xFFu, 0x01u };
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 2u));
    ET_CHECK_U32_EQ(8u, push_tx());

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, junk, sizeof(junk)));
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 2u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(0x1234u, et_modbus_master_result(&g_m, NULL));

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(0u, st.crc_err);
}

/* ⑤ 与在途事务"严格同形"的坏帧(地址/fc/字节数域全对, 内容是垃圾):
 *    只有这种序列的 CRC 失败才计入 crc_err(HC-2 的计数门槛) */
static void mbm_noise_expected_shape(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[4] = { 5u, 6u, 7u, 8u };
    static const uint8_t fake[13] = {
        ADDR, ET_MODBUS_FC_READ_HOLDING, 0x08u,
        0xDEu, 0xADu, 0xBEu, 0xEFu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    ET_CHECK_U32_EQ(8u, push_tx());

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, fake, sizeof(fake)));
    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(1u, st.crc_err);
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 4u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(5u, et_modbus_master_result(&g_m, NULL));
}

/* ⑥ 重同步后 qty 上限(125)边界不回退: 满帧 255B 仍能在噪声后解析 */
static void mbm_noise_qty_max_resync(void)
{
    uint8_t  resp[300];
    uint32_t resplen;
    uint16_t vals[ET_MODBUS_RD_QTY_MAX];
    uint16_t qty = 0u;
    static const uint8_t junk[1] = { 0xFFu };
    et_modbus_master_stats_t st;

    memset(vals, 0, sizeof(vals));
    vals[0] = 0x0ABCu;
    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u,
                                   ET_MODBUS_RD_QTY_MAX));
    ET_CHECK_U32_EQ(8u, push_tx());

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, junk, sizeof(junk)));
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals,
                           ET_MODBUS_RD_QTY_MAX);
    ET_CHECK_U32_EQ(255u, resplen);         /* 3 + 2*125 + 2 */
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(0x0ABCu, et_modbus_master_result(&g_m, &qty));
    ET_CHECK_U32_EQ(ET_MODBUS_RD_QTY_MAX, qty);

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(0u, st.crc_err);
}

/* ⑦ 写路径不回退: 噪声前缀后写真应答(0x10 回显)仍能终结事务 */
static void mbm_noise_write_resync(void)
{
    uint8_t  resp[16];
    uint32_t resplen;
    uint16_t vals[3] = { 0x1111u, 0x2222u, 0x3333u };
    static const uint8_t junk[2] = { 0xFFu, 0x00u };
    et_modbus_master_stats_t st;

    setup(0u, ADDR);
    ET_CHECK(et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_MULTIPLE, 4u, vals, 3u));
    ET_CHECK_U32_EQ(15u, push_tx());

    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, junk, sizeof(junk)));
    resplen = mk_write_resp(resp, ADDR, ET_MODBUS_FC_WRITE_MULTIPLE, 4u, 3u);
    ET_CHECK_U32_EQ(8u, resplen);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(0u, st.crc_err);
    ET_CHECK_U32_EQ(1u, st.responses);
}

/* qty 边界: 读 0/125/126 */
static void mbm_qty_bounds_read(void)
{
    uint16_t v[126];

    setup(0u, ADDR);
    memset(v, 0, sizeof(v));
    ET_CHECK(!et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 0u));
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u,
                                   ET_MODBUS_RD_QTY_MAX));
    {
        uint32_t len = 0u;

        ET_CHECK_U32_EQ(8u, et_modbus_master_tx(&g_m, &len) != NULL ? len : 0u);
    }
    ET_CHECK_U32_EQ(8u, push_tx());
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);

    /* 在途期间不得发起新事务(单事务语义) */
    ET_CHECK(!et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 1u));
    ET_CHECK(!et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_SINGLE, 0u, v, 1u));

    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_TIMEOUT);       /* retry_max=0 */

    /* 终态后可再发起 */
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u,
                                   ET_MODBUS_RD_QTY_MAX + 1u) == false);
}

/* qty 边界: 写 0/123/124 + 单写 qty≠1 */
static void mbm_qty_bounds_write(void)
{
    uint16_t v[124];

    setup(0u, ADDR);
    memset(v, 0, sizeof(v));
    ET_CHECK(!et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_MULTIPLE, 0u, v, 0u));
    ET_CHECK(!et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_MULTIPLE, 0u, v,
                                     ET_MODBUS_WR_QTY_MAX + 1u));
    ET_CHECK(!et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_SINGLE, 0u, v, 2u));
    ET_CHECK(!et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_MULTIPLE, 0u, NULL, 1u));

    ET_CHECK(et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_MULTIPLE, 0u, v,
                                    ET_MODBUS_WR_QTY_MAX));
    {
        uint32_t len = 0u;

        ET_CHECK_U32_EQ(9u + (2u * ET_MODBUS_WR_QTY_MAX),
                        et_modbus_master_tx(&g_m, &len) != NULL ? len : 0u);
    }
    ET_CHECK_U32_EQ(9u + (2u * ET_MODBUS_WR_QTY_MAX), push_tx());
}

/* 功能码白名单: 读只收 0x03/0x04, 写只收 0x06/0x10 */
static void mbm_fc_rejected(void)
{
    uint16_t v[4];

    setup(0u, ADDR);
    memset(v, 0, sizeof(v));
    ET_CHECK(!et_modbus_master_read(&g_m, ET_MODBUS_FC_WRITE_SINGLE, 0u, 1u));
    ET_CHECK(!et_modbus_master_read(&g_m, ET_MODBUS_FC_WRITE_MULTIPLE, 0u, 1u));
    ET_CHECK(!et_modbus_master_read(&g_m, 0x63u, 0u, 1u));
    ET_CHECK(!et_modbus_master_write(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, v, 1u));
    ET_CHECK(!et_modbus_master_write(&g_m, ET_MODBUS_FC_READ_INPUT, 0u, v, 1u));
    ET_CHECK(!et_modbus_master_write(&g_m, 0x63u, 0u, v, 1u));
    ET_CHECK(et_modbus_master_tx(&g_m, NULL) == NULL);       /* 一律未组帧 */
}

/* 未初始化句柄: 一律拒绝 */
static void mbm_uninit_reject(void)
{
    et_modbus_master_t m;
    uint16_t v = 1u;

    memset(&m, 0, sizeof(m));                /* inited = false */
    ET_CHECK(!et_modbus_master_read(&m, ET_MODBUS_FC_READ_HOLDING, 0u, 1u));
    ET_CHECK(!et_modbus_master_write(&m, ET_MODBUS_FC_WRITE_SINGLE, 0u, &v, 1u));
    ET_CHECK(et_modbus_master_tx(&m, NULL) == NULL);
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&m, NULL, 0u));
    ET_CHECK(et_modbus_master_poll(&m, 0u) == ET_MB_REJECT);
    ET_CHECK(et_modbus_master_poll(NULL, 0u) == ET_MB_REJECT);
    ET_CHECK_U32_EQ(0u, et_modbus_master_result(&m, NULL));
    ET_CHECK(et_modbus_master_data(&m, NULL) == NULL);
    ET_CHECK_U32_EQ(0u, et_modbus_master_exc(&m));
}

/* tx() 语义: 唯一含义 = "有请求待上线"; sent() 后为 NULL */
static void mbm_tx_pending_semantics(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[1] = { 1u };
    uint32_t len = 0u;

    setup(1u, ADDR);
    ET_CHECK(et_modbus_master_tx(&g_m, &len) == NULL);        /* 空闲: 无待发 */
    ET_CHECK_U32_EQ(0u, len);

    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 1u));
    ET_CHECK(et_modbus_master_tx(&g_m, &len) != NULL);        /* 已组帧: 待上线 */
    ET_CHECK_U32_EQ(8u, len);
    ET_CHECK_U32_EQ(8u, push_tx());
    ET_CHECK(et_modbus_master_tx(&g_m, &len) == NULL);        /* 已上线: 无待发 */

    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_BUSY);           /* 超时 → 待重发 */
    ET_CHECK(et_modbus_master_tx(&g_m, &len) != NULL);
    ET_CHECK_U32_EQ(8u, len);
    ET_CHECK_U32_EQ(8u, push_tx());

    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 1u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    ET_CHECK(et_modbus_master_tx(&g_m, &len) == NULL);
}

/* 多实例: 状态与统计互不影响 */
static void mbm_multi_instance(void)
{
    static uint8_t rx_a[ET_MODBUS_ADU_MAX], tx_a[ET_MODBUS_ADU_MAX];
    static uint8_t rx_b[ET_MODBUS_ADU_MAX], tx_b[ET_MODBUS_ADU_MAX];
    et_modbus_master_cfg_t ca, cb;
    et_modbus_master_t ma, mb;
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[2] = { 11u, 22u };
    et_modbus_master_stats_t st;

    memset(&ca, 0, sizeof(ca));
    memset(&cb, 0, sizeof(cb));
    ca.addr = 0x11u; ca.resp_timeout_ms = TIMEOUT_MS;
    cb.addr = 0x22u; cb.resp_timeout_ms = TIMEOUT_MS;
    ET_CHECK(et_modbus_master_init(&ma, &ca, rx_a, sizeof(rx_a), tx_a, sizeof(tx_a)));
    ET_CHECK(et_modbus_master_init(&mb, &cb, rx_b, sizeof(rx_b), tx_b, sizeof(tx_b)));

    /* A 读 0x11; B 读 0x22 —— 并发在途互不干扰 */
    ET_CHECK(et_modbus_master_read(&ma, ET_MODBUS_FC_READ_HOLDING, 0u, 2u));
    ET_CHECK(et_modbus_master_read(&mb, ET_MODBUS_FC_READ_HOLDING, 0u, 2u));
    {
        uint32_t la = 0u, lb = 0u;

        ET_CHECK(et_modbus_master_tx(&ma, &la) != NULL);
        ET_CHECK(et_modbus_master_tx(&mb, &lb) != NULL);
        ET_CHECK_U32_EQ(8u, la);
        ET_CHECK_U32_EQ(8u, lb);
        ET_CHECK(ma.tx[0] == 0x11u);
        ET_CHECK(mb.tx[0] == 0x22u);
    }
    et_modbus_master_sent(&ma, g_now);
    et_modbus_master_sent(&mb, g_now);

    /* 地址不符的应答只被 B 接受 */
    resplen = mk_read_resp(resp, 0x22u, ET_MODBUS_FC_READ_HOLDING, vals, 2u);
    ET_CHECK_U32_EQ(0u, et_modbus_master_feed(&ma, resp, resplen));
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&mb, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&ma, g_now) == ET_MB_BUSY);
    ET_CHECK(et_modbus_master_poll(&mb, g_now) == ET_MB_OK);
    ET_CHECK_U32_EQ(11u, et_modbus_master_result(&mb, NULL));

    resplen = mk_read_resp(resp, 0x11u, ET_MODBUS_FC_READ_HOLDING, vals, 2u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&ma, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&ma, g_now) == ET_MB_OK);

    et_modbus_master_stats(&ma, &st);
    ET_CHECK_U32_EQ(1u, st.addr_mismatch);
    ET_CHECK_U32_EQ(1u, st.responses);
    et_modbus_master_stats(&mb, &st);
    ET_CHECK_U32_EQ(0u, st.addr_mismatch);
    ET_CHECK_U32_EQ(1u, st.responses);
}

/* 统计累积口径 */
static void mbm_stats_accumulate(void)
{
    uint8_t  resp[32];
    uint32_t resplen;
    uint16_t vals[2] = { 1u, 2u };
    et_modbus_master_stats_t st;

    setup(1u, ADDR);

    /* ① OK */
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 2u));
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_read_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING, vals, 2u);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);

    /* ② 异常 */
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 9u, 1u));
    ET_CHECK_U32_EQ(8u, push_tx());
    resplen = mk_exc_resp(resp, ADDR, ET_MODBUS_FC_READ_HOLDING,
                          ET_MODBUS_EXC_ILLEGAL_ADDR);
    ET_CHECK_U32_EQ(1u, et_modbus_master_feed(&g_m, resp, resplen));
    ET_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_EXC);

    /* ③ 超时 + 1 次重发 + 再超时 */
    ET_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 1u));
    ET_CHECK_U32_EQ(8u, push_tx());
    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_BUSY);
    ET_CHECK_U32_EQ(8u, push_tx());
    ET_CHECK(poll_after(TIMEOUT_MS) == ET_MB_TIMEOUT);

    et_modbus_master_stats(&g_m, &st);
    ET_CHECK_U32_EQ(4u, st.requests);        /* 1 + 1 + 2 */
    ET_CHECK_U32_EQ(1u, st.responses);       /* 异常应答计入 exceptions, 不计 responses */
    ET_CHECK_U32_EQ(1u, st.exceptions);
    ET_CHECK_U32_EQ(1u, st.retries);
    ET_CHECK_U32_EQ(1u, st.timeouts);
}

const et_test_case_t *test_modbus_master_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        { "mbm.init_validation",        mbm_init_validation },
        { "mbm.read_holding_normal",    mbm_read_holding_normal },
        { "mbm.read_input_normal",      mbm_read_input_normal },
        { "mbm.write_single_normal",    mbm_write_single_normal },
        { "mbm.write_multiple_normal",  mbm_write_multiple_normal },
        { "mbm.fragmented_response",    mbm_fragmented_response },
        { "mbm.timeout_retry_exhaust",  mbm_timeout_retry_exhaust },
        { "mbm.retry_then_success",     mbm_retry_then_success },
        { "mbm.timeout_no_retry",       mbm_timeout_no_retry },
        { "mbm.exception_no_retry",     mbm_exception_no_retry },
        { "mbm.broadcast_write",        mbm_broadcast_write },
        { "mbm.broadcast_read_rejected", mbm_broadcast_read_rejected },
        { "mbm.late_no_pollution",      mbm_late_response_no_pollution },
        { "mbm.crc_bad_discarded",      mbm_crc_bad_discarded },
        { "mbm.addr_mismatch_discarded", mbm_addr_mismatch_discarded },
        { "mbm.garbage_resync",         mbm_garbage_resync },
        { "mbm.read_bc_mismatch",       mbm_read_bc_mismatch },
        { "mbm.write_echo_mismatch",    mbm_write_echo_mismatch },
        { "mbm.noise_self_echo",        mbm_noise_self_echo },
        { "mbm.noise_bare_head",        mbm_noise_bare_head },
        { "mbm.noise_odd_bc",           mbm_noise_odd_bc },
        { "mbm.noise_oversize_bc",      mbm_noise_oversize_bc },
        { "mbm.noise_expected_shape",   mbm_noise_expected_shape },
        { "mbm.noise_qty_max_resync",   mbm_noise_qty_max_resync },
        { "mbm.noise_write_resync",     mbm_noise_write_resync },
        { "mbm.qty_bounds_read",        mbm_qty_bounds_read },
        { "mbm.qty_bounds_write",       mbm_qty_bounds_write },
        { "mbm.fc_rejected",            mbm_fc_rejected },
        { "mbm.uninit_reject",          mbm_uninit_reject },
        { "mbm.tx_pending_semantics",   mbm_tx_pending_semantics },
        { "mbm.multi_instance",         mbm_multi_instance },
        { "mbm.stats_accumulate",       mbm_stats_accumulate },
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

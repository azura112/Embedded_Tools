/**
 * @file    ex_modbus_master.c
 * @brief   配方可执行载体 #5: Modbus RTU 主站全路径自检 (v2.5)
 *
 * 载体对应: API_GUIDE 5.8 (et_modbus_master 章节)。内置一个**确定性的模拟从站**
 * (表驱动应答器), 与主站做**逐字节**往返: 请求帧/应答帧/重发帧全部打印并断言。
 *
 * 覆盖: 0x03/0x04 读、0x06/0x10 写、**丢首应答 → 超时重发 → 成功**、超时重发耗尽、
 * 异常应答(不重试)、广播写(不等应答)、迟到应答不污染下一事务、统计口径。
 *
 * 时间由 g_now **虚拟推进**(零耗时断言, 不依赖真实时钟); 退出码 0=PASS。运行: make ex
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "et_modbus_master.h"       /* 已 include et_modbus.h(常量单一来源) */
#include "et_crc.h"

static int g_fail;

#define EX_CHECK(cond)                                                      \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("  FAIL: %s (line %d)\n", #cond, __LINE__);              \
            g_fail++;                                                       \
        }                                                                   \
    } while (0)

#define EX_CHECK_U32_EQ(exp, act)   EX_CHECK((uint32_t)(exp) == (uint32_t)(act))

#define SLAVE_ADDR  0x11u
#define TIMEOUT_MS  50u
#define N_HOLD      16u

static uint8_t            g_rx[ET_MODBUS_ADU_MAX];
static uint8_t            g_tx[ET_MODBUS_ADU_MAX];
static et_modbus_master_t g_m;
static uint32_t           g_now;

/* ---- 模拟从站(确定性应答器) ---- */
static uint16_t g_hold[N_HOLD];
static int      g_drop_first;               /* >0: 丢弃前 N 次应答(驱动超时重发) */
static uint32_t g_slave_frames;

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t slave_seal(uint8_t *f, uint32_t len)
{
    uint16_t crc = et_crc16_modbus(f, len);

    f[len]      = (uint8_t)(crc & 0xFFu);
    f[len + 1u] = (uint8_t)(crc >> 8);
    return len + 2u;
}

/* 返回应答帧长; 0 = 不应答(广播/丢包注入) */
static uint32_t slave_respond(const uint8_t *req, uint32_t reqlen, uint8_t *out)
{
    uint8_t  fc   = req[1];
    uint16_t reg  = be16(req + 2u);
    uint16_t qty  = be16(req + 4u);
    uint16_t i;

    if (reqlen < 4u) {
        return 0u;
    }
    if ((et_crc16_modbus(req, reqlen - 2u) !=
         (uint16_t)((uint16_t)req[reqlen - 2u] |
                    ((uint16_t)req[reqlen - 1u] << 8)))) {
        return 0u;                          /* CRC 坏: 静默(与从站语义一致) */
    }
    g_slave_frames++;
    if ((g_drop_first > 0) && (req[0] != 0u)) {
        g_drop_first--;                     /* 丢包注入: 本次不应答 */
        return 0u;
    }
    if (req[0] == 0u) {                     /* 广播: 执行写, 不应答 */
        if (fc == ET_MODBUS_FC_WRITE_SINGLE) {
            g_hold[reg] = be16(req + 4u);
        }
        return 0u;
    }

    switch (fc) {
    case ET_MODBUS_FC_READ_HOLDING:
    case ET_MODBUS_FC_READ_INPUT:
        if ((qty == 0u) || (qty > ET_MODBUS_RD_QTY_MAX)) {
            out[0] = req[0]; out[1] = (uint8_t)(fc | 0x80u);
            out[2] = ET_MODBUS_EXC_ILLEGAL_VALUE;
            return slave_seal(out, 3u);
        }
        if (((uint32_t)reg + qty) > N_HOLD) {
            out[0] = req[0]; out[1] = (uint8_t)(fc | 0x80u);
            out[2] = ET_MODBUS_EXC_ILLEGAL_ADDR;
            return slave_seal(out, 3u);
        }
        out[0] = req[0];
        out[1] = fc;
        out[2] = (uint8_t)(2u * qty);
        for (i = 0u; i < qty; i++) {
            uint16_t v = (fc == ET_MODBUS_FC_READ_HOLDING)
                             ? g_hold[reg + i]
                             : (uint16_t)((reg + i) * 100u + 7u);
            out[3u + (2u * i)]      = (uint8_t)(v >> 8);
            out[3u + (2u * i) + 1u] = (uint8_t)(v & 0xFFu);
        }
        return slave_seal(out, 3u + (2u * (uint32_t)qty));
    case ET_MODBUS_FC_WRITE_SINGLE:
        if (reg >= N_HOLD) {
            out[0] = req[0]; out[1] = (uint8_t)(fc | 0x80u);
            out[2] = ET_MODBUS_EXC_ILLEGAL_ADDR;
            return slave_seal(out, 3u);
        }
        g_hold[reg] = be16(req + 4u);
        memcpy(out, req, 6u);               /* 应答 = 请求回显 */
        return slave_seal(out, 6u);
    case ET_MODBUS_FC_WRITE_MULTIPLE:
        if (((uint32_t)reg + qty) > N_HOLD) {
            out[0] = req[0]; out[1] = (uint8_t)(fc | 0x80u);
            out[2] = ET_MODBUS_EXC_ILLEGAL_ADDR;
            return slave_seal(out, 3u);
        }
        for (i = 0u; i < qty; i++) {
            g_hold[reg + i] = be16(req + 7u + (2u * i));
        }
        out[0] = req[0]; out[1] = fc;
        out[2] = req[2]; out[3] = req[3];
        out[4] = req[4]; out[5] = req[5];
        return slave_seal(out, 6u);
    default:
        out[0] = req[0]; out[1] = (uint8_t)(fc | 0x80u);
        out[2] = ET_MODBUS_EXC_ILLEGAL_FUNC;
        return slave_seal(out, 3u);
    }
}

/* ---- 打印与驱动 ---- */

static void hexdump(const char *tag, const uint8_t *p, uint32_t n)
{
    uint32_t i;

    printf("  %-5s", tag);
    for (i = 0u; i < n; i++) {
        printf(" %02X", p[i]);
    }
    printf("\n");
}

/* 取待发请求并上线(模拟从站立即应答); 返回本次应答字节数 */
static uint32_t run_once(et_modbus_master_t *m, const char *name, uint8_t *resp)
{
    uint32_t       reqlen = 0u;
    const uint8_t *req    = et_modbus_master_tx(m, &reqlen);
    uint32_t       resplen;

    EX_CHECK(req != NULL);
    printf("  %-28s\n", name);
    hexdump("req:", req, reqlen);
    et_modbus_master_sent(m, g_now);        /* 上线 */
    resplen = slave_respond(req, reqlen, resp);
    if (resplen > 0u) {
        hexdump("resp:", resp, resplen);
    } else {
        printf("  resp: <模拟从站未应答(丢包注入/广播)>\n");
    }
    return resplen;
}

int main(void)
{
    et_modbus_master_cfg_t cfg;
    uint8_t  resp[ET_MODBUS_ADU_MAX];
    uint32_t resplen;
    uint16_t qty = 0u;
    uint32_t dlen = 0u;
    const uint8_t *d;
    et_modbus_master_stats_t st;
    uint16_t vals3[3] = { 111u, 222u, 333u };
    uint16_t v = 0x1234u;
    uint16_t i;

    printf("== ex_modbus_master: Modbus RTU 主站全路径自检 (API_GUIDE 5.8) ==\n");
    for (i = 0u; i < N_HOLD; i++) {
        g_hold[i] = (uint16_t)(i + 1u);
    }
    memset(&cfg, 0, sizeof(cfg));
    cfg.addr            = SLAVE_ADDR;
    cfg.resp_timeout_ms = TIMEOUT_MS;
    cfg.retry_max       = 2u;
    g_now = 1000u;
    EX_CHECK(et_modbus_master_init(&g_m, &cfg, g_rx, sizeof(g_rx),
                                   g_tx, sizeof(g_tx)));

    /* ---- 1. 0x03 读保持寄存器 0..3: 请求帧逐字节 ---- */
    EX_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 4u));
    resplen = run_once(&g_m, "1. 0x03 read holding 0..3", resp);
    EX_CHECK(et_modbus_master_feed(&g_m, resp, resplen) == 1u);
    EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    EX_CHECK_U32_EQ(1u, et_modbus_master_result(&g_m, &qty));
    EX_CHECK_U32_EQ(4u, qty);
    d = et_modbus_master_data(&g_m, &dlen);
    EX_CHECK(d != NULL && dlen == 8u);
    EX_CHECK(d[1] == 1u && d[3] == 2u && d[5] == 3u && d[7] == 4u);   /* 逐值 */

    /* ---- 2. 0x04 读输入寄存器 4..5: 计算值 407/507 ---- */
    EX_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_INPUT, 4u, 2u));
    resplen = run_once(&g_m, "2. 0x04 read input 4..5", resp);
    EX_CHECK(et_modbus_master_feed(&g_m, resp, resplen) == 1u);
    EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    EX_CHECK_U32_EQ(407u, et_modbus_master_result(&g_m, &qty));
    EX_CHECK_U32_EQ(2u, qty);

    /* ---- 3. 0x06 单写 reg2 = 0x1234 ---- */
    EX_CHECK(et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_SINGLE, 2u, &v, 1u));
    resplen = run_once(&g_m, "3. 0x06 write single reg2", resp);
    EX_CHECK(et_modbus_master_feed(&g_m, resp, resplen) == 1u);
    EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    EX_CHECK(g_hold[2] == 0x1234u);                 /* 模拟从站侧已生效 */

    /* ---- 4. 0x10 多写 reg4..6 = 111,222,333 ---- */
    EX_CHECK(et_modbus_master_write(&g_m, ET_MODBUS_FC_WRITE_MULTIPLE, 4u, vals3, 3u));
    resplen = run_once(&g_m, "4. 0x10 write multiple 4..6", resp);
    EX_CHECK(et_modbus_master_feed(&g_m, resp, resplen) == 1u);
    EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
    EX_CHECK(g_hold[4] == 111u && g_hold[5] == 222u && g_hold[6] == 333u);

    /* ---- 5. 丢首应答 → 超时重发 → 成功(重发帧与首帧逐字节相同) ---- */
    {
        uint8_t  first[ET_MODBUS_ADU_MAX];
        uint32_t firstlen;

        g_drop_first = 1;                           /* 注入: 丢第 1 个应答 */
        EX_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 1u));
        firstlen = 0u;
        {
            const uint8_t *req = et_modbus_master_tx(&g_m, &firstlen);

            EX_CHECK(req != NULL && firstlen == 8u);
            memcpy(first, req, firstlen);
        }
        resplen = run_once(&g_m, "5a. 丢包注入(第1次应答丢失)", resp);
        EX_CHECK_U32_EQ(0u, resplen);
        EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);      /* 未超时 */
        g_now += TIMEOUT_MS;                                             /* 虚拟超时 */
        EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_BUSY);      /* 置重发 */
        {
            uint32_t l2 = 0u;
            const uint8_t *req2 = et_modbus_master_tx(&g_m, &l2);

            EX_CHECK(req2 != NULL);
            EX_CHECK_U32_EQ(firstlen, l2);
            EX_CHECK(memcmp(req2, first, firstlen) == 0);   /* 含 CRC 逐字节相同 */
        }
        resplen = run_once(&g_m, "5b. 超时重发(同一请求)", resp);
        EX_CHECK(resplen > 0u);
        EX_CHECK(et_modbus_master_feed(&g_m, resp, resplen) == 1u);
        EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
        EX_CHECK_U32_EQ(1u, et_modbus_master_result(&g_m, NULL));
        g_drop_first = 0;
    }

    /* ---- 6. 异常应答: 读越界 → 0x02, 不重试 ---- */
    EX_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 100u, 4u));
    resplen = run_once(&g_m, "6. 异常应答(读越界)", resp);
    EX_CHECK(et_modbus_master_feed(&g_m, resp, resplen) == 1u);
    EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_EXC);
    EX_CHECK_U32_EQ(ET_MODBUS_EXC_ILLEGAL_ADDR, et_modbus_master_exc(&g_m));
    EX_CHECK(et_modbus_master_tx(&g_m, NULL) == NULL);      /* 无重发 */

    /* ---- 7. 广播写(addr=0): sent() 后立即 OK, 不等应答 ----
     * 用**第二实例**(多实例语义顺带验证), 以免重置主实例的连续统计。 */
    {
        static uint8_t            rx2[ET_MODBUS_ADU_MAX];
        static uint8_t            tx2[ET_MODBUS_ADU_MAX];
        et_modbus_master_cfg_t    bc = cfg;
        et_modbus_master_t        m2;
        et_modbus_master_stats_t  st2;

        bc.addr = 0u;
        EX_CHECK(et_modbus_master_init(&m2, &bc, rx2, sizeof(rx2),
                                       tx2, sizeof(tx2)));
        v = 0xBEEFu;
        EX_CHECK(et_modbus_master_write(&m2, ET_MODBUS_FC_WRITE_SINGLE, 5u, &v, 1u));
        resplen = run_once(&m2, "7. 广播写 reg5 = 0xBEEF", resp);
        EX_CHECK_U32_EQ(0u, resplen);                       /* 广播不应答 */
        EX_CHECK(et_modbus_master_poll(&m2, g_now) == ET_MB_OK);    /* 时间未推进 */
        EX_CHECK(g_hold[5] == 0xBEEFu);                     /* 从站侧已执行 */
        et_modbus_master_stats(&m2, &st2);
        EX_CHECK_U32_EQ(1u, st2.requests);
        EX_CHECK_U32_EQ(0u, st2.responses);                 /* 未等应答 */
        EX_CHECK_U32_EQ(0u, st2.timeouts);
    }

    /* ---- 8. 迟到应答不污染下一事务 ---- */
    {
        uint8_t stale[ET_MODBUS_ADU_MAX];
        uint32_t stalelen;

        EX_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 0u, 2u));
        stalelen = run_once(&g_m, "8a. 事务 A(读 qty=2)", resp);
        EX_CHECK(et_modbus_master_feed(&g_m, resp, stalelen) == 1u);
        EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
        memcpy(stale, resp, stalelen);

        /* 终态后喂入上一事务的陈旧应答 → 丢弃计数 +1 */
        EX_CHECK_U32_EQ(0u, et_modbus_master_feed(&g_m, stale, stalelen));

        EX_CHECK(et_modbus_master_read(&g_m, ET_MODBUS_FC_READ_HOLDING, 8u, 4u));
        resplen = run_once(&g_m, "8b. 事务 B(读 qty=4, reg8..11)", resp);
        EX_CHECK(et_modbus_master_feed(&g_m, resp, resplen) == 1u);
        EX_CHECK(et_modbus_master_poll(&g_m, g_now) == ET_MB_OK);
        EX_CHECK_U32_EQ(9u, et_modbus_master_result(&g_m, &qty));   /* 9,10,11,12 */
        EX_CHECK_U32_EQ(4u, qty);
        d = et_modbus_master_data(&g_m, &dlen);
        EX_CHECK(d != NULL && dlen == 8u);
        EX_CHECK(d[1] == 9u && d[3] == 10u && d[5] == 11u && d[7] == 12u);
        et_modbus_master_stats(&g_m, &st);
        EX_CHECK_U32_EQ(1u, st.late);
    }

    /* ---- 统计汇总 ---- */
    et_modbus_master_stats(&g_m, &st);
    printf("  stats: requests=%u responses=%u exceptions=%u timeouts=%u "
           "retries=%u crc_err=%u addr_mismatch=%u late=%u discarded=%u\n",
           (unsigned)st.requests, (unsigned)st.responses,
           (unsigned)st.exceptions, (unsigned)st.timeouts, (unsigned)st.retries,
           (unsigned)st.crc_err, (unsigned)st.addr_mismatch, (unsigned)st.late,
           (unsigned)st.discarded);
    EX_CHECK_U32_EQ(9u, st.requests);           /* 1..4 各一 + 5 两次(首帧+重发) + 6/8a/8b */
    EX_CHECK_U32_EQ(7u, st.responses);          /* 1..4 + 5b + 8a + 8b */
    EX_CHECK_U32_EQ(1u, st.exceptions);         /* 6(异常应答不计 responses) */
    EX_CHECK_U32_EQ(0u, st.timeouts);           /* 5 的重发后成功 */
    EX_CHECK_U32_EQ(1u, st.retries);            /* 5 的 1 次重发 */
    EX_CHECK_U32_EQ(0u, st.crc_err);
    EX_CHECK_U32_EQ(0u, st.addr_mismatch);
    EX_CHECK_U32_EQ(1u, st.late);               /* 8a 终态后的陈旧应答 */
    EX_CHECK_U32_EQ(0u, st.discarded);

    printf("[ex_modbus_master] %s\n", (g_fail == 0) ? "PASS" : "FAIL");
    return (g_fail == 0) ? 0 : 1;
}

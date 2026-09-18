/**
 * @file    ex_modbus_slave.c
 * @brief   配方可执行载体 #4: Modbus RTU 从站全路径自检 (v2.4)
 *
 * 载体对应: API_GUIDE 5.7 (et_modbus 章节)。脚本化"主站"帧逐条喂入从站,
 * 应答与期望**逐字节比对**——覆盖 0x03/0x04 读、0x06/0x10 写、异常 0x01/0x02/0x03、
 * CRC 坏帧静默、地址不符静默、广播写不应答、分片喂入、粘包两帧、静默丢弃。
 *
 * 寄存器映射示例: 8 个保持寄存器(可读写) + 输入寄存器(计算型只读)——值域语义
 * 由应用钩子决定(库只管协议与访问控制; kv 直通配方见 API_GUIDE 11.13)。
 *
 * 自检全部为**确定性字节序列**(零耗时断言); 退出码 0=PASS。运行: make ex
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "et_modbus.h"
#include "et_crc.h"

static int g_fail;

#define EX_CHECK(cond)                                                      \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("  FAIL: %s (line %d)\n", #cond, __LINE__);              \
            g_fail++;                                                       \
        }                                                                   \
    } while (0)

/* 本地等价断言宏(示例不依赖 test/ 框架) */
#define EX_CHECK_U32_EQ(exp, act)                                               EX_CHECK((uint32_t)(exp) == (uint32_t)(act))

#define SLAVE       0x11u
#define SILENCE_MS  5u
#define N_HOLD      8u

static uint8_t     g_rx[64];
static uint8_t     g_tx[256];
static et_modbus_t g_mb;
static uint16_t    g_hold[N_HOLD];
static int         g_step;

/* ---- 应用侧寄存器映射 ---- */
static uint8_t rd_cb(void *user, uint8_t func, uint16_t addr, uint16_t qty,
                     uint8_t *dst)
{
    uint16_t i;

    (void)user;
    if (((uint32_t)addr + qty) > N_HOLD) {
        return ET_MODBUS_EXC_ILLEGAL_ADDR;
    }
    for (i = 0u; i < qty; i++) {
        uint16_t v = (func == ET_MODBUS_FC_READ_HOLDING)
                         ? g_hold[addr + i]
                         : (uint16_t)((addr + i) * 100u + 7u);
        dst[2u * i]      = (uint8_t)(v >> 8);
        dst[2u * i + 1u] = (uint8_t)(v & 0xFFu);
    }
    return 0u;
}

static uint8_t wr_cb(void *user, uint8_t func, uint16_t addr, uint16_t qty,
                     const uint8_t *src)
{
    uint16_t i;

    (void)user;
    (void)func;
    if (((uint32_t)addr + qty) > N_HOLD) {
        return ET_MODBUS_EXC_ILLEGAL_ADDR;
    }
    for (i = 0u; i < qty; i++) {
        g_hold[addr + i] = (uint16_t)(((uint16_t)src[2u * i] << 8) |
                                      (uint16_t)src[2u * i + 1u]);
    }
    return 0u;
}

/* ---- 主站侧组帧(CRC16-MODBUS, 线上低字节在前) ---- */
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
    f[2u + plen] = (uint8_t)(crc & 0xFFu);
    f[3u + plen] = (uint8_t)(crc >> 8);
    return 4u + plen;
}

/* 构造异常应答期望: [addr, func|0x80, exc, crc] */
static uint32_t mk_exc(uint8_t *f, uint8_t addr, uint8_t func, uint8_t exc)
{
    f[0] = addr;
    f[1] = (uint8_t)(func | 0x80u);
    f[2] = exc;
    return 4u;                              /* 见下方 seal */
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static void hexdump(const char *tag, const uint8_t *p, uint32_t n)
{
    uint32_t i;

    printf("  %s", tag);
    for (i = 0u; i < n; i++) {
        printf(" %02X", p[i]);
    }
    printf("\n");
}

/* 喂入一帧并逐字节比对应答; explen==0 表示期望无应答 */
static void step(const char *name, const uint8_t *req, uint32_t reqlen,
                 const uint8_t *exp, uint32_t explen)
{
    const uint8_t *r;
    uint32_t       len = 0u;
    uint32_t       n;

    g_step++;
    n = et_modbus_feed(&g_mb, req, reqlen);
    r = et_modbus_response(&g_mb, &len);
    printf("  [%02d] %-30s req=%2u resp=%2u\n", g_step, name,
           (unsigned)reqlen, (unsigned)len);
    hexdump("req :", req, reqlen);
    if (len > 0u) {
        hexdump("resp:", r, len);
    }
    EX_CHECK_U32_EQ((explen > 0u) ? 1u : 0u, n);
    EX_CHECK_U32_EQ(explen, len);
    if ((explen > 0u) && (len > 0u)) {
        EX_CHECK(memcmp(r, exp, explen) == 0);
    }
}

int main(void)
{
    et_modbus_cfg_t cfg;
    et_modbus_stats_t st;
    uint8_t  req[300];
    uint8_t  pay[300];
    uint8_t  exp[300];
    uint32_t i;
    uint16_t crc;

    printf("== ex_modbus_slave: Modbus RTU 从站全路径自检 (API_GUIDE 5.7) ==\n");
    for (i = 0u; i < N_HOLD; i++) {
        g_hold[i] = (uint16_t)(i + 1u);
    }
    memset(&cfg, 0, sizeof(cfg));
    cfg.slave_addr = SLAVE;
    cfg.silence_ms = SILENCE_MS;
    cfg.rd = rd_cb;
    cfg.wr = wr_cb;
    EX_CHECK(et_modbus_init(&g_mb, &cfg, g_rx, sizeof(g_rx),
                            g_tx, sizeof(g_tx)));

    /* ---- 1. 0x03 读保持寄存器 [0,4): 初值 1,2,3,4 ---- */
    put16(pay, 0u); put16(pay + 2u, 4u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    exp[0] = SLAVE; exp[1] = ET_MODBUS_FC_READ_HOLDING; exp[2] = 8u;
    put16(exp + 3u, 1u); put16(exp + 5u, 2u);
    put16(exp + 7u, 3u); put16(exp + 9u, 4u);
    crc = et_crc16_modbus(exp, 11u);
    exp[11] = (uint8_t)(crc & 0xFFu); exp[12] = (uint8_t)(crc >> 8);
    step("0x03 read holding 0..3", req, 8u, exp, 13u);

    /* ---- 2. 0x04 读输入寄存器 [4,6): 计算值 407,507 ---- */
    put16(pay, 4u); put16(pay + 2u, 2u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_INPUT, pay, 4u);
    exp[0] = SLAVE; exp[1] = ET_MODBUS_FC_READ_INPUT; exp[2] = 4u;
    put16(exp + 3u, 407u); put16(exp + 5u, 507u);
    crc = et_crc16_modbus(exp, 7u);
    exp[7] = (uint8_t)(crc & 0xFFu); exp[8] = (uint8_t)(crc >> 8);
    step("0x04 read input 4..5", req, 8u, exp, 9u);

    /* ---- 3. 0x06 单写 reg2 = 0x1234: 应答回显 ---- */
    put16(pay, 2u); put16(pay + 2u, 0x1234u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_WRITE_SINGLE, pay, 4u);
    step("0x06 write single reg2", req, 8u, req, 8u);
    EX_CHECK(g_hold[2] == 0x1234u);
    /* 读回验证 */
    put16(pay, 2u); put16(pay + 2u, 1u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    exp[0] = SLAVE; exp[1] = ET_MODBUS_FC_READ_HOLDING; exp[2] = 2u;
    put16(exp + 3u, 0x1234u);
    crc = et_crc16_modbus(exp, 5u);
    exp[5] = (uint8_t)(crc & 0xFFu); exp[6] = (uint8_t)(crc >> 8);
    step("0x03 read back reg2", req, 8u, exp, 7u);

    /* ---- 4. 0x10 多写 reg1..3 = 0x0011,0x0022,0x0033 ---- */
    put16(pay, 1u); put16(pay + 2u, 3u); pay[4] = 6u;
    put16(pay + 5u, 0x0011u); put16(pay + 7u, 0x0022u); put16(pay + 9u, 0x0033u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_WRITE_MULTIPLE, pay, 11u);
    exp[0] = SLAVE; exp[1] = ET_MODBUS_FC_WRITE_MULTIPLE;
    put16(exp + 2u, 1u); put16(exp + 4u, 3u);
    crc = et_crc16_modbus(exp, 6u);
    exp[6] = (uint8_t)(crc & 0xFFu); exp[7] = (uint8_t)(crc >> 8);
    step("0x10 write multiple 1..3", req, 15u, exp, 8u);
    EX_CHECK((g_hold[1] == 0x0011u) && (g_hold[2] == 0x0022u) &&
             (g_hold[3] == 0x0033u));

    /* ---- 5. 异常 0x01: 未知功能码(静默路径界定) ----
     * v2.5 CO-8 守护: 应答有**两条产生路径**(feed 快路径 / tick 静默路径),
     * 调用侧两处都要复查并发送。本段显式建模两个 flush 点:
     *   flush 点① (feed 之后) 必须无应答; flush 点② (tick 之后) 必须拿到异常帧。
     * v2.4 板上正是漏了 flush 点② → 现象"0x63 请求无任何应答"。 */
    req[0] = SLAVE; req[1] = 0x63u;
    crc = et_crc16_modbus(req, 2u);
    req[2] = (uint8_t)(crc & 0xFFu); req[3] = (uint8_t)(crc >> 8);
    g_step++;
    printf("  [%02d] %-30s (silence-delimited, 2 flush points)\n", g_step,
           "0x63 illegal function");
    EX_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 4u));
    {
        uint32_t len = 0u;

        /* flush 点①: feed 快路径未产生应答(未知功能码只能靠静默界定) */
        EX_CHECK(et_modbus_response(&g_mb, &len) == NULL);
        EX_CHECK_U32_EQ(0u, len);
    }
    et_modbus_tick(&g_mb, 0u);
    et_modbus_tick(&g_mb, SILENCE_MS + 1u);
    (void)mk_exc(exp, SLAVE, 0x63u, ET_MODBUS_EXC_ILLEGAL_FUNC);
    crc = et_crc16_modbus(exp, 3u);
    exp[3] = (uint8_t)(crc & 0xFFu); exp[4] = (uint8_t)(crc >> 8);
    {
        uint32_t len = 0u;
        const uint8_t *r = et_modbus_response(&g_mb, &len);   /* flush 点② */

        EX_CHECK(r != NULL);                    /* tick 静默路径产生了应答 */
        EX_CHECK_U32_EQ(5u, len);
        EX_CHECK(r != NULL && memcmp(r, exp, 5u) == 0);
        hexdump("resp:", r, len);
    }

    /* ---- 6. 异常 0x02: 地址越界(6+4 > 8) ---- */
    put16(pay, 6u); put16(pay + 2u, 4u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    (void)mk_exc(exp, SLAVE, ET_MODBUS_FC_READ_HOLDING, ET_MODBUS_EXC_ILLEGAL_ADDR);
    crc = et_crc16_modbus(exp, 3u);
    exp[3] = (uint8_t)(crc & 0xFFu); exp[4] = (uint8_t)(crc >> 8);
    step("exc 0x02 addr out of range", req, 8u, exp, 5u);

    /* ---- 7. 异常 0x03: qty=0 ---- */
    put16(pay, 0u); put16(pay + 2u, 0u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    (void)mk_exc(exp, SLAVE, ET_MODBUS_FC_READ_HOLDING, ET_MODBUS_EXC_ILLEGAL_VALUE);
    crc = et_crc16_modbus(exp, 3u);
    exp[3] = (uint8_t)(crc & 0xFFu); exp[4] = (uint8_t)(crc >> 8);
    step("exc 0x03 qty=0", req, 8u, exp, 5u);

    /* ---- 8. 广播写(地址 0): 执行且不应答 ---- */
    put16(pay, 5u); put16(pay + 2u, 0xBEEFu);
    (void)mk(req, 0u, ET_MODBUS_FC_WRITE_SINGLE, pay, 4u);
    step("broadcast 0x06 (no reply)", req, 8u, NULL, 0u);
    EX_CHECK(g_hold[5] == 0xBEEFu);

    /* ---- 9. 广播读: 忽略(不应答) ---- */
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, 0u, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    step("broadcast 0x03 (ignored)", req, 8u, NULL, 0u);

    /* ---- 10. CRC 坏帧: 静默 ---- */
    put16(pay, 0u); put16(pay + 2u, 1u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    req[6] ^= 0xFFu;
    step("bad CRC (silent)", req, 8u, NULL, 0u);

    /* ---- 11. 地址不符: 静默 ---- */
    (void)mk(req, 0x22u, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    step("addr mismatch (silent)", req, 8u, NULL, 0u);

    /* ---- 12. 分片喂入: 3+3+2 ---- */
    put16(pay, 0u); put16(pay + 2u, 2u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    g_step++;
    printf("  [%02d] %-30s (3+3+2 chunks)\n", g_step, "fragmented feed");
    EX_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 3u));
    EX_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req + 3u, 3u));
    EX_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req + 6u, 2u));
    {
        uint32_t len = 0u;

        EX_CHECK(et_modbus_response(&g_mb, &len) != NULL);
        EX_CHECK_U32_EQ(9u, len);
    }

    /* ---- 13. 粘包两帧: 一次喂入 → 第 1 应答; feed(NULL,0) → 第 2 应答 ---- */
    {
        uint8_t  two[16];
        uint8_t  p2[4];
        uint32_t len = 0u;

        put16(p2, 0u); put16(p2 + 2u, 1u);
        (void)mk(two, SLAVE, ET_MODBUS_FC_READ_HOLDING, p2, 4u);
        put16(p2, 1u); put16(p2 + 2u, 1u);
        (void)mk(two + 8u, SLAVE, ET_MODBUS_FC_READ_HOLDING, p2, 4u);
        g_step++;
        printf("  [%02d] %-30s (2 frames in one feed)\n", g_step, "sticky frames");
        EX_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, two, 16u));
        EX_CHECK(et_modbus_response(&g_mb, &len) != NULL);
        EX_CHECK_U32_EQ(7u, len);
        EX_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, NULL, 0u));   /* 排空后续帧 */
        EX_CHECK(et_modbus_response(&g_mb, &len) != NULL);
        EX_CHECK_U32_EQ(7u, len);
    }

    /* ---- 14. 静默丢弃残帧后重新同步 ---- */
    put16(pay, 0u); put16(pay + 2u, 3u);
    (void)mk(req, SLAVE, ET_MODBUS_FC_READ_HOLDING, pay, 4u);
    g_step++;
    printf("  [%02d] %-30s (partial + silence)\n", g_step, "tick discards partial");
    EX_CHECK_U32_EQ(0u, et_modbus_feed(&g_mb, req, 3u));
    et_modbus_tick(&g_mb, 0u);
    et_modbus_tick(&g_mb, SILENCE_MS + 1u);
    EX_CHECK_U32_EQ(1u, et_modbus_feed(&g_mb, req, 8u));         /* 重新同步成功 */

    /* ---- 统计汇总 ---- */
    et_modbus_stats(&g_mb, &st);
    printf("  stats: frames=%u responses=%u exceptions=%u crc_err=%u "
           "addr_mismatch=%u discarded=%u\n",
           (unsigned)st.frames, (unsigned)st.responses,
           (unsigned)st.exceptions, (unsigned)st.crc_err,
           (unsigned)st.addr_mismatch, (unsigned)st.discarded);
    EX_CHECK_U32_EQ(3u, st.exceptions);         /* 0x01 + 0x02 + 0x03 各一 */
    EX_CHECK_U32_EQ(1u, st.crc_err);
    EX_CHECK_U32_EQ(1u, st.addr_mismatch);
    EX_CHECK_U32_EQ(1u, st.discarded);          /* 半截帧静默丢弃 */

    printf("[ex_modbus_slave] %s\n", (g_fail == 0) ? "PASS" : "FAIL");
    return (g_fail == 0) ? 0 : 1;
}

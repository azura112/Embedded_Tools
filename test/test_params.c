/**
 * @file    test_params.c
 * @brief   et_params 单元测试 (host 模拟 flash + 字节级掉电注入)
 *
 * 覆盖: schema 校验 / 默认值 / 往返持久化 / 范围与类型拒绝 / 代际切换 /
 *       逐版迁移 (成功/失败回滚) / 恢复出厂保留 keep 字段 / 100 点掉电注入。
 */
#include "et_test.h"
#include "et_params.h"
#include "et_kv.h"
#include "port.h"
#include "port_host.h"
#include <string.h>

enum {
    F_MODE = 0,
    F_THRESH,
    F_BAUD,
    F_OFFSET,
    F_DEV_ID,
    F_CAL,
    F_ADDED,        /* v2 新增字段 */
    F_COUNT
};

static const uint8_t  def_mode   = 1u;
static const uint16_t def_thresh = 100u;
static const uint32_t def_baud   = 115200u;
static const int32_t  def_offset = 0;
static const uint8_t  def_dev[6] = { 'E', 'T', 0u, 0u, 0u, 1u };
static const uint8_t  def_cal[4] = { 0x11u, 0x22u, 0x33u, 0x44u };
static const uint16_t def_added  = 7u;

/* 描述符: {type, len, group, keep, since_ver, min, max, def, def_len, name} */
static const et_param_desc_t g_desc[F_COUNT] = {
    { ET_PARAM_U8,   0u, 1u, false, 1u, 0,      2,      &def_mode,   1u, "mode"   },
    { ET_PARAM_U16,  0u, 1u, false, 1u, 0,      1000,   &def_thresh, 2u, "thresh" },
    { ET_PARAM_U32,  0u, 2u, false, 1u, 1000,   921600, &def_baud,   4u, "baud"   },
    { ET_PARAM_I32,  0u, 2u, false, 1u, -1000,  1000,   &def_offset, 4u, "offset" },
    { ET_PARAM_BLOB, 6u, 3u, true,  1u, 0,      0,      def_dev,     6u, "dev_id" },
    { ET_PARAM_BLOB, 4u, 3u, false, 1u, 0,      0,      def_cal,     4u, "cal"    },
    { ET_PARAM_U16,  0u, 1u, false, 2u, 0,      65535,  &def_added,  2u, "added"  },
};

static et_kv_t        g_kv;
static et_kv_layout_t g_layout = { 0u, 1u };
static et_params_t    g_p;
static uint8_t        g_work[ET_PARAMS_WORK_SIZE(F_COUNT)];

static bool g_mig_fail = false;

static bool mig_1_2(uint16_t from, uint16_t to, uint16_t field,
                    const et_param_desc_t *d, const void *old_val, uint16_t old_len,
                    void *new_val, uint16_t *new_len, void *ctx)
{
    (void)from; (void)to; (void)d; (void)ctx;
    if (g_mig_fail) {
        return false;
    }
    if (field == F_THRESH) {            /* v1 单位 -> v2 单位: ×10 */
        uint16_t v = (uint16_t)((const uint8_t *)old_val)[0];
        v = (uint16_t)(v | ((uint16_t)((const uint8_t *)old_val)[1] << 8));
        v = (uint16_t)(v * 10u);
        ((uint8_t *)new_val)[0] = (uint8_t)(v & 0xFFu);
        ((uint8_t *)new_val)[1] = (uint8_t)((v >> 8) & 0xFFu);
        *new_len = 2u;
        return true;
    }
    memcpy(new_val, old_val, old_len);
    *new_len = old_len;
    return true;
}

static et_params_schema_t g_schema = { g_desc, F_COUNT, 2u, mig_1_2, NULL };
static et_params_schema_t g_schema_v1 = { g_desc, F_COUNT - 1u, 1u, NULL, NULL };

static void p_fresh(void)
{
    port_host_flash_reset();
    ET_CHECK(et_kv_format(&g_kv, &g_layout));
    ET_CHECK(et_params_init(&g_p, &g_kv, &g_schema, g_work, sizeof(g_work)));
}

static void p_reopen(void)
{
    ET_CHECK(et_kv_init(&g_kv, &g_layout));
    ET_CHECK(et_params_init(&g_p, &g_kv, &g_schema, g_work, sizeof(g_work)));
}

static uint16_t get_u16(uint16_t f)
{
    uint16_t v = 0u, len = 0u;
    ET_CHECK(et_params_get(&g_p, f, &v, sizeof(v), &len));
    ET_CHECK(len == 2u);
    return v;
}

static uint8_t get_u8(uint16_t f)
{
    uint8_t v = 0u;
    uint16_t len = 0u;
    ET_CHECK(et_params_get(&g_p, f, &v, sizeof(v), &len));
    ET_CHECK(len == 1u);
    return v;
}

static uint32_t get_u32(uint16_t f)
{
    uint32_t v = 0u;
    uint16_t len = 0u;
    ET_CHECK(et_params_get(&g_p, f, &v, sizeof(v), &len));
    ET_CHECK(len == 4u);
    return v;
}

static void get_blob(uint16_t f, uint8_t *buf, uint16_t n)
{
    uint16_t len = 0u;
    ET_CHECK(et_params_get(&g_p, f, buf, n, &len));
    ET_CHECK(len == n);
}

static void set_u8(uint16_t f, uint8_t v)   { ET_CHECK(et_params_set(&g_p, f, &v, 1u)); }
static void set_u16(uint16_t f, uint16_t v) { ET_CHECK(et_params_set(&g_p, f, &v, 2u)); }
static void set_u32(uint16_t f, uint32_t v) { ET_CHECK(et_params_set(&g_p, f, &v, 4u)); }
static void set_i32(uint16_t f, int32_t v)  { ET_CHECK(et_params_set(&g_p, f, &v, 4u)); }

static void check_set_a(void)
{
    ET_CHECK_U32_EQ(1u, get_u8(F_MODE));
    ET_CHECK_U32_EQ(111u, get_u16(F_THRESH));
    ET_CHECK_U32_EQ(111111u, get_u32(F_BAUD));
    ET_CHECK_U32_EQ((uint32_t)(-111), (uint32_t)get_u32(F_OFFSET));
}

static void apply_set_a(void)
{
    set_u8(F_MODE, 1u);
    set_u16(F_THRESH, 111u);
    set_u32(F_BAUD, 111111u);
    set_i32(F_OFFSET, -111);
    ET_CHECK(et_params_set(&g_p, F_DEV_ID, "\xA1\xA2\xA3\xA4\xA5\xA6", 6u));
    ET_CHECK(et_params_set(&g_p, F_CAL, "\x01\x02\x03\x04", 4u));
    set_u16(F_ADDED, 1111u);
}

static void apply_set_b(void)
{
    set_u8(F_MODE, 2u);
    set_u16(F_THRESH, 222u);
    set_u32(F_BAUD, 222222u);
    set_i32(F_OFFSET, -222);
    ET_CHECK(et_params_set(&g_p, F_DEV_ID, "\xB1\xB2\xB3\xB4\xB5\xB6", 6u));
    ET_CHECK(et_params_set(&g_p, F_CAL, "\x0A\x0B\x0C\x0D", 4u));
    set_u16(F_ADDED, 2222u);
}

/* 全部 A 组 或 全部 B 组 (不得混套) */
static void check_all_a_or_b(void)
{
    uint16_t mode = get_u8(F_MODE);
    uint16_t th = get_u16(F_THRESH);
    uint32_t baud = get_u32(F_BAUD);
    uint32_t off = get_u32(F_OFFSET);
    uint16_t added = get_u16(F_ADDED);
    uint8_t dev[6];

    get_blob(F_DEV_ID, dev, 6u);
    if ((mode == 1u) && (th == 111u) && (baud == 111111u) &&
        (off == (uint32_t)(-111)) && (added == 1111u) && (dev[0] == 0xA1u)) {
        return;
    }
    ET_CHECK(mode == 2u);
    ET_CHECK(th == 222u);
    ET_CHECK(baud == 222222u);
    ET_CHECK(off == (uint32_t)(-222));
    ET_CHECK(added == 2222u);
    ET_CHECK(dev[0] == 0xB1u);
}

/* ===================== 用例 ===================== */

static void params_defaults_on_empty(void)
{
    uint8_t dev[6];

    p_fresh();
    ET_CHECK(!g_p.loaded);
    ET_CHECK_U32_EQ(0u, et_params_loaded_version(&g_p));
    ET_CHECK_U32_EQ(F_COUNT, et_params_validate(&g_p));
    ET_CHECK_U32_EQ(1u, get_u8(F_MODE));
    ET_CHECK_U32_EQ(100u, get_u16(F_THRESH));
    ET_CHECK_U32_EQ(115200u, get_u32(F_BAUD));
    ET_CHECK_U32_EQ(0u, get_u32(F_OFFSET));
    ET_CHECK_U32_EQ(7u, get_u16(F_ADDED));
    get_blob(F_DEV_ID, dev, 6u);
    ET_CHECK(dev[0] == 'E');
}

static void params_save_load_roundtrip(void)
{
    uint8_t dev[6];

    p_fresh();
    apply_set_a();
    ET_CHECK(et_params_save(&g_p));
    ET_CHECK(g_p.loaded);
    ET_CHECK_U32_EQ(2u, et_params_loaded_version(&g_p));
    p_reopen();
    ET_CHECK(g_p.loaded);
    ET_CHECK_U32_EQ(2u, et_params_loaded_version(&g_p));
    check_set_a();
    ET_CHECK_U32_EQ(1111u, get_u16(F_ADDED));
    get_blob(F_DEV_ID, dev, 6u);
    ET_CHECK(dev[5] == 0xA6u);
}

static void params_set_validation(void)
{
    p_fresh();
    ET_CHECK(!et_params_set(&g_p, F_MODE, "\x03", 1u));       /* 越上界 */
    ET_CHECK(!et_params_set(&g_p, F_THRESH, "\xE9\x03", 2u)); /* 1001 */
    ET_CHECK(!et_params_set(&g_p, F_BAUD, "\xE7\x03\x00\x00", 4u)); /* 999 */
    ET_CHECK(!et_params_set(&g_p, F_OFFSET, "\xE9\x03\x00\x00", 4u)); /* 1001 */
    ET_CHECK(!et_params_set(&g_p, F_DEV_ID, "abcde", 5u));    /* blob 长度不符 */
    ET_CHECK(!et_params_set(&g_p, F_COUNT, "\x01", 1u));      /* 越界字段 */
    ET_CHECK_U32_EQ(1u, get_u8(F_MODE));                     /* 拒绝后原值不变 */
    ET_CHECK_U32_EQ(F_COUNT, et_params_validate(&g_p));
}

static void params_gen_switch(void)
{
    uint8_t gen[3];
    uint16_t glen = 0u;
    uint16_t g1;
    uint16_t i;

    p_fresh();
    apply_set_a();
    ET_CHECK(et_params_save(&g_p));
    ET_CHECK(et_kv_get(&g_kv, ET_PARAMS_KEY_GEN, gen, sizeof(gen), &glen));
    ET_CHECK(glen == 3u);
    g1 = gen[0];
    ET_CHECK(g1 <= 1u);
    apply_set_b();
    ET_CHECK(et_params_save(&g_p));
    ET_CHECK(et_kv_get(&g_kv, ET_PARAMS_KEY_GEN, gen, sizeof(gen), &glen));
    ET_CHECK(glen == 3u);
    ET_CHECK(gen[0] == (uint8_t)(1u - g1));        /* 代际交替 */
    for (i = 0u; i < F_COUNT; i++) {               /* 旧代际记录仍在 (可回退) */
        uint8_t tmp[ET_PARAMS_VAL_MAX];
        uint16_t got = 0u;
        ET_CHECK(et_kv_get(&g_kv,
                           (uint16_t)(ET_PARAMS_KEY_BASE + (uint16_t)g1 * ET_PARAMS_KEY_STRIDE + i),
                           tmp, (uint16_t)sizeof(tmp), &got));
        ET_CHECK(got > 0u);
    }
}

static void params_migrate_v1_to_v2(void)
{
    p_fresh();
    ET_CHECK(et_params_init(&g_p, &g_kv, &g_schema_v1, g_work, sizeof(g_work)));
    set_u16(F_THRESH, 5u);
    set_u32(F_BAUD, 9600u);
    ET_CHECK(et_params_save(&g_p));
    ET_CHECK_U32_EQ(1u, et_params_loaded_version(&g_p));

    p_reopen();                                     /* 以 v2 schema 加载 v1 数据 */
    ET_CHECK(g_p.loaded);
    ET_CHECK_U32_EQ(1u, et_params_loaded_version(&g_p));
    ET_CHECK_U32_EQ(5u, get_u16(F_THRESH));
    ET_CHECK_U32_EQ(9600u, get_u32(F_BAUD));
    g_mig_fail = false;
    ET_CHECK(et_params_migrate(&g_p));
    ET_CHECK_U32_EQ(2u, et_params_loaded_version(&g_p));
    ET_CHECK_U32_EQ(50u, get_u16(F_THRESH));        /* v1 单位 ×10 */
    ET_CHECK_U32_EQ(9600u, get_u32(F_BAUD));        /* 透传字段不变 */
    ET_CHECK_U32_EQ(7u, get_u16(F_ADDED));          /* 新增字段 = 默认值 */
    ET_CHECK(et_params_save(&g_p));
    p_reopen();
    ET_CHECK_U32_EQ(2u, et_params_loaded_version(&g_p));
    ET_CHECK_U32_EQ(50u, get_u16(F_THRESH));
    ET_CHECK_U32_EQ(7u, get_u16(F_ADDED));
}

static void params_migrate_fail_rollback(void)
{
    p_fresh();
    ET_CHECK(et_params_init(&g_p, &g_kv, &g_schema_v1, g_work, sizeof(g_work)));
    set_u16(F_THRESH, 5u);
    ET_CHECK(et_params_save(&g_p));

    p_reopen();
    ET_CHECK_U32_EQ(1u, et_params_loaded_version(&g_p));
    g_mig_fail = true;
    ET_CHECK(!et_params_migrate(&g_p));             /* 回调失败 */
    g_mig_fail = false;
    ET_CHECK_U32_EQ(1u, et_params_loaded_version(&g_p));   /* 回滚到旧配置 */
    ET_CHECK_U32_EQ(5u, get_u16(F_THRESH));         /* 未被 ×10 */
    ET_CHECK_U32_EQ(F_COUNT, et_params_validate(&g_p));
}

static void params_factory_reset_keep(void)
{
    uint8_t dev[6];

    p_fresh();
    apply_set_a();
    ET_CHECK(et_params_save(&g_p));
    ET_CHECK(et_params_factory_reset(&g_p));
    ET_CHECK_U32_EQ(1u, get_u8(F_MODE));           /* 回默认 */
    ET_CHECK_U32_EQ(100u, get_u16(F_THRESH));
    ET_CHECK_U32_EQ(115200u, get_u32(F_BAUD));
    get_blob(F_DEV_ID, dev, 6u);
    ET_CHECK(dev[0] == 0xA1u);                      /* keep 字段保留 */
    p_reopen();
    ET_CHECK_U32_EQ(1u, get_u8(F_MODE));
    get_blob(F_DEV_ID, dev, 6u);
    ET_CHECK(dev[5] == 0xA6u);
}

static void params_powercut_100(void)
{
    uint32_t w0;
    uint32_t w1;
    uint32_t span;
    uint32_t i;

    /* 先测一次干净 save 的写入跨度 (同一基线态, 同一代码路径) */
    p_fresh();
    apply_set_a();
    ET_CHECK(et_params_save(&g_p));
    w0 = port_host_flash_written();
    apply_set_b();
    ET_CHECK(et_params_save(&g_p));
    w1 = port_host_flash_written();
    span = w1 - w0;

    ET_CHECK(span > 0u);
    for (i = 0u; i < 100u; i++) {
        uint32_t k = (span * i) / 100u;

        p_fresh();                                  /* 清盘 + A 组已提交 */
        apply_set_a();
        ET_CHECK(et_params_save(&g_p));
        w0 = port_host_flash_written();
        apply_set_b();
        port_host_flash_fail_after(w0 + k);         /* 第 k 字节处掉电 */
        (void)et_params_save(&g_p);
        port_host_flash_fail_after(0xFFFFFFFFu);    /* 模拟重启后恢复写入 */
        p_reopen();
        ET_CHECK(g_p.loaded);                       /* 必有一代完整 */
        ET_CHECK_U32_EQ(F_COUNT, et_params_validate(&g_p));
        check_all_a_or_b();                         /* 旧套或新套, 不得混 */
    }
}

static void params_bad_schema_reject(void)
{
    et_params_schema_t bad = g_schema;
    et_param_desc_t one[1];

    p_fresh();
    bad.count = 0u;
    ET_CHECK(!et_params_init(&g_p, &g_kv, &bad, g_work, sizeof(g_work)));
    bad = g_schema;
    bad.schema_ver = 0u;
    ET_CHECK(!et_params_init(&g_p, &g_kv, &bad, g_work, sizeof(g_work)));
    ET_CHECK(!et_params_init(&g_p, &g_kv, &g_schema, g_work, 8u));   /* 工作区不足 */
    one[0] = g_desc[F_MODE];
    one[0].type = (uint8_t)ET_PARAM_BLOB;
    one[0].len = 0u;                                 /* blob 长度 0 非法 */
    one[0].def = def_dev;
    one[0].def_len = 0u;
    bad = g_schema;
    bad.desc = one;
    bad.count = 1u;
    ET_CHECK(!et_params_init(&g_p, &g_kv, &bad, g_work, sizeof(g_work)));
}

static void params_get_bounds(void)
{
    uint8_t buf[ET_PARAMS_VAL_MAX];
    uint16_t len = 0u;

    p_fresh();
    ET_CHECK(!et_params_get(&g_p, F_BAUD, buf, 2u, &len));   /* cap 不足 */
    ET_CHECK(!et_params_get(&g_p, F_COUNT, buf, sizeof(buf), &len));  /* 越界 */
    ET_CHECK(!et_params_get(&g_p, F_MODE, NULL, 1u, &len));  /* NULL buf */
    ET_CHECK(et_params_get(&g_p, F_MODE, buf, 1u, &len));
    ET_CHECK(len == 1u);
    ET_CHECK_U32_EQ(1u, buf[0]);
}

/* v2.30-r2 CO-1: U32 全值域 (无符号比较) —— 独立 schema/介质, 不扰动主用例的 F_COUNT 面 */
static const uint32_t def_u32_full = 0x7FFFFFFFu;
static const et_param_desc_t g_desc_u32[2] = {
    /* 0: 全范围 —— max_i = -1 即 0xFFFFFFFF */
    { ET_PARAM_U32, 0u, 0u, false, 1u, 0, -1,         &def_u32_full, 4u, "full" },
    /* 1: 有界 —— 上界 0x7FFFFFFF, 高半区仍须被拒 */
    { ET_PARAM_U32, 0u, 0u, false, 1u, 0, 0x7FFFFFFF, &def_u32_full, 4u, "half" },
};
static et_params_schema_t g_schema_u32 = { g_desc_u32, 2u, 1u, NULL, NULL };
static et_kv_t            g_kv2;
static et_kv_layout_t     g_layout2 = { 2u, 3u };
static et_params_t        g_p2;
static uint8_t            g_work2[ET_PARAMS_WORK_SIZE(2)];

static void params_u32_full_range(void)
{
    static const uint32_t vals[3] = { 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu };
    uint32_t got = 0u;
    uint16_t len = 0u;
    size_t   i;

    port_host_flash_reset();
    ET_CHECK(et_kv_format(&g_kv2, &g_layout2));
    ET_CHECK(et_params_init(&g_p2, &g_kv2, &g_schema_u32, g_work2, sizeof(g_work2)));

    for (i = 0u; i < 3u; i++) {                     /* 三个边界值全部可 set/get */
        ET_CHECK(et_params_set(&g_p2, 0u, &vals[i], 4u));
        got = 0u;
        len = 0u;
        ET_CHECK(et_params_get(&g_p2, 0u, &got, sizeof(got), &len));
        ET_CHECK(len == 4u);
        ET_CHECK_U32_EQ(vals[i], got);
    }
    ET_CHECK(et_params_validate(&g_p2) == 2u);

    ET_CHECK(!et_params_set(&g_p2, 1u, &vals[1], 4u));   /* 有界描述符: 高半区仍拒 */
    ET_CHECK(!et_params_set(&g_p2, 1u, &vals[2], 4u));
    ET_CHECK(et_params_set(&g_p2, 1u, &vals[0], 4u));

    ET_CHECK(et_params_save(&g_p2));                     /* 持久化往返 */
    ET_CHECK(et_kv_init(&g_kv2, &g_layout2));
    ET_CHECK(et_params_init(&g_p2, &g_kv2, &g_schema_u32, g_work2, sizeof(g_work2)));
    ET_CHECK(g_p2.loaded);
    got = 0u;
    len = 0u;
    ET_CHECK(et_params_get(&g_p2, 0u, &got, sizeof(got), &len));
    ET_CHECK_U32_EQ(0xFFFFFFFFu, got);
}

/* v2.30-r2 CO-2(β): 回退仅限同版本代际 —— 跨版本损坏 ⇒ 不回退, 落默认值且 loaded=false */
static void params_fallback_cross_version(void)
{
    uint8_t  gen[3];
    uint16_t glen = 0u;
    uint16_t cur;
    uint8_t  one = 0x01u;

    p_fresh();
    ET_CHECK(et_params_init(&g_p, &g_kv, &g_schema_v1, g_work, sizeof(g_work)));
    set_u16(F_THRESH, 5u);
    ET_CHECK(et_params_save(&g_p));                  /* 代际 A: ver=1 */

    p_reopen();                                      /* v2 schema */
    g_mig_fail = false;
    ET_CHECK(et_params_migrate(&g_p));
    ET_CHECK(et_params_save(&g_p));                  /* 代际 B: ver=2 (GEN ver=2) */
    ET_CHECK(et_kv_get(&g_kv, ET_PARAMS_KEY_GEN, gen, sizeof(gen), &glen));
    ET_CHECK(glen == 3u);
    cur = gen[0];

    /* 破坏当前代际: 用错误长度覆盖一个 U16 字段 ⇒ 逐字段校验失败 */
    ET_CHECK(et_kv_set(&g_kv,
                       (uint16_t)(ET_PARAMS_KEY_BASE + (uint16_t)cur * ET_PARAMS_KEY_STRIDE + F_THRESH),
                       &one, 1u));
    p_reopen();                                      /* 另一代际仍是 ver=1 ⇒ 不回退 */
    ET_CHECK(!g_p.loaded);
    ET_CHECK_U32_EQ(0u, et_params_loaded_version(&g_p));
    ET_CHECK_U32_EQ(115200u, get_u32(F_BAUD));       /* 工作区 = 默认值 */
}

static const et_test_case_t g_cases[] = {
    { "params.defaults_on_empty",      params_defaults_on_empty      },
    { "params.save_load_roundtrip",    params_save_load_roundtrip    },
    { "params.set_validation",         params_set_validation         },
    { "params.gen_switch",             params_gen_switch             },
    { "params.migrate_v1_to_v2",       params_migrate_v1_to_v2       },
    { "params.migrate_fail_rollback",  params_migrate_fail_rollback  },
    { "params.factory_reset_keep",     params_factory_reset_keep     },
    { "params.powercut_100",           params_powercut_100           },
    { "params.bad_schema_reject",      params_bad_schema_reject      },
    { "params.get_bounds",             params_get_bounds             },
    { "params.u32_full_range",         params_u32_full_range         },
    { "params.fallback_cross_version", params_fallback_cross_version },
};

const et_test_case_t *test_params_cases(size_t *count)
{
    *count = sizeof(g_cases) / sizeof(g_cases[0]);
    return g_cases;
}

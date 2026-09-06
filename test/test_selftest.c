/**
 * @file    test_selftest.c
 * @brief   et_selftest 组件测试 (框架自测 + host 侧全套件复跑)
 *
 * 双重职责 (v1.7):
 *   1. 框架自测 ≥8 例: 注册表/未知套件/存储门控跳过/失败计数/报告事件/动态注册;
 *   2. host 复用: 同一份组件在 host 虚拟 flash 上跑全部内建套件 (含 kv/bootctl
 *      存储门控套件), 与板上结果可比对 —— 覆盖边界见 et_selftest.h。
 */
#include <string.h>

#include "et_test.h"
#include "et_selftest.h"
#include "et_kv.h"
#include "et_bootctl.h"
#include "et_config.h"
#include "port_host.h"

/* ---- 事件收集器 ---- */
#define EV_MAX 64
static et_selftest_evt_t g_ev[EV_MAX];
static const char       *g_ev_suite[EV_MAX];
static uint32_t          g_ev_num[EV_MAX];
static uint32_t          g_ev_n;

static void collector(void *user, et_selftest_evt_t evt,
                      const char *suite, uint32_t num)
{
    (void)user;
    if (g_ev_n < EV_MAX) {
        g_ev[g_ev_n] = evt;
        g_ev_suite[g_ev_n] = suite;
        g_ev_num[g_ev_n] = num;
        g_ev_n++;
    }
}

static uint32_t g_extra_ok_calls;

static bool extra_ok(et_selftest_report_fn r, void *u)
{
    (void)r;
    (void)u;
    g_extra_ok_calls++;
    return true;
}

static bool extra_fail(et_selftest_report_fn r, void *u)
{
    (void)r;
    (void)u;
    return false;
}

static bool extra_note(et_selftest_report_fn r, void *u)
{
    et_selftest_note_fail(r, u, "host.note", 123u);
    return false;
}

static void storage_enable(void)
{
    static const et_kv_layout_t   lay = { 14u, 15u };
    static const et_bootctl_cfg_t bc = { 13u, { 11u, 12u },
                                         PORT_FLASH_SECTOR_SIZE, 2u };

    port_host_flash_reset();
#if ET_MODULE_KV
    et_selftest_set_kv_layout(&lay);
#endif
#if ET_MODULE_BOOTCTL
    et_selftest_set_bootctl_cfg(&bc);
#endif
}

static int ev_find(et_selftest_evt_t evt, const char *suite)
{
    uint32_t i;

    for (i = 0u; i < g_ev_n; i++) {
        if ((g_ev[i] == evt) &&
            ((suite == NULL) || (strcmp(g_ev_suite[i], suite) == 0))) {
            return (int)i;
        }
    }
    return -1;
}

/* ---- 用例 ---- */

static void sf_suite_count(void)
{
    uint16_t base;

    port_host_flash_reset();
    g_ev_n = 0u;
    base = et_selftest_suite_count();
    ET_CHECK_U32_EQ(17u, base);             /* 13 G474 移植 + 4 补齐 */
    ET_CHECK(et_selftest_register("host.extra", extra_ok));
    ET_CHECK_U32_EQ((uint32_t)(base + 1u), et_selftest_suite_count());
}

static void sf_run_suite_unknown(void)
{
    port_host_flash_reset();
    ET_CHECK(!et_selftest_run_suite("nope", collector, NULL));
    ET_CHECK(!et_selftest_run_suite(NULL, collector, NULL));
}

static void sf_skip_without_storage(void)
{
    port_host_flash_reset();
    g_ev_n = 0u;
#if ET_MODULE_KV
    et_selftest_set_kv_layout(NULL);
#endif
    ET_CHECK(et_selftest_run_suite("kv", collector, NULL));     /* SKIP=通过 */
    ET_CHECK(ev_find(ET_SELFTEST_SUITE_SKIP, "kv") >= 0);
}

static void sf_builtin_single(void)
{
    port_host_flash_reset();
    g_ev_n = 0u;
    ET_CHECK(et_selftest_run_suite("crc", collector, NULL));
    ET_CHECK(ev_find(ET_SELFTEST_SUITE_PASS, "crc") >= 0);
    ET_CHECK(ev_find(ET_SELFTEST_SUITE_FAIL, NULL) < 0);
}

static void sf_register_ok(void)
{
    port_host_flash_reset();
    ET_CHECK(et_selftest_run_suite("host.extra", NULL, NULL));
    ET_CHECK_U32_EQ(1u, g_extra_ok_calls);
    ET_CHECK(!et_selftest_register("host.extra", extra_ok));    /* 重名拒绝 */
    ET_CHECK(!et_selftest_register(NULL, extra_ok));
}

static void sf_report_null(void)
{
    port_host_flash_reset();
    ET_CHECK(et_selftest_run_all(NULL, NULL));      /* 静默跑 */
}

static void sf_run_all_pass(void)
{
    uint16_t total;

    storage_enable();
    g_ev_n = 0u;
    total = et_selftest_suite_count();      /* 17 内建 + host.extra = 18 */
    ET_CHECK_U32_EQ(18u, total);
    ET_CHECK(et_selftest_run_all(collector, NULL));
    ET_CHECK(ev_find(ET_SELFTEST_BEGIN, NULL) >= 0);
    ET_CHECK_U32_EQ(total, g_ev_num[ev_find(ET_SELFTEST_BEGIN, NULL)]);
    ET_CHECK_U32_EQ(total, g_ev_num[ev_find(ET_SELFTEST_DONE, NULL)]);
    ET_CHECK(ev_find(ET_SELFTEST_SUITE_FAIL, NULL) < 0);
    ET_CHECK(ev_find(ET_SELFTEST_SUITE_SKIP, NULL) < 0);        /* 门控已配置 */
    ET_CHECK(ev_find(ET_SELFTEST_SUITE_PASS, "bootctl") >= 0);  /* 存储套件实跑 */
    ET_CHECK(ev_find(ET_SELFTEST_SUITE_PASS, "kv") >= 0);
}

static void sf_run_all_failure(void)
{
    port_host_flash_reset();
    ET_CHECK(et_selftest_register("host.extra_fail", extra_fail));
    g_ev_n = 0u;
    ET_CHECK(!et_selftest_run_all(collector, NULL));            /* 有失败套件 */
    ET_CHECK(ev_find(ET_SELFTEST_SUITE_FAIL, "host.extra_fail") >= 0);
}

static void sf_note_fail(void)
{
    port_host_flash_reset();
    ET_CHECK(et_selftest_register("host.note", extra_note));
    g_ev_n = 0u;
    ET_CHECK(!et_selftest_run_suite("host.note", collector, NULL));
    {
        int idx = ev_find(ET_SELFTEST_CHECK_FAIL, "host.note");

        ET_CHECK(idx >= 0);
        ET_CHECK_U32_EQ(123u, g_ev_num[idx]);
    }
}

const et_test_case_t *test_selftest_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"selftest.suite_count",     sf_suite_count},
        {"selftest.run_unknown",     sf_run_suite_unknown},
        {"selftest.skip_no_storage", sf_skip_without_storage},
        {"selftest.builtin_single",  sf_builtin_single},
        {"selftest.register_ok",     sf_register_ok},
        {"selftest.report_null",     sf_report_null},
        {"selftest.run_all_pass",    sf_run_all_pass},
        {"selftest.run_all_fail",    sf_run_all_failure},
        {"selftest.note_fail",       sf_note_fail},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

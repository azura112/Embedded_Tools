/**
 * @file    test_wdt.c
 * @brief   et_wdt 单元测试 (host 软件看门狗模拟 + 虚拟时间)
 *
 * 注: host 模拟的擦除上限 PORT_FLASH_ERASE_MS_MAX=1 → 契约下限 = 2ms,
 *     测试一律以宏推导, 不硬编码 40。
 */
#include <stddef.h>
#include <string.h>

#include "et_test.h"
#include "et_wdt.h"
#include "port.h"
#include "port_host.h"

#define WDT_MIN     ((uint32_t)PORT_FLASH_ERASE_MS_MAX * 2u)

static int      g_fire_cnt;
static uint32_t g_guard_calls;
static bool     g_guard_ret;                    /* guard 任务返回值注入 */

static void on_fire(void *user)
{
    (void)user;
    g_fire_cnt++;
}

static bool job_ok(void *user)
{
    (void)user;
    g_guard_calls++;
    return true;
}

static bool job_fail(void *user)
{
    (void)user;
    g_guard_calls++;
    return false;
}

static void wdt_fresh(void)
{
    port_host_wdt_reset();
    port_host_tick_set(0u);
    g_fire_cnt = 0;
    g_guard_calls = 0;
}

/* ---- 用例 ---- */

static void enable_validation(void)
{
    wdt_fresh();
    ET_CHECK(!et_wdt_enable(WDT_MIN - 1u));     /* 低于契约下限 */
    ET_CHECK(!et_wdt_enable(0u));
    ET_CHECK_U32_EQ(0u, port_host_wdt_feeds()); /* 拒绝时零副作用 */

    ET_CHECK(et_wdt_enable(WDT_MIN));           /* 恰好下限 */
    ET_CHECK(et_wdt_enable(WDT_MIN * 4u));      /* 重复 enable = 重配置 */
}

static void feed_within_window(void)
{
    wdt_fresh();
    ET_CHECK(et_wdt_enable(100u));
    port_host_tick_advance(50u);
    et_wdt_feed();
    port_host_tick_advance(49u);
    port_host_wdt_poll();
    ET_CHECK_U32_EQ(0, g_fire_cnt);             /* 窗口内: 不触发 */
}

static void timeout_fires_once(void)
{
    wdt_fresh();
    port_host_wdt_install(on_fire, NULL);
    ET_CHECK(et_wdt_enable(100u));
    port_host_tick_advance(100u);
    port_host_wdt_poll();
    ET_CHECK_U32_EQ(1, g_fire_cnt);             /* 超时触发一次 */
    port_host_tick_advance(100u);
    port_host_wdt_poll();
    ET_CHECK_U32_EQ(1, g_fire_cnt);             /* 不重复触发 */
}

static void feed_resets_window(void)
{
    wdt_fresh();
    port_host_wdt_install(on_fire, NULL);
    ET_CHECK(et_wdt_enable(100u));
    port_host_tick_advance(90u);
    et_wdt_feed();                              /* 90ms 处喂狗 */
    port_host_tick_advance(9u);
    port_host_wdt_poll();
    ET_CHECK_U32_EQ(0, g_fire_cnt);             /* 新窗口内 (距喂狗 9ms) */
    port_host_tick_advance(91u);                /* 距喂狗 100ms 到期 */
    port_host_wdt_poll();
    ET_CHECK_U32_EQ(1, g_fire_cnt);
}

static void disable_stops_sim(void)
{
    wdt_fresh();
    port_host_wdt_install(on_fire, NULL);
    ET_CHECK(et_wdt_enable(100u));
    ET_CHECK(et_wdt_disable());                 /* host 模拟可停 */
    port_host_tick_advance(1000u);
    port_host_wdt_poll();
    ET_CHECK_U32_EQ(0, g_fire_cnt);
    et_wdt_feed();                              /* 停后喂狗无效果 */
    ET_CHECK_U32_EQ(0u, port_host_wdt_feeds());
}

static void guard_normal_path(void)
{
    wdt_fresh();
    ET_CHECK(et_wdt_enable(100u));
    ET_CHECK(et_wdt_guard(job_ok, NULL));
    ET_CHECK_U32_EQ(1, g_guard_calls);
    ET_CHECK_U32_EQ(2u, port_host_wdt_feeds()); /* 前后各一 */
}

static void guard_fail_path(void)
{
    wdt_fresh();
    ET_CHECK(et_wdt_enable(100u));
    ET_CHECK(!et_wdt_guard(job_fail, NULL));    /* 任务失败透传 */
    ET_CHECK_U32_EQ(1, g_guard_calls);
    ET_CHECK_U32_EQ(2u, port_host_wdt_feeds()); /* 失败路径同样前后喂狗 */
}

static void guard_null_no_feed(void)
{
    wdt_fresh();
    ET_CHECK(!et_wdt_guard(NULL, NULL));
    ET_CHECK_U32_EQ(0u, port_host_wdt_feeds()); /* 零喂狗 */
}

static void feed_before_enable_noop(void)
{
    wdt_fresh();
    et_wdt_feed();                              /* 未 enable: 无效果 */
    ET_CHECK_U32_EQ(0u, port_host_wdt_feeds());
}

static void long_job_kept_alive(void)
{
    /* 场景: guard 内部再喂狗, 长任务跨多个原窗口不触发超时 */
    wdt_fresh();
    port_host_wdt_install(on_fire, NULL);
    ET_CHECK(et_wdt_enable(100u));
    port_host_tick_advance(90u);
    et_wdt_feed();                              /* 任务中途喂 (如每扇区) */
    port_host_tick_advance(90u);
    et_wdt_feed();
    port_host_tick_advance(50u);
    port_host_wdt_poll();
    ET_CHECK_U32_EQ(0, g_fire_cnt);             /* 持续喂狗不超时 */
    ET_CHECK_U32_EQ(2u, port_host_wdt_feeds());
}

static const et_test_case_t g_cases[] = {
    { "wdt.enable_validation",   enable_validation },
    { "wdt.feed_within_window",  feed_within_window },
    { "wdt.timeout_fires_once",  timeout_fires_once },
    { "wdt.feed_resets_window",  feed_resets_window },
    { "wdt.disable_stops",       disable_stops_sim },
    { "wdt.guard_normal",        guard_normal_path },
    { "wdt.guard_fail_path",     guard_fail_path },
    { "wdt.guard_null",          guard_null_no_feed },
    { "wdt.feed_before_enable",  feed_before_enable_noop },
    { "wdt.long_job_alive",      long_job_kept_alive },
};

const et_test_case_t *test_wdt_cases(size_t *count)
{
    *count = sizeof(g_cases) / sizeof(g_cases[0]);
    return g_cases;
}

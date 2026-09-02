/**
 * @file    test_softclock.c
 * @brief   et_softclock 单元测试
 *
 * 期望值由独立的 days_from_civil(逆变换, Howard Hinnant)在测试侧构造,
 * 另加一组公开可查的锚点时间戳(UTC)双重校验。
 */
#include "et_test.h"
#include "et_softclock.h"

/* 测试侧日期 → UNIX 秒 (days_from_civil, 与实现互为逆变换) */
static uint32_t days_from_civil(uint32_t y, uint32_t m, uint32_t d)
{
    uint32_t era, yoe, doy, doe;

    y -= (m <= 2u) ? 1u : 0u;
    era = y / 400u;
    yoe = y - era * 400u;
    doy = (153u * (m + ((m > 2u) ? (uint32_t)0u - 3u : 9u)) + 2u) / 5u + d - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097u + doe - 719468u;
}

static uint32_t unix_of(uint32_t y, uint32_t mo, uint32_t d,
                        uint32_t h, uint32_t mi, uint32_t s)
{
    return days_from_civil(y, mo, d) * 86400u + h * 3600u + mi * 60u + s;
}

static void expect_dt(uint32_t unix, uint32_t y, uint32_t mo, uint32_t d,
                      uint32_t h, uint32_t mi, uint32_t s)
{
    et_softclock_t  sc;
    et_datetime_t   dt;

    ET_CHECK(et_softclock_init(&sc, unix));
    ET_CHECK(et_softclock_get_datetime(&sc, &dt));
    ET_CHECK_U32_EQ(y,  dt.year);
    ET_CHECK_U32_EQ(mo, dt.month);
    ET_CHECK_U32_EQ(d,  dt.day);
    ET_CHECK_U32_EQ(h,  dt.hour);
    ET_CHECK_U32_EQ(mi, dt.min);
    ET_CHECK_U32_EQ(s,  dt.sec);
}

static et_softclock_t g_sc;

static void sc_init_validation(void)
{
    ET_CHECK(!et_softclock_init(NULL, 0u));
    ET_CHECK(et_softclock_init(&g_sc, 1000u));
    ET_CHECK_U32_EQ(1000u, et_softclock_unix(&g_sc));
}

static void sc_epoch_1970(void)
{
    expect_dt(0u, 1970u, 1u, 1u, 0u, 0u, 0u);
}

static void sc_known_anchors(void)
{
    expect_dt(951782400u,  2000u, 2u, 29u, 0u, 0u, 0u);     /* 闰日 */
    expect_dt(1000000000u, 2001u, 9u, 9u, 1u, 46u, 40u);
    expect_dt(2145916800u, 2038u, 1u, 1u, 0u, 0u, 0u);      /* 2038 边界 */
    expect_dt(4102444800u, 2100u, 1u, 1u, 0u, 0u, 0u);
    expect_dt(4107542400u, 2100u, 3u, 1u, 0u, 0u, 0u);      /* 2100 非闰: 2/29 不存在 */
}

static void sc_non_leap_century(void)
{
    /* 2100-02-28 之后直接 03-01(世纪年非闰) */
    expect_dt(unix_of(2100u, 2u, 28u, 12u, 0u, 0u), 2100u, 2u, 28u, 12u, 0u, 0u);
    expect_dt(unix_of(2100u, 2u, 28u, 12u, 0u, 0u) + 86400u,
              2100u, 3u, 1u, 12u, 0u, 0u);
}

static void sc_leap_day_2000(void)
{
    /* 2000 闰年: 02-28 → 02-29 → 03-01 */
    expect_dt(unix_of(2000u, 2u, 28u, 0u, 0u, 0u) + 86399u,
              2000u, 2u, 28u, 23u, 59u, 59u);
    expect_dt(unix_of(2000u, 2u, 28u, 0u, 0u, 0u) + 86400u,
              2000u, 2u, 29u, 0u, 0u, 0u);
    expect_dt(unix_of(2000u, 2u, 28u, 0u, 0u, 0u) + 2u * 86400u,
              2000u, 3u, 1u, 0u, 0u, 0u);
}

static void sc_month_end_rollover(void)
{
    expect_dt(unix_of(2026u, 2u, 28u, 23u, 59u, 59u) + 1u,
              2026u, 3u, 1u, 0u, 0u, 0u);                   /* 平年 2 月 28 天 */
    expect_dt(unix_of(2026u, 4u, 30u, 23u, 59u, 59u) + 1u,
              2026u, 5u, 1u, 0u, 0u, 0u);                   /* 30 天月 */
    expect_dt(unix_of(2026u, 12u, 31u, 23u, 59u, 59u) + 1u,
              2027u, 1u, 1u, 0u, 0u, 0u);                   /* 跨年 */
}

static void sc_poll_sec_chain(void)
{
    uint32_t t = 0u;

    ET_CHECK(et_softclock_init(&g_sc, 0u));
    et_softclock_poll(&g_sc, t);                            /* 锁基准 */
    for (t = 1000u; t <= 3000u; t += 1000u) {
        et_softclock_poll(&g_sc, t);
        ET_CHECK_U32_EQ(t / 1000u, et_softclock_unix(&g_sc));
    }
}

static void sc_poll_ms_accumulation(void)
{
    uint32_t t;

    ET_CHECK(et_softclock_init(&g_sc, 100u));
    et_softclock_poll(&g_sc, 0u);
    for (t = 250u; t <= 2000u; t += 250u) {                 /* 8x250ms = +2s */
        et_softclock_poll(&g_sc, t);
    }
    ET_CHECK_U32_EQ(102u, et_softclock_unix(&g_sc));
}

static void sc_poll_tick_wraparound(void)
{
    ET_CHECK(et_softclock_init(&g_sc, 7u));
    et_softclock_poll(&g_sc, 0xFFFFFF00u);                  /* 回绕前锁基准 */
    et_softclock_poll(&g_sc, 0xFFFFFFFCu);                  /* +252ms */
    et_softclock_poll(&g_sc, 0x00000014u);                  /* 跨回绕 +24ms */
    ET_CHECK_U32_EQ(7u, et_softclock_unix(&g_sc));          /* 共 +276ms → 不足 1s */

    et_softclock_poll(&g_sc, 0x000003E8u);                  /* 再 +980ms → 累计超 1s */
    ET_CHECK_U32_EQ(8u, et_softclock_unix(&g_sc));
}

static void sc_poll_large_delta(void)
{
    ET_CHECK(et_softclock_init(&g_sc, 0u));
    et_softclock_poll(&g_sc, 0u);
    et_softclock_poll(&g_sc, 3600000u);                     /* 模拟长休眠 +1h */
    ET_CHECK_U32_EQ(3600u, et_softclock_unix(&g_sc));
}

static void sc_set_unix_resync(void)
{
    ET_CHECK(et_softclock_init(&g_sc, 0u));
    et_softclock_poll(&g_sc, 0u);
    et_softclock_poll(&g_sc, 5000u);
    ET_CHECK_U32_EQ(5u, et_softclock_unix(&g_sc));
    et_softclock_set_unix(&g_sc, unix_of(2026u, 9u, 2u, 12u, 0u, 0u));
    ET_CHECK_U32_EQ(unix_of(2026u, 9u, 2u, 12u, 0u, 0u), et_softclock_unix(&g_sc));
    et_softclock_poll(&g_sc, 6000u);                        /* 重设后继续推进 */
    ET_CHECK_U32_EQ(unix_of(2026u, 9u, 2u, 12u, 0u, 0u) + 1u,
                    et_softclock_unix(&g_sc));
}

static void sc_multi_instance(void)
{
    static et_softclock_t a, b;

    ET_CHECK(et_softclock_init(&a, 100u));
    ET_CHECK(et_softclock_init(&b, 200u));
    et_softclock_poll(&a, 0u);
    et_softclock_poll(&b, 0u);
    et_softclock_poll(&a, 3000u);
    ET_CHECK_U32_EQ(103u, et_softclock_unix(&a));
    ET_CHECK_U32_EQ(200u, et_softclock_unix(&b));           /* b 未推进不受影响 */
}

static void sc_datetime_invalid_args(void)
{
    ET_CHECK(et_softclock_init(&g_sc, 0u));
    ET_CHECK(!et_softclock_get_datetime(&g_sc, NULL));
    ET_CHECK(!et_softclock_get_datetime(NULL, &(et_datetime_t){0}));
}

const et_test_case_t *test_softclock_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"sc.init_validation",     sc_init_validation},
        {"sc.epoch_1970",          sc_epoch_1970},
        {"sc.known_anchors",       sc_known_anchors},
        {"sc.non_leap_century",    sc_non_leap_century},
        {"sc.leap_day_2000",       sc_leap_day_2000},
        {"sc.month_end_rollover",  sc_month_end_rollover},
        {"sc.poll_sec_chain",      sc_poll_sec_chain},
        {"sc.poll_ms_accum",       sc_poll_ms_accumulation},
        {"sc.poll_wraparound",     sc_poll_tick_wraparound},
        {"sc.poll_large_delta",    sc_poll_large_delta},
        {"sc.set_unix_resync",     sc_set_unix_resync},
        {"sc.multi_instance",      sc_multi_instance},
        {"sc.invalid_args",        sc_datetime_invalid_args},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

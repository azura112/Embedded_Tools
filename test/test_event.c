/**
 * @file    test_event.c
 * @brief   et_event 单元测试
 */
#include "et_test.h"
#include "et_event.h"

static et_event_group_t g_evt;

static void ev_set_wait_basic(void)
{
    et_event_init(&g_evt);
    ET_CHECK_U32_EQ(0u, et_event_peek(&g_evt));

    et_event_set(&g_evt, 0x01u | 0x08u);
    ET_CHECK_U32_EQ(0x09u, et_event_peek(&g_evt));

    /* 只取走 mask 命中的位 */
    ET_CHECK_U32_EQ(0x01u, et_event_wait_and_clear(&g_evt, 0x03u));
    ET_CHECK_U32_EQ(0x08u, et_event_peek(&g_evt));  /* 未命中位保留 */
}

static void ev_multi_set_accumulates(void)
{
    et_event_init(&g_evt);
    et_event_set(&g_evt, 0x02u);
    et_event_set(&g_evt, 0x04u);
    et_event_set(&g_evt, 0x02u);                    /* 重复置位幂等 */
    ET_CHECK_U32_EQ(0x06u, et_event_peek(&g_evt));

    ET_CHECK_U32_EQ(0x06u, et_event_wait_and_clear(&g_evt, 0xFFFFFFFFu));
    ET_CHECK_U32_EQ(0u, et_event_peek(&g_evt));
}

static void ev_wait_none_returns_zero(void)
{
    et_event_init(&g_evt);
    ET_CHECK_U32_EQ(0u, et_event_wait_and_clear(&g_evt, 0xFFu));

    et_event_set(&g_evt, 0x10u);
    ET_CHECK_U32_EQ(0u, et_event_wait_and_clear(&g_evt, 0x0Fu)); /* 不匹配取不到 */
    ET_CHECK_U32_EQ(0x10u, et_event_peek(&g_evt));               /* 且不清除 */
}

static void ev_clear_direct(void)
{
    et_event_init(&g_evt);
    et_event_set(&g_evt, 0x1234u);
    et_event_clear(&g_evt, 0x00FFu);
    ET_CHECK_U32_EQ(0x1200u, et_event_peek(&g_evt));
    et_event_clear(&g_evt, 0xFFFFFFFFu);
    ET_CHECK_U32_EQ(0u, et_event_peek(&g_evt));
}

static void ev_zero_mask_noop(void)
{
    et_event_init(&g_evt);
    et_event_set(&g_evt, 0x01u);
    et_event_set(&g_evt, 0u);                       /* 零位集为无操作 */
    ET_CHECK_U32_EQ(0u, et_event_wait_and_clear(&g_evt, 0u));
    ET_CHECK_U32_EQ(0x01u, et_event_peek(&g_evt));
    et_event_clear(&g_evt, 0u);
    ET_CHECK_U32_EQ(0x01u, et_event_peek(&g_evt));
}

static void ev_full_width(void)
{
    et_event_init(&g_evt);
    et_event_set(&g_evt, 0x80000000u | 0x00000001u);
    ET_CHECK_U32_EQ(0x80000001u, et_event_wait_and_clear(&g_evt, 0xFFFFFFFFu));
    ET_CHECK_U32_EQ(0u, et_event_peek(&g_evt));
}

static void ev_null_safety(void)
{
    et_event_init(NULL);                            /* 全部 API 空指针安全 */
    et_event_set(NULL, 1u);
    ET_CHECK_U32_EQ(0u, et_event_peek(NULL));
    ET_CHECK_U32_EQ(0u, et_event_wait_and_clear(NULL, 1u));
    et_event_clear(NULL, 1u);
    ET_CHECK(1);
}

const et_test_case_t *test_event_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"event.set_wait_basic",   ev_set_wait_basic},
        {"event.multi_accumulate", ev_multi_set_accumulates},
        {"event.wait_none_zero",   ev_wait_none_returns_zero},
        {"event.clear_direct",     ev_clear_direct},
        {"event.zero_mask_noop",   ev_zero_mask_noop},
        {"event.full_width",       ev_full_width},
        {"event.null_safety",      ev_null_safety},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

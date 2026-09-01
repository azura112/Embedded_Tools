/**
 * @file    test_queue.c
 * @brief   et_queue 单元测试
 */
#include "et_test.h"
#include "et_queue.h"
#include <string.h>

typedef struct {
    uint32_t id;
    uint16_t len;
    uint8_t  tag;
} test_msg_t;

static uint8_t      g_storage[64];
static et_queue_t   g_q;

static void q_init_floor_capacity(void)
{
    /* 10 字节存储 / 3 字节元素 => 向下取整容量 3 */
    ET_CHECK(et_queue_init(&g_q, g_storage, 10u, 3u));
    ET_CHECK_U32_EQ(3u, et_queue_capacity(&g_q));
    ET_CHECK(et_queue_is_empty(&g_q));
    ET_CHECK_U32_EQ(0u, et_queue_count(&g_q));
}

static void q_init_rejects_bad(void)
{
    ET_CHECK(!et_queue_init(NULL, g_storage, 8u, 2u));
    ET_CHECK(!et_queue_init(&g_q, NULL, 8u, 2u));
    ET_CHECK(!et_queue_init(&g_q, g_storage, 8u, 0u));  /* 非法元素大小 */
    ET_CHECK(!et_queue_init(&g_q, g_storage, 1u, 4u));  /* 容量取整为 0 */
}

static void q_fifo_order(void)
{
    uint8_t a = 1u, b = 2u, c = 3u;
    uint8_t out = 0u;

    (void)et_queue_init(&g_q, g_storage, sizeof(g_storage), sizeof(uint8_t));
    ET_CHECK(et_queue_push(&g_q, &a));
    ET_CHECK(et_queue_push(&g_q, &b));
    ET_CHECK(et_queue_push(&g_q, &c));
    ET_CHECK_U32_EQ(3u, et_queue_count(&g_q));

    ET_CHECK(et_queue_pop(&g_q, &out));
    ET_CHECK_U32_EQ(1u, out);
    ET_CHECK(et_queue_pop(&g_q, &out));
    ET_CHECK_U32_EQ(2u, out);
    ET_CHECK(et_queue_pop(&g_q, &out));
    ET_CHECK_U32_EQ(3u, out);
    ET_CHECK(et_queue_is_empty(&g_q));
}

static void q_full_reject_and_empty_pop_fail(void)
{
    uint8_t v = 0u;
    uint32_t i;

    (void)et_queue_init(&g_q, g_storage, sizeof(g_storage), 1u);
    for (i = 0u; i < et_queue_capacity(&g_q); i++) {
        ET_CHECK(et_queue_push(&g_q, &v));
    }
    ET_CHECK(et_queue_is_full(&g_q));
    ET_CHECK(!et_queue_push(&g_q, &v));             /* 队满拒绝且不覆盖 */
    ET_CHECK_U32_EQ((uint32_t)sizeof(g_storage), et_queue_count(&g_q));

    while (et_queue_pop(&g_q, &v)) {
        /* 排空 */
    }
    ET_CHECK(!et_queue_pop(&g_q, &v));              /* 队空弹出失败 */
}

static void q_struct_item_integrity(void)
{
    test_msg_t m;
    test_msg_t out;
    uint32_t   i;

    (void)et_queue_init(&g_q, g_storage, sizeof(g_storage), sizeof(test_msg_t));
    for (i = 0u; i < et_queue_capacity(&g_q); i++) {
        m.id = i * 7u + 1u;
        m.len = (uint16_t)(i * 13u);
        m.tag = (uint8_t)(i ^ 0xA5u);
        ET_CHECK(et_queue_push(&g_q, &m));
    }
    for (i = 0u; i < et_queue_capacity(&g_q); i++) {
        ET_CHECK(et_queue_pop(&g_q, &out));
        ET_CHECK_U32_EQ(i * 7u + 1u, out.id);
        ET_CHECK_U32_EQ(i * 13u, out.len);
        ET_CHECK_U32_EQ((uint32_t)(i ^ 0xA5u), out.tag);
    }
}

static void q_wraparound_alternating(void)
{
    uint8_t  out;
    uint32_t round;

    /* 容量 3 的队列反复"填满-排空", 使槽位索引多次回绕复用 */
    (void)et_queue_init(&g_q, g_storage, 3u, 1u);
    for (round = 0u; round < 300u; round++) {
        uint8_t base = (uint8_t)(round * 3u);
        uint8_t k;

        for (k = 0u; k < 3u; k++) {
            uint8_t v     = base + k;
            uint8_t expct = base + k;               /* 用 uint8 截断后再比较 */

            ET_CHECK(et_queue_push(&g_q, &v));
            ET_CHECK(et_queue_pop(&g_q, &out));
            ET_CHECK_U32_EQ(expct, out);
        }
    }
}

static void q_reset_works(void)
{
    uint8_t v = 5u;
    uint8_t out = 0u;

    (void)et_queue_init(&g_q, g_storage, 4u, 1u);
    (void)et_queue_push(&g_q, &v);
    (void)et_queue_push(&g_q, &v);
    et_queue_reset(&g_q);
    ET_CHECK(et_queue_is_empty(&g_q));
    ET_CHECK(!et_queue_pop(&g_q, &out));            /* 复位后不可再弹出 */
    ET_CHECK_U32_EQ(0u, out);                       /* item 未被触碰 */

    /* 复位后可正常重新入队 */
    ET_CHECK(et_queue_push(&g_q, &v));
    ET_CHECK(et_queue_pop(&g_q, &out));
    ET_CHECK_U32_EQ(5u, out);
}

static void q_count_tracking(void)
{
    uint8_t v = 1u;
    uint8_t out = 0u;

    (void)et_queue_init(&g_q, g_storage, 8u, 1u);
    ET_CHECK(et_queue_push(&g_q, &v));
    ET_CHECK(et_queue_push(&g_q, &v));
    (void)et_queue_pop(&g_q, &out);
    ET_CHECK_U32_EQ(1u, et_queue_count(&g_q));
    (void)et_queue_pop(&g_q, &out);
    ET_CHECK_U32_EQ(0u, et_queue_count(&g_q));
}

const et_test_case_t *test_queue_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"queue.init_floor_capacity",  q_init_floor_capacity},
        {"queue.init_rejects_bad",     q_init_rejects_bad},
        {"queue.fifo_order",           q_fifo_order},
        {"queue.full_reject",          q_full_reject_and_empty_pop_fail},
        {"queue.struct_integrity",     q_struct_item_integrity},
        {"queue.wraparound_alternating", q_wraparound_alternating},
        {"queue.reset_works",          q_reset_works},
        {"queue.count_tracking",       q_count_tracking},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

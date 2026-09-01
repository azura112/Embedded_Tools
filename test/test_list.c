/**
 * @file    test_list.c
 * @brief   et_list 单元测试
 */
#include "et_test.h"
#include "et_list.h"
#include <string.h>

typedef struct {
    et_list_node_t node;
    int32_t        id;
} item_t;

static et_list_t g_list;
static item_t    g_items[8];

/* 回调侧共享的记录区 */
static int32_t   g_visit_seq[16];
static uint32_t  g_visit_n;

static item_t *to_item(et_list_node_t *n)
{
    return ET_LIST_CONTAINER(n, item_t, node);
}

static void visit_record(et_list_node_t *n, void *user)
{
    (void)user;
    g_visit_seq[g_visit_n++] = to_item(n)->id;
}

static void visit_remove_cur(et_list_node_t *n, void *user)
{
    et_list_remove((et_list_t *)user, n);
}

static void visit_remove_successor(et_list_node_t *n, void *user)
{
    et_list_node_t *succ = n->next;

    if (succ != &((et_list_t *)user)->head) {
        et_list_remove((et_list_t *)user, succ);
    }
}

static void visit_clear_all(et_list_node_t *n, void *user)
{
    /* 只在第一次回调时一次性摘光整表 */
    et_list_t *l = (et_list_t *)user;

    if (!et_list_is_empty(l)) {
        while (et_list_remove(l, l->head.next)) {
            /* 逐个摘除 */
        }
    }
    (void)n;
}

static void list_reset(void)
{
    uint32_t i;

    et_list_init(&g_list);
    for (i = 0u; i < 8u; i++) {
        et_list_node_init(&g_items[i].node);
        g_items[i].id = (int32_t)i;
    }
    g_visit_n = 0u;
    memset(g_visit_seq, 0, sizeof(g_visit_seq));
}

static void list_init_empty(void)
{
    list_reset();
    ET_CHECK(et_list_is_empty(&g_list));
    ET_CHECK_U32_EQ(0u, et_list_count(&g_list));
    ET_CHECK(et_list_front(&g_list) == NULL);
    ET_CHECK(et_list_back(&g_list) == NULL);
}

static void list_push_back_order(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 5u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    ET_CHECK_U32_EQ(5u, et_list_count(&g_list));
    ET_CHECK(to_item(et_list_front(&g_list))->id == 0);
    ET_CHECK(to_item(et_list_back(&g_list))->id == 4);
}

static void list_push_front_order(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 5u; i++) {
        ET_CHECK(et_list_push_front(&g_list, &g_items[i].node));
    }
    ET_CHECK(to_item(et_list_front(&g_list))->id == 4);
    ET_CHECK(to_item(et_list_back(&g_list))->id == 0);
}

static void list_mixed_push(void)
{
    list_reset();
    ET_CHECK(et_list_push_back(&g_list, &g_items[0].node));
    ET_CHECK(et_list_push_front(&g_list, &g_items[1].node));
    ET_CHECK(et_list_push_back(&g_list, &g_items[2].node));
    ET_CHECK(et_list_push_front(&g_list, &g_items[3].node));
    ET_CHECK_U32_EQ(4u, et_list_count(&g_list));

    et_list_foreach(&g_list, visit_record, NULL);
    ET_CHECK_U32_EQ(4u, g_visit_n);
    ET_CHECK_U32_EQ(3u, (uint32_t)g_visit_seq[0]);      /* 3 1 0 2 */
    ET_CHECK_U32_EQ(1u, (uint32_t)g_visit_seq[1]);
    ET_CHECK_U32_EQ(0u, (uint32_t)g_visit_seq[2]);
    ET_CHECK_U32_EQ(2u, (uint32_t)g_visit_seq[3]);
}

static void list_remove_middle(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 5u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    ET_CHECK(et_list_remove(&g_list, &g_items[2].node));
    ET_CHECK_U32_EQ(4u, et_list_count(&g_list));

    et_list_foreach(&g_list, visit_record, NULL);
    ET_CHECK_U32_EQ(4u, g_visit_n);
    ET_CHECK_U32_EQ(0u, (uint32_t)g_visit_seq[0]);      /* 0 1 3 4 */
    ET_CHECK_U32_EQ(1u, (uint32_t)g_visit_seq[1]);
    ET_CHECK_U32_EQ(3u, (uint32_t)g_visit_seq[2]);
    ET_CHECK_U32_EQ(4u, (uint32_t)g_visit_seq[3]);
}

static void list_remove_ends(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 3u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    ET_CHECK(et_list_remove(&g_list, &g_items[0].node));        /* 首节点 */
    ET_CHECK(to_item(et_list_front(&g_list))->id == 1);
    ET_CHECK(et_list_remove(&g_list, &g_items[2].node));        /* 尾节点 */
    ET_CHECK(to_item(et_list_back(&g_list))->id == 1);
    ET_CHECK_U32_EQ(1u, et_list_count(&g_list));
    ET_CHECK(et_list_remove(&g_list, &g_items[1].node));        /* 最后一个 */
    ET_CHECK(et_list_is_empty(&g_list));
}

static void list_double_remove_rejected(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 3u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    ET_CHECK(et_list_remove(&g_list, &g_items[1].node));
    ET_CHECK(!et_list_remove(&g_list, &g_items[1].node));       /* 重复移除被拒 */
    ET_CHECK_U32_EQ(2u, et_list_count(&g_list));                /* 计数未受干扰 */

    et_list_foreach(&g_list, visit_record, NULL);               /* 链结构完好 */
    ET_CHECK_U32_EQ(2u, g_visit_n);
    ET_CHECK_U32_EQ(0u, (uint32_t)g_visit_seq[0]);
    ET_CHECK_U32_EQ(2u, (uint32_t)g_visit_seq[1]);
}

static void list_remove_unlinked_rejected(void)
{
    item_t outsider;

    list_reset();
    et_list_node_init(&outsider.node);
    outsider.id = 99;
    ET_CHECK(!et_list_remove(&g_list, &outsider.node));         /* 从未入链 */
    ET_CHECK_U32_EQ(0u, et_list_count(&g_list));
}

static void list_double_push_rejected(void)
{
    list_reset();
    ET_CHECK(et_list_push_back(&g_list, &g_items[0].node));
    ET_CHECK(!et_list_push_back(&g_list, &g_items[0].node));    /* 同节点二次入链 */
    ET_CHECK(!et_list_push_front(&g_list, &g_items[0].node));
    ET_CHECK_U32_EQ(1u, et_list_count(&g_list));

    /* 另一链表也视为"已在链上" */
    {
        et_list_t l2;

        et_list_init(&l2);
        ET_CHECK(!et_list_push_back(&l2, &g_items[0].node));
    }
}

static void list_foreach_visits_all(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 8u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    et_list_foreach(&g_list, visit_record, NULL);
    ET_CHECK_U32_EQ(8u, g_visit_n);
    for (i = 0u; i < 8u; i++) {
        ET_CHECK_U32_EQ(i, (uint32_t)g_visit_seq[i]);
    }
}

static void list_foreach_self_remove(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 6u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    /* 遍历中移除当前节点: 剩余节点仍应全部被访问 */
    et_list_foreach(&g_list, visit_remove_cur, &g_list);
    ET_CHECK(et_list_is_empty(&g_list));
    ET_CHECK_U32_EQ(0u, et_list_count(&g_list));
}

static void list_foreach_remove_successor(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 6u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    /* 每访问一个节点就删掉它的后继: 奇数位节点被删, 0/2/4 幸存 */
    et_list_foreach(&g_list, visit_remove_successor, &g_list);
    ET_CHECK_U32_EQ(3u, et_list_count(&g_list));

    g_visit_n = 0u;
    et_list_foreach(&g_list, visit_record, NULL);
    ET_CHECK_U32_EQ(3u, g_visit_n);
    ET_CHECK_U32_EQ(0u, (uint32_t)g_visit_seq[0]);
    ET_CHECK_U32_EQ(2u, (uint32_t)g_visit_seq[1]);
    ET_CHECK_U32_EQ(4u, (uint32_t)g_visit_seq[2]);
}

static void list_foreach_clear_all_in_one_visit(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 6u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    /* 回调一次摘光整表: 遍历必须干净终止且不越界 */
    et_list_foreach(&g_list, visit_clear_all, &g_list);
    ET_CHECK(et_list_is_empty(&g_list));
    ET_CHECK_U32_EQ(0u, et_list_count(&g_list));
}

static void list_reuse_after_remove(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 3u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    ET_CHECK(et_list_remove(&g_list, &g_items[1].node));
    ET_CHECK(et_list_push_front(&g_list, &g_items[1].node));    /* 摘除后可重新入链 */

    g_visit_n = 0u;
    et_list_foreach(&g_list, visit_record, NULL);
    ET_CHECK_U32_EQ(3u, g_visit_n);
    ET_CHECK_U32_EQ(1u, (uint32_t)g_visit_seq[0]);              /* 1 0 2 */
    ET_CHECK_U32_EQ(0u, (uint32_t)g_visit_seq[1]);
    ET_CHECK_U32_EQ(2u, (uint32_t)g_visit_seq[2]);
}

static void list_reinit_resets(void)
{
    uint32_t i;

    list_reset();
    for (i = 0u; i < 4u; i++) {
        ET_CHECK(et_list_push_back(&g_list, &g_items[i].node));
    }
    et_list_init(&g_list);                                      /* 重新初始化 */
    ET_CHECK(et_list_is_empty(&g_list));
    ET_CHECK_U32_EQ(0u, et_list_count(&g_list));
    /* 重新初始化后节点须重新 node_init 才能入链(其 prev 仍指旧链) */
    et_list_node_init(&g_items[0].node);
    ET_CHECK(et_list_push_back(&g_list, &g_items[0].node));
    ET_CHECK_U32_EQ(1u, et_list_count(&g_list));
}

static void list_container_recovery(void)
{
    /* 大偏移成员: 验证 ET_LIST_CONTAINER 偏移计算正确 */
    typedef struct {
        uint8_t  pad[13];
        et_list_node_t node;
        uint32_t payload;
    } wide_t;

    wide_t a, b;
    et_list_t l;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.payload = 0x11111111u;
    b.payload = 0x22222222u;

    et_list_init(&l);
    ET_CHECK(et_list_push_back(&l, &a.node));
    ET_CHECK(et_list_push_back(&l, &b.node));

    ET_CHECK(ET_LIST_CONTAINER(et_list_front(&l), wide_t, node) == &a);
    ET_CHECK(ET_LIST_CONTAINER(et_list_back(&l),  wide_t, node) == &b);
    ET_CHECK_U32_EQ(0x11111111u, ET_LIST_CONTAINER(et_list_front(&l), wide_t, node)->payload);
}

const et_test_case_t *test_list_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"list.init_empty",              list_init_empty},
        {"list.push_back_order",         list_push_back_order},
        {"list.push_front_order",        list_push_front_order},
        {"list.mixed_push",              list_mixed_push},
        {"list.remove_middle",           list_remove_middle},
        {"list.remove_ends",             list_remove_ends},
        {"list.double_remove_rejected",  list_double_remove_rejected},
        {"list.remove_unlinked_rejected", list_remove_unlinked_rejected},
        {"list.double_push_rejected",    list_double_push_rejected},
        {"list.foreach_visits_all",      list_foreach_visits_all},
        {"list.foreach_self_remove",     list_foreach_self_remove},
        {"list.foreach_remove_successor", list_foreach_remove_successor},
        {"list.foreach_clear_all",       list_foreach_clear_all_in_one_visit},
        {"list.reuse_after_remove",      list_reuse_after_remove},
        {"list.reinit_resets",           list_reinit_resets},
        {"list.container_recovery",      list_container_recovery},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

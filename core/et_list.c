/**
 * @file    et_list.c
 * @brief   侵入式双向链表实现
 */
#include "et_list.h"

#if ET_MODULE_LIST

void et_list_init(et_list_t *l)
{
    ET_ASSERT(l != NULL);
    if (l == NULL) {
        return;
    }
    l->head.next = &l->head;
    l->head.prev = &l->head;
    l->count     = 0u;
}

void et_list_node_init(et_list_node_t *n)
{
    ET_ASSERT(n != NULL);
    if (n == NULL) {
        return;
    }
    n->prev = NULL;
    n->next = NULL;
}

/* 通用挂链: 在 prev 与 prev->next 之间插入 n */
static void list_link(et_list_t *l, et_list_node_t *prev, et_list_node_t *n)
{
    n->prev      = prev;
    n->next      = prev->next;
    prev->next->prev = n;
    prev->next   = n;
    l->count++;
}

bool et_list_push_back(et_list_t *l, et_list_node_t *n)
{
    ET_ASSERT(l != NULL);
    ET_ASSERT(n != NULL);
    if ((l == NULL) || (n == NULL) || (n->prev != NULL)) {
        return false;                       /* 已在链上, 拒绝重复插入 */
    }
    list_link(l, l->head.prev, n);
    return true;
}

bool et_list_push_front(et_list_t *l, et_list_node_t *n)
{
    ET_ASSERT(l != NULL);
    ET_ASSERT(n != NULL);
    if ((l == NULL) || (n == NULL) || (n->prev != NULL)) {
        return false;
    }
    list_link(l, &l->head, n);
    return true;
}

bool et_list_remove(et_list_t *l, et_list_node_t *n)
{
    et_list_node_t *succ;

    ET_ASSERT(l != NULL);
    ET_ASSERT(n != NULL);
    if ((l == NULL) || (n == NULL) || (n->prev == NULL) || (n == &l->head)) {
        return false;                       /* 不在链上(未链接/已移除) */
    }
    succ = n->next;                         /* 先取后继, 摘链后要保留在 n->next 里 */
    n->prev->next = succ;
    succ->prev    = n->prev;
    l->count--;
    n->prev = NULL;
    n->next = succ;                         /* 约定: 移除后 next 指向原后继, 供 foreach 安全遍历 */
    return true;
}

et_list_node_t *et_list_front(const et_list_t *l)
{
    ET_ASSERT(l != NULL);
    if ((l == NULL) || (l->head.next == &l->head)) {
        return NULL;
    }
    return l->head.next;
}

et_list_node_t *et_list_back(const et_list_t *l)
{
    ET_ASSERT(l != NULL);
    if ((l == NULL) || (l->head.prev == &l->head)) {
        return NULL;
    }
    return l->head.prev;
}

bool et_list_is_empty(const et_list_t *l)
{
    ET_ASSERT(l != NULL);
    if (l == NULL) {
        return true;
    }
    return l->head.next == &l->head;
}

uint32_t et_list_count(const et_list_t *l)
{
    ET_ASSERT(l != NULL);
    if (l == NULL) {
        return 0u;
    }
    return l->count;
}

void et_list_foreach(et_list_t *l, et_list_visit_fn fn, void *user)
{
    et_list_node_t *cur;

    ET_ASSERT(l != NULL);
    ET_ASSERT(fn != NULL);
    if ((l == NULL) || (fn == NULL)) {
        return;
    }
    cur = l->head.next;
    while (cur != &l->head) {
        fn(cur, user);
        /*
         * 推进策略(配合 remove 保留 next 指向原后继的约定):
         *  - cur 仍在链上: cur->next 即下一节点(remove 已把被删后继绕过);
         *  - cur 被回调移除: cur->next 指向移除时的后继, 沿途跳过同样
         *    被移除的节点(prev == NULL), 直到回到链上节点或哨兵。
         */
        cur = cur->next;
        while ((cur != &l->head) && (cur->prev == NULL)) {
            cur = cur->next;
        }
    }
}

#endif /* ET_MODULE_LIST */

/**
 * @file    et_list.h
 * @brief   侵入式双向链表 (零拷贝组织任意结构, O(1) 插入/删除)
 *
 * 设计要点:
 *  - 节点由用户嵌入自己的结构体, 链表本身不拷贝不分配任何数据;
 *  - 哨兵头节点: 空/尾边界统一, 插入删除无分支特判;
 *  - "在链上"判据为 node->prev != NULL: 未链接或已移除的节点 prev 为 NULL,
 *    因此重复 remove/重复 push 均被拒绝, 不会破坏链结构;
 *  - remove 后 node->next 仍指向移除时的后继(仅为配合 foreach 安全遍历,
 *    除该字段外的状态在移除后一律视为无效, 重新入链前无需清理)。
 *
 * 与 et_queue 的区别: queue 是值拷贝 FIFO; list 是把节点嵌入用户结构体后
 * 按需组织, 支持任意位置 O(1) 删除, 不移动数据。
 *
 * 并发约定 (单上下文模块):
 *  - 所有 API 仅限单一上下文调用(典型为主循环);
 *  - 跨上下文共享(如 ISR 中插入、主循环中遍历)时, 须由调用方用
 *    PORT_CRITICAL_ENTER/EXIT 包裹【完整】操作序列(含遍历全程)。
 */
#ifndef ET_LIST_H
#define ET_LIST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_LIST

#ifdef __cplusplus
extern "C" {
#endif

/* 链表节点: 嵌入用户结构体, 两个指针成员为内部状态, 用户勿动 */
typedef struct et_list_node {
    struct et_list_node *prev;
    struct et_list_node *next;
} et_list_node_t;

typedef struct {
    et_list_node_t head;            /* 哨兵: head.next=首节点, head.prev=尾节点 */
    uint32_t       count;           /* 当前节点数, 勿动 */
} et_list_t;

/* 遍历回调: node 为用户结构体内嵌的节点指针, 用 ET_LIST_CONTAINER 还原宿主结构体 */
typedef void (*et_list_visit_fn)(et_list_node_t *node, void *user);

/* 由节点指针得到宿主结构体指针(典型用法见 API_GUIDE) */
#define ET_LIST_CONTAINER(node_ptr, type, member) \
    ((type *)((char *)(node_ptr) - offsetof(type, member)))

/* 初始化链表: 得到空表 */
void     et_list_init(et_list_t *l);

/* 初始化节点: 置为"不在链上"状态(静态结构体建议调用, 置零等效) */
void     et_list_node_init(et_list_node_t *n);

/* 尾插/头插: 节点已在任一链表上时返回 false */
bool     et_list_push_back(et_list_t *l, et_list_node_t *n);
bool     et_list_push_front(et_list_t *l, et_list_node_t *n);

/* O(1) 移除: 不在链上(未链接/已移除)返回 false 且链表不受影响 */
bool     et_list_remove(et_list_t *l, et_list_node_t *n);

et_list_node_t *et_list_front(const et_list_t *l);      /* 空表返回 NULL */
et_list_node_t *et_list_back(const et_list_t *l);       /* 空表返回 NULL */
bool     et_list_is_empty(const et_list_t *l);
uint32_t et_list_count(const et_list_t *l);

/* 正向遍历: 回调中移除当前节点/其后继/任意未访问节点均安全;
 * 遍历期间新插入的节点不保证被访问到 */
void     et_list_foreach(et_list_t *l, et_list_visit_fn fn, void *user);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_LIST */
#endif /* ET_LIST_H */

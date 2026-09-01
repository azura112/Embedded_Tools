/**
 * @file    et_mempool.h
 * @brief   固定块内存池 (零动态内存, 位图管理)
 *
 * 设计要点:
 *  - 存储区由调用方提供, 内部自动布局: [位图][对齐填充][块区];
 *  - 分配/释放 O(block_count/8) 最坏, 带扫描起点提示, 常规场景近似 O(1);
 *  - 【非中断安全】: 在 ISR 与主循环间共享时, 调用方须用临界区包裹。
 *
 * 存储区大小可用 et_mempool_bytes_needed() 预先计算,
 * 或定义为 static 型数组后取 sizeof。
 */
#ifndef ET_MEMPOOL_H
#define ET_MEMPOOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_MEMPOOL

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t block_size;        /* 对齐后的块字节数           */
    uint32_t block_count;       /* 块总数                     */
    uint32_t free_count;        /* 当前空闲块数               */
    uint32_t scan_hint;         /* 下次分配的扫描起点提示     */
    uint8_t *bitmap;            /* 位图(位于存储区头部)       */
    uint8_t *blocks;            /* 块区首地址(已对齐)         */
} et_mempool_t;

/* 计算指定块大小/块数所需的存储区字节数(含位图与对齐开销) */
size_t   et_mempool_bytes_needed(uint32_t block_size, uint32_t block_count);

/* 初始化: storage_size 不足时返回 false */
bool     et_mempool_init(et_mempool_t *mp, void *storage, size_t storage_size,
                         uint32_t block_size, uint32_t block_count);

/* 分配一块: 成功返回块指针; 池耗尽返回 NULL */
void    *et_mempool_alloc(et_mempool_t *mp);

/* 释放一块: 返回是否成功; STRICT 模式下拒绝越界指针与重复释放 */
bool     et_mempool_free(et_mempool_t *mp, void *ptr);

uint32_t et_mempool_free_count(const et_mempool_t *mp);

/* 判断 ptr 是否为本池内、按块对齐的有效地址(不检查分配状态) */
bool     et_mempool_contains(const et_mempool_t *mp, const void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_MEMPOOL */
#endif /* ET_MEMPOOL_H */

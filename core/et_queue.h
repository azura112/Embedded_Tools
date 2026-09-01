/**
 * @file    et_queue.h
 * @brief   定长消息队列 (SPSC, 单生产者/单消费者)
 *
 * 设计要点:
 *  - 元素定长, 存储区由调用方提供, 零动态内存;
 *  - 与 ringbuf 相同的自由递增索引技巧, SPSC 场景无需加锁;
 *  - 多生产者或多消费者场景须由调用方用临界区包裹。
 */
#ifndef ET_QUEUE_H
#define ET_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_QUEUE

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t head;         /* 下一个写入槽位(自由递增) */
    volatile uint32_t tail;         /* 下一个读取槽位(自由递增) */
    uint32_t          capacity;     /* 队列容量(元素个数)       */
    uint32_t          item_size;    /* 单个元素字节数           */
    uint8_t          *buf;          /* 外部提供的存储区         */
} et_queue_t;

/* 初始化: storage_size 将被按 item_size 向下取整分配槽位, 返回是否成功 */
bool     et_queue_init(et_queue_t *q, void *storage, size_t storage_size,
                       uint32_t item_size);

/* 复位: 清空队列, 仅在无并发访问时调用 */
void     et_queue_reset(et_queue_t *q);

uint32_t et_queue_count(const et_queue_t *q);
bool     et_queue_is_empty(const et_queue_t *q);
bool     et_queue_is_full(const et_queue_t *q);
uint32_t et_queue_capacity(const et_queue_t *q);

/* 入队: 成功返回 true; 队满时返回 false 且不覆盖旧数据 */
bool     et_queue_push(et_queue_t *q, const void *item);

/* 出队: 成功写入 item 并返回 true; 队空时返回 false(item 不被修改) */
bool     et_queue_pop(et_queue_t *q, void *item);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_QUEUE */
#endif /* ET_QUEUE_H */

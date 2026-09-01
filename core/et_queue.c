/**
 * @file    et_queue.c
 * @brief   定长消息队列实现
 */
#include "et_queue.h"

#if ET_MODULE_QUEUE

/* 元素字节拷贝(小对象直接逐字节, 避免依赖 libc memcpy) */
static void q_cpy(uint8_t *dst, const uint8_t *src, uint32_t n)
{
    while (n-- != 0u) {
        *dst++ = *src++;
    }
}

bool et_queue_init(et_queue_t *q, void *storage, size_t storage_size,
                   uint32_t item_size)
{
    ET_ASSERT(q != NULL);
    ET_ASSERT(storage != NULL);
    ET_ASSERT(item_size != 0u);
    if ((q == NULL) || (storage == NULL) || (item_size == 0u)) {
        return false;
    }
    q->head      = 0u;
    q->tail      = 0u;
    q->item_size = item_size;
    q->buf       = (uint8_t *)storage;
    q->capacity  = (uint32_t)(storage_size / (size_t)item_size);
    if (q->capacity == 0u) {
        return false;
    }
    return true;
}

void et_queue_reset(et_queue_t *q)
{
    q->head = 0u;
    q->tail = 0u;
}

uint32_t et_queue_count(const et_queue_t *q)
{
    return q->head - q->tail;
}

bool et_queue_is_empty(const et_queue_t *q)
{
    return et_queue_count(q) == 0u;
}

bool et_queue_is_full(const et_queue_t *q)
{
    return et_queue_count(q) >= q->capacity;
}

uint32_t et_queue_capacity(const et_queue_t *q)
{
    return q->capacity;
}

bool et_queue_push(et_queue_t *q, const void *item)
{
    uint32_t slot;

    ET_ASSERT(item != NULL);
    if ((item == NULL) || et_queue_is_full(q)) {
        return false;
    }
    slot = q->head % q->capacity;
    q_cpy(&q->buf[slot * q->item_size], (const uint8_t *)item, q->item_size);
    q->head++;
    return true;
}

bool et_queue_pop(et_queue_t *q, void *item)
{
    uint32_t slot;

    if (et_queue_is_empty(q)) {
        return false;
    }
    slot = q->tail % q->capacity;
    if (item != NULL) {
        q_cpy((uint8_t *)item, &q->buf[slot * q->item_size], q->item_size);
    }
    q->tail++;
    return true;
}

#endif /* ET_MODULE_QUEUE */

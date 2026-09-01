/**
 * @file    et_ringbuf.h
 * @brief   无锁环形缓冲区 (SPSC, 单生产者/单消费者)
 *
 * 设计要点:
 *  - head/tail 自由递增, 回绕由无符号减法自然处理, 不牺牲存储槽位;
 *  - 写者仅更新 head, 读者仅更新 tail => SPSC 场景无需任何加锁;
 *  - 多生产者或多消费者场景须由调用方用临界区包裹;
 *  - 提供拷贝式与零拷贝连续段两套 API, 后者适合 DMA / 零拷贝场景。
 *
 * 中断安全: SPSC 下 write* 可在 ISR 调用且主循环 read*, 反之亦然;
 *           drop/read_peek 属"读者侧"操作, write_reserve/write_commit 属"写者侧"操作,
 *           同一侧不可重入。
 */
#ifndef ET_RINGBUF_H
#define ET_RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_RINGBUF

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint32_t head;     /* 写索引(自由递增)          */
    volatile uint32_t tail;     /* 读索引(自由递增)          */
    uint32_t          size;     /* 容量(字节)                */
    uint8_t          *buf;      /* 外部提供的存储区          */
} et_ringbuf_t;

/* 初始化: storage 为调用方提供的缓冲区, size 为其字节数(>=1), 返回是否成功 */
bool     et_ringbuf_init(et_ringbuf_t *rb, void *storage, uint32_t size);

/* 复位: 清空全部数据(head=tail=0), 仅在无并发访问时调用 */
void     et_ringbuf_reset(et_ringbuf_t *rb);

uint32_t et_ringbuf_used(const et_ringbuf_t *rb);       /* 已存字节数   */
uint32_t et_ringbuf_free_space(const et_ringbuf_t *rb); /* 剩余可写字节 */
bool     et_ringbuf_is_empty(const et_ringbuf_t *rb);
bool     et_ringbuf_is_full(const et_ringbuf_t *rb);

/* 拷贝式 API: 空间不足时执行部分操作, 返回实际写入/读出/窥视字节数 */
uint32_t et_ringbuf_write(et_ringbuf_t *rb, const void *data, uint32_t len);
uint32_t et_ringbuf_read(et_ringbuf_t *rb, void *data, uint32_t len);
uint32_t et_ringbuf_peek(const et_ringbuf_t *rb, void *out, uint32_t len);

/* 丢弃 len 字节(常与 peek 配合), 超出已存数据时按上限截断 */
void     et_ringbuf_drop(et_ringbuf_t *rb, uint32_t len);

/* 零拷贝 API:
 * write_reserve 取一段【连续】可写区域(可能因回绕小于 want), 写入数据后
 * 必须以 write_commit(got 以内的长度) 发布; 未 commit 前读者不可见。
 * read_peek 取一段【连续】可读区域, 消费用 drop。got==0 时返回 NULL。 */
uint8_t        *et_ringbuf_write_reserve(et_ringbuf_t *rb, uint32_t want, uint32_t *got);
void            et_ringbuf_write_commit(et_ringbuf_t *rb, uint32_t len);
const uint8_t  *et_ringbuf_read_peek(const et_ringbuf_t *rb, uint32_t want, uint32_t *got);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_RINGBUF */
#endif /* ET_RINGBUF_H */

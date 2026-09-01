/**
 * @file    et_ringbuf.c
 * @brief   无锁环形缓冲区实现
 */
#include "et_ringbuf.h"

#if ET_MODULE_RINGBUF

/* 取模: 容量为 2 的幂时编译为位与 */
static inline uint32_t rb_mod(const et_ringbuf_t *rb, uint32_t idx)
{
#if ET_RINGBUF_POW2
    return idx & (rb->size - 1u);
#else
    return idx % rb->size;
#endif
}

/* 从环形位置 src_idx 拷出 len 字节到 dst(自动处理跨回绕两段) */
static void rb_cpy_out(const et_ringbuf_t *rb, uint32_t src_idx,
                       uint8_t *dst, uint32_t len)
{
    uint32_t pos = rb_mod(rb, src_idx);
    uint32_t first = rb->size - pos;
    uint32_t rest;

    if (first > len) {
        first = len;
    }
    rest = len - first;                 /* 剩余量须在消耗 first 前算好 */
    while (first-- != 0u) {
        *dst++ = rb->buf[pos++];
    }
    if (rest != 0u) {                   /* 跨回绕: 继续从缓冲区头部拷贝 */
        pos = 0u;
        while (rest-- != 0u) {
            *dst++ = rb->buf[pos++];
        }
    }
}

/* 把 src 的 len 字节拷入环形位置 dst_idx(自动处理跨回绕两段) */
static void rb_cpy_in(et_ringbuf_t *rb, uint32_t dst_idx,
                      const uint8_t *src, uint32_t len)
{
    uint32_t pos = rb_mod(rb, dst_idx);
    uint32_t first = rb->size - pos;
    uint32_t rest;

    if (first > len) {
        first = len;
    }
    rest = len - first;                 /* 剩余量须在消耗 first 前算好 */
    while (first-- != 0u) {
        rb->buf[pos++] = *src++;
    }
    if (rest != 0u) {                   /* 跨回绕: 继续写入缓冲区头部 */
        pos = 0u;
        while (rest-- != 0u) {
            rb->buf[pos++] = *src++;
        }
    }
}

bool et_ringbuf_init(et_ringbuf_t *rb, void *storage, uint32_t size)
{
    ET_ASSERT(rb != NULL);
    ET_ASSERT(storage != NULL);
    ET_ASSERT(size != 0u);
    if ((rb == NULL) || (storage == NULL) || (size == 0u)) {
        return false;
    }
    rb->head = 0u;
    rb->tail = 0u;
    rb->size = size;
    rb->buf  = (uint8_t *)storage;
    return true;
}

void et_ringbuf_reset(et_ringbuf_t *rb)
{
    rb->head = 0u;
    rb->tail = 0u;
}

uint32_t et_ringbuf_used(const et_ringbuf_t *rb)
{
    /* 无符号减法: 即使 head/tail 各自回绕结果仍正确 */
    return rb->head - rb->tail;
}

uint32_t et_ringbuf_free_space(const et_ringbuf_t *rb)
{
    return rb->size - et_ringbuf_used(rb);
}

bool et_ringbuf_is_empty(const et_ringbuf_t *rb)
{
    return et_ringbuf_used(rb) == 0u;
}

bool et_ringbuf_is_full(const et_ringbuf_t *rb)
{
    return et_ringbuf_used(rb) >= rb->size;
}

uint32_t et_ringbuf_write(et_ringbuf_t *rb, const void *data, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    uint32_t free_cnt = et_ringbuf_free_space(rb);

    if (len > free_cnt) {
        len = free_cnt;
    }
    if ((len != 0u) && (src != NULL)) {
        rb_cpy_in(rb, rb->head, src, len);
        rb->head += len;
    } else if ((src == NULL) && (len != 0u)) {
        len = 0u;
    }
    return len;
}

uint32_t et_ringbuf_read(et_ringbuf_t *rb, void *data, uint32_t len)
{
    uint32_t used = et_ringbuf_used(rb);

    if (len > used) {
        len = used;
    }
    if ((len != 0u) && (data != NULL)) {
        rb_cpy_out(rb, rb->tail, (uint8_t *)data, len);
        rb->tail += len;
    } else if ((data == NULL) && (len != 0u)) {
        len = 0u;
    }
    return len;
}

uint32_t et_ringbuf_peek(const et_ringbuf_t *rb, void *out, uint32_t len)
{
    uint32_t used = et_ringbuf_used(rb);

    if (len > used) {
        len = used;
    }
    if ((len != 0u) && (out != NULL)) {
        rb_cpy_out(rb, rb->tail, (uint8_t *)out, len);
    } else if ((out == NULL) && (len != 0u)) {
        len = 0u;
    }
    return len;
}

void et_ringbuf_drop(et_ringbuf_t *rb, uint32_t len)
{
    uint32_t used = et_ringbuf_used(rb);

    if (len > used) {
        len = used;
    }
    rb->tail += len;
}

uint8_t *et_ringbuf_write_reserve(et_ringbuf_t *rb, uint32_t want, uint32_t *got)
{
    uint32_t free_cnt = et_ringbuf_free_space(rb);
    uint32_t till_end = rb->size - rb_mod(rb, rb->head);
    uint32_t len = want;

    if (len > free_cnt) {
        len = free_cnt;
    }
    if (len > till_end) {
        len = till_end;             /* 只返回到物理末尾的连续区域 */
    }
    if (got != NULL) {
        *got = len;
    }
    if (len == 0u) {
        return NULL;
    }
    return &rb->buf[rb_mod(rb, rb->head)];
}

void et_ringbuf_write_commit(et_ringbuf_t *rb, uint32_t len)
{
    uint32_t free_cnt = et_ringbuf_free_space(rb);

    if (len > free_cnt) {
        len = free_cnt;             /* 防御: commit 不得越过读指针 */
    }
    rb->head += len;
}

const uint8_t *et_ringbuf_read_peek(const et_ringbuf_t *rb, uint32_t want, uint32_t *got)
{
    uint32_t used = et_ringbuf_used(rb);
    uint32_t till_end = rb->size - rb_mod(rb, rb->tail);
    uint32_t len = want;

    if (len > used) {
        len = used;
    }
    if (len > till_end) {
        len = till_end;             /* 只返回到物理末尾的连续区域 */
    }
    if (got != NULL) {
        *got = len;
    }
    if (len == 0u) {
        return NULL;
    }
    return &rb->buf[rb_mod(rb, rb->tail)];
}

#endif /* ET_MODULE_RINGBUF */

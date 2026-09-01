/**
 * @file    et_mempool.c
 * @brief   固定块内存池实现 (位图管理)
 */
#include "et_mempool.h"

#if ET_MODULE_MEMPOOL

static uint32_t mp_align_up(uint32_t v, uint32_t align)
{
    return (uint32_t)(((v + align - 1u) / align) * align);
}

/* 置位/清位/测位 */
static void mp_bit_set(et_mempool_t *mp, uint32_t idx)
{
    mp->bitmap[idx >> 3] |= (uint8_t)(1u << (idx & 7u));
}

static bool mp_bit_test(const et_mempool_t *mp, uint32_t idx)
{
    return (mp->bitmap[idx >> 3] & (uint8_t)(1u << (idx & 7u))) != 0u;
}

static void mp_bit_clear(et_mempool_t *mp, uint32_t idx)
{
    mp->bitmap[idx >> 3] &= (uint8_t)~(1u << (idx & 7u));
}

size_t et_mempool_bytes_needed(uint32_t block_size, uint32_t block_count)
{
    uint32_t bs = mp_align_up(block_size, ET_MEMPOOL_ALIGN);
    size_t   bitmap_size = (size_t)((block_count + 7u) / 8u);
    size_t   blocks_off  = (size_t)mp_align_up((uint32_t)bitmap_size,
                                               ET_MEMPOOL_ALIGN);

    return blocks_off + (size_t)bs * (size_t)block_count;
}

bool et_mempool_init(et_mempool_t *mp, void *storage, size_t storage_size,
                     uint32_t block_size, uint32_t block_count)
{
    uint8_t *base = (uint8_t *)storage;
    size_t   bitmap_size;
    size_t   blocks_off;
    size_t   i;
    size_t   needed;

    ET_ASSERT(mp != NULL);
    ET_ASSERT(storage != NULL);
    ET_ASSERT(block_size != 0u);
    ET_ASSERT(block_count != 0u);
    if ((mp == NULL) || (storage == NULL) ||
        (block_size == 0u) || (block_count == 0u)) {
        return false;
    }

    bitmap_size = (size_t)((block_count + 7u) / 8u);
    blocks_off  = (size_t)mp_align_up((uint32_t)bitmap_size, ET_MEMPOOL_ALIGN);
    needed      = blocks_off + (size_t)mp_align_up(block_size, ET_MEMPOOL_ALIGN) *
                  (size_t)block_count;
    if (storage_size < needed) {
        return false;
    }

    for (i = 0u; i < bitmap_size; i++) {    /* 清零位图(不依赖 libc) */
        base[i] = 0u;
    }

    mp->block_size = mp_align_up(block_size, ET_MEMPOOL_ALIGN);
    mp->block_count = block_count;
    mp->free_count  = block_count;
    mp->scan_hint   = 0u;
    mp->bitmap      = base;
    mp->blocks      = &base[blocks_off];
    return true;
}

void *et_mempool_alloc(et_mempool_t *mp)
{
    uint32_t i;
    uint32_t idx;

    if (mp->free_count == 0u) {
        return NULL;
    }
    for (i = 0u; i < mp->block_count; i++) {
        idx = mp->scan_hint + i;
        if (idx >= mp->block_count) {
            idx -= mp->block_count;
        }
        if (!mp_bit_test(mp, idx)) {
            mp_bit_set(mp, idx);
            mp->scan_hint = (idx + 1u < mp->block_count) ? (idx + 1u) : 0u;
            mp->free_count--;
            return &mp->blocks[(size_t)idx * mp->block_size];
        }
    }
    return NULL;    /* 位图与计数不一致时的兜底(正常不会到达) */
}

bool et_mempool_free(et_mempool_t *mp, void *ptr)
{
    uint32_t idx;

#if ET_MEMPOOL_STRICT
    if (!et_mempool_contains(mp, ptr)) {
        return false;                   /* 非本池地址 */
    }
#endif
    idx = (uint32_t)(((uint8_t *)ptr - mp->blocks) / mp->block_size);
#if ET_MEMPOOL_STRICT
    if (!mp_bit_test(mp, idx)) {
        return false;                   /* 重复释放 */
    }
#endif
    mp_bit_clear(mp, idx);
    mp->free_count++;
    if (idx <= mp->scan_hint) {         /* 尽早复用低位块 */
        mp->scan_hint = idx;
    }
    return true;
}

uint32_t et_mempool_free_count(const et_mempool_t *mp)
{
    return mp->free_count;
}

bool et_mempool_contains(const et_mempool_t *mp, const void *ptr)
{
    const uint8_t *p  = (const uint8_t *)ptr;
    size_t         off;

    if (ptr == NULL) {
        return false;
    }
    if (p < mp->blocks) {
        return false;
    }
    off = (size_t)(p - mp->blocks);
    if (off >= (size_t)mp->block_count * (size_t)mp->block_size) {
        return false;
    }
    return (off % (size_t)mp->block_size) == 0u;
}

#endif /* ET_MODULE_MEMPOOL */

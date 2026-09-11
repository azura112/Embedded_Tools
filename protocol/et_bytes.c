/**
 * @file    et_bytes.c
 * @brief   大端/小端 u16/u32 打包解包实现 (统一边界检查)
 */
#include "et_bytes.h"

#if ET_MODULE_BYTES

/* 边界检查: off 不回绕且 [off, off+size) 落在 len 内 */
static bool bytes_ok(const void *p, uint32_t len, uint32_t off, uint32_t size)
{
    if (p == NULL) {
        return false;
    }
    if (off > len) {
        return false;
    }
    return (len - off) >= size;
}

bool et_bytes_be16_get(const uint8_t *buf, uint32_t len, uint32_t off, uint16_t *out)
{
    if (out == NULL) {
        return false;
    }
    if (!bytes_ok(buf, len, off, 2u)) {
        return false;
    }
    *out = (uint16_t)(((uint16_t)buf[off] << 8) | (uint16_t)buf[off + 1u]);
    return true;
}

bool et_bytes_be32_get(const uint8_t *buf, uint32_t len, uint32_t off, uint32_t *out)
{
    if (out == NULL) {
        return false;
    }
    if (!bytes_ok(buf, len, off, 4u)) {
        return false;
    }
    *out = ((uint32_t)buf[off] << 24) | ((uint32_t)buf[off + 1u] << 16) |
           ((uint32_t)buf[off + 2u] << 8) | (uint32_t)buf[off + 3u];
    return true;
}

bool et_bytes_le16_get(const uint8_t *buf, uint32_t len, uint32_t off, uint16_t *out)
{
    if (out == NULL) {
        return false;
    }
    if (!bytes_ok(buf, len, off, 2u)) {
        return false;
    }
    *out = (uint16_t)(((uint16_t)buf[off + 1u] << 8) | (uint16_t)buf[off]);
    return true;
}

bool et_bytes_le32_get(const uint8_t *buf, uint32_t len, uint32_t off, uint32_t *out)
{
    if (out == NULL) {
        return false;
    }
    if (!bytes_ok(buf, len, off, 4u)) {
        return false;
    }
    *out = ((uint32_t)buf[off + 3u] << 24) | ((uint32_t)buf[off + 2u] << 16) |
           ((uint32_t)buf[off + 1u] << 8) | (uint32_t)buf[off];
    return true;
}

bool et_bytes_be16_put(uint8_t *buf, uint32_t len, uint32_t off, uint16_t val)
{
    if (!bytes_ok(buf, len, off, 2u)) {
        return false;
    }
    buf[off]        = (uint8_t)(val >> 8);
    buf[off + 1u]   = (uint8_t)(val & 0xFFu);
    return true;
}

bool et_bytes_be32_put(uint8_t *buf, uint32_t len, uint32_t off, uint32_t val)
{
    if (!bytes_ok(buf, len, off, 4u)) {
        return false;
    }
    buf[off]        = (uint8_t)(val >> 24);
    buf[off + 1u]   = (uint8_t)(val >> 16);
    buf[off + 2u]   = (uint8_t)(val >> 8);
    buf[off + 3u]   = (uint8_t)(val & 0xFFu);
    return true;
}

bool et_bytes_le16_put(uint8_t *buf, uint32_t len, uint32_t off, uint16_t val)
{
    if (!bytes_ok(buf, len, off, 2u)) {
        return false;
    }
    buf[off]        = (uint8_t)(val & 0xFFu);
    buf[off + 1u]   = (uint8_t)(val >> 8);
    return true;
}

bool et_bytes_le32_put(uint8_t *buf, uint32_t len, uint32_t off, uint32_t val)
{
    if (!bytes_ok(buf, len, off, 4u)) {
        return false;
    }
    buf[off]        = (uint8_t)(val & 0xFFu);
    buf[off + 1u]   = (uint8_t)(val >> 8);
    buf[off + 2u]   = (uint8_t)(val >> 16);
    buf[off + 3u]   = (uint8_t)(val >> 24);
    return true;
}

#endif /* ET_MODULE_BYTES */

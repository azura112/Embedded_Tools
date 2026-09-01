/**
 * @file    et_crc.c
 * @brief   循环冗余校验实现 (位算法)
 */
#include "et_crc.h"

#if ET_MODULE_CRC

uint8_t et_crc8_update(uint8_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t        bit;

    if (p == NULL) {
        return crc;
    }
    while (len-- != 0u) {
        crc ^= *p++;
        for (bit = 0u; bit < 8u; bit++) {
            crc = (uint8_t)((crc & 0x80u) != 0u) ?
                  (uint8_t)((uint8_t)(crc << 1) ^ 0x07u) :
                  (uint8_t)(crc << 1);
        }
    }
    return crc;
}

uint16_t et_crc16_modbus_update(uint16_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t        bit;

    if (p == NULL) {
        return crc;
    }
    while (len-- != 0u) {
        crc ^= (uint16_t)(*p++);
        for (bit = 0u; bit < 8u; bit++) {
            crc = ((crc & 0x0001u) != 0u) ?
                  (uint16_t)((uint16_t)(crc >> 1) ^ 0xA001u) :
                  (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

uint16_t et_crc16_ccitt_update(uint16_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t        bit;

    if (p == NULL) {
        return crc;
    }
    while (len-- != 0u) {
        crc ^= (uint16_t)((uint16_t)(*p++) << 8);
        for (bit = 0u; bit < 8u; bit++) {
            crc = ((crc & 0x8000u) != 0u) ?
                  (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u) :
                  (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint32_t et_crc32_update(uint32_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t        bit;

    if (p == NULL) {
        return crc;
    }
    while (len-- != 0u) {
        crc ^= (uint32_t)(*p++);
        for (bit = 0u; bit < 8u; bit++) {
            crc = ((crc & 0x00000001u) != 0u) ?
                  (uint32_t)(crc >> 1) ^ 0xEDB88320u :
                  (uint32_t)(crc >> 1);
        }
    }
    return crc;
}

uint8_t et_crc8(const void *data, uint32_t len)
{
    return et_crc8_update(ET_CRC8_INIT, data, len);
}

uint16_t et_crc16_modbus(const void *data, uint32_t len)
{
    return et_crc16_modbus_update(ET_CRC16_MODBUS_INIT, data, len);
}

uint16_t et_crc16_ccitt(const void *data, uint32_t len)
{
    return et_crc16_ccitt_update(ET_CRC16_CCITT_INIT, data, len);
}

uint32_t et_crc32(const void *data, uint32_t len)
{
    return et_crc32_update(ET_CRC32_INIT, data, len) ^ 0xFFFFFFFFu;
}

#endif /* ET_MODULE_CRC */

/**
 * @file    et_crc.h
 * @brief   循环冗余校验 (位算法实现, 零查表内存, 支持流式增量计算)
 *
 * 提供三种常用族系:
 *   CRC8        : poly 0x07,      init 0x00,     非反射(SMBus 常用)
 *   CRC16/MODBUS: poly 0x8005反射, init 0xFFFF, 输出原值(Modbus-RTU 标准)
 *   CRC16/CCITT-FALSE: poly 0x1021, init 0xFFFF, 非反射
 *   CRC32/IEEE  : poly 0x04C11DB7 反射(0xEDB88320), init 0xFFFFFFFF,
 *                 结果异或输出 0xFFFFFFFF(即 zlib/PNG 兼容)
 *
 * 用法:
 *   一次性: crc = et_crc16_modbus(data, len);
 *   流式  : c = ET_CRC16_MODBUS_INIT;
 *           c = et_crc16_modbus_update(c, part1, n1);
 *           c = et_crc16_modbus_update(c, part2, n2);
 *
 * 说明: 位算法对典型 MCU 帧(几十~几百字节)性能足够; 若需更高吞吐,
 *       可在不变更 API 的前提下替换为查表实现。
 */
#ifndef ET_CRC_H
#define ET_CRC_H

#include <stdint.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_CRC

#ifdef __cplusplus
extern "C" {
#endif

#define ET_CRC8_INIT            0x00u
#define ET_CRC16_MODBUS_INIT    0xFFFFu
#define ET_CRC16_CCITT_INIT     0xFFFFu
#define ET_CRC32_INIT           0xFFFFFFFFu

/* ---- 流式更新: 从 seed 累续计算, 返回新 CRC ---- */
uint8_t  et_crc8_update(uint8_t crc, const void *data, uint32_t len);
uint16_t et_crc16_modbus_update(uint16_t crc, const void *data, uint32_t len);
uint16_t et_crc16_ccitt_update(uint16_t crc, const void *data, uint32_t len);
uint32_t et_crc32_update(uint32_t crc, const void *data, uint32_t len);

/* ---- 一次性计算 ---- */
uint8_t  et_crc8(const void *data, uint32_t len);
uint16_t et_crc16_modbus(const void *data, uint32_t len);
uint16_t et_crc16_ccitt(const void *data, uint32_t len);
uint32_t et_crc32(const void *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_CRC */
#endif /* ET_CRC_H */

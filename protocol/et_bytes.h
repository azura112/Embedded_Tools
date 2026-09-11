/**
 * @file    et_bytes.h
 * @brief   大端/小端 u16/u32 打包解包 (带边界检查, 零依赖)
 *
 * 定位 (v2.1 P3 决议):
 *  - 帧载荷/存储值/传输字段的字节序读写小件; et_frame_pack 只管其自身协议
 *    字段, 通用载荷仍由调用方拼装 —— 本模块补上该缺口;
 *  - 纯函数集合, 无状态、无硬件依赖, 可被任何层使用。
 *
 * 语义:
 *  - get/put 均带 **(buf,len,off) 边界检查**: 越界(NULL/len 不足/off 回绕)
 *    返回 false 且**不读不写**目标; 无未检查变体(安全面即唯一面);
 *  - off 用 uint32; 判定用 `off > len || len - off < size` 避免 off+size 回绕;
 *  - 字节序显式: be = 网络序(高字节在前), le = 低字节在前。
 */
#ifndef ET_BYTES_H
#define ET_BYTES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_BYTES

#ifdef __cplusplus
extern "C" {
#endif

/* 读: 成功写入 *out 并返回 true; 越界/空指针返回 false 且不写 *out */
bool et_bytes_be16_get(const uint8_t *buf, uint32_t len, uint32_t off, uint16_t *out);
bool et_bytes_be32_get(const uint8_t *buf, uint32_t len, uint32_t off, uint32_t *out);
bool et_bytes_le16_get(const uint8_t *buf, uint32_t len, uint32_t off, uint16_t *out);
bool et_bytes_le32_get(const uint8_t *buf, uint32_t len, uint32_t off, uint32_t *out);

/* 写: 成功返回 true; 越界/空指针返回 false 且不写 buf */
bool et_bytes_be16_put(uint8_t *buf, uint32_t len, uint32_t off, uint16_t val);
bool et_bytes_be32_put(uint8_t *buf, uint32_t len, uint32_t off, uint32_t val);
bool et_bytes_le16_put(uint8_t *buf, uint32_t len, uint32_t off, uint16_t val);
bool et_bytes_le32_put(uint8_t *buf, uint32_t len, uint32_t off, uint32_t val);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_BYTES */
#endif /* ET_BYTES_H */

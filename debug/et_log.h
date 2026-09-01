/**
 * @file    et_log.h
 * @brief   分级日志 (运行时过滤 + 编译期裁剪, 精简格式化器不依赖 libc printf)
 *
 * 输出格式:
 *   [时基ms][级别字符][标签] 正文\n
 *   例: [12345][E][uart] crc error
 *
 * 两级开关:
 *   - 编译期: ET_LOG_MAX_LEVEL 以上的宏整体消失(零代码体积);
 *   - 运行期: et_log_set_level() 动态调整过滤线。
 *
 * 格式化支持: %d %i %u %x %X %c %s %p %% 及 ld/li/lu/lx/lld/llu/llx 长度修饰。
 * 不支持浮点与域宽(嵌入式日志场景极少需要, 保持精简)。
 *
 * 并发说明: 默认不加锁, 多上下文并发输出可能交错; 如需原子行输出,
 *          可在上层用临界区包裹调用。
 */
#ifndef ET_LOG_H
#define ET_LOG_H

#include <stdint.h>
#include "et_config.h"

#if ET_MODULE_LOG

#ifdef __cplusplus
extern "C" {
#endif

/* 级别数值约定(越小越详细) */
#define ET_LOG_LEVEL_TRACE      0
#define ET_LOG_LEVEL_DEBUG      1
#define ET_LOG_LEVEL_INFO       2
#define ET_LOG_LEVEL_WARN       3
#define ET_LOG_LEVEL_ERROR      4
#define ET_LOG_LEVEL_NONE       5

/* 编译期裁剪线: 高于它的级别不产生任何代码(可用 -D 覆盖) */
#ifndef ET_LOG_MAX_LEVEL
#define ET_LOG_MAX_LEVEL        ET_LOG_LEVEL_TRACE
#endif

typedef enum {
    ET_LOG_TRACE = ET_LOG_LEVEL_TRACE,
    ET_LOG_DEBUG = ET_LOG_LEVEL_DEBUG,
    ET_LOG_INFO  = ET_LOG_LEVEL_INFO,
    ET_LOG_WARN  = ET_LOG_LEVEL_WARN,
    ET_LOG_ERROR = ET_LOG_LEVEL_ERROR,
} et_log_level_t;

/* 运行时过滤线(低于该级别的日志被丢弃) */
void           et_log_set_level(et_log_level_t lv);
et_log_level_t et_log_get_level(void);

/* 核心输出: 返回输出的字符数(-1 表示被过滤) */
int et_log_output(et_log_level_t lv, const char *tag, const char *fmt, ...);

/* 无前缀裸输出 */
int et_log_raw(const char *fmt, ...);

/* 十六进制转储: 偏移 + 16字节/行 + ASCII 列 */
void et_log_hexdump(et_log_level_t lv, const char *tag,
                    const void *data, uint32_t len);

/* ---- 便捷宏(受编译期裁剪控制) ---- */
#if ET_LOG_MAX_LEVEL <= ET_LOG_LEVEL_ERROR
#define ET_LOGE(tag, ...)   et_log_output(ET_LOG_LEVEL_ERROR, (tag), __VA_ARGS__)
#else
#define ET_LOGE(tag, ...)   ((void)0)
#endif

#if ET_LOG_MAX_LEVEL <= ET_LOG_LEVEL_WARN
#define ET_LOGW(tag, ...)   et_log_output(ET_LOG_LEVEL_WARN, (tag), __VA_ARGS__)
#else
#define ET_LOGW(tag, ...)   ((void)0)
#endif

#if ET_LOG_MAX_LEVEL <= ET_LOG_LEVEL_INFO
#define ET_LOGI(tag, ...)   et_log_output(ET_LOG_LEVEL_INFO, (tag), __VA_ARGS__)
#else
#define ET_LOGI(tag, ...)   ((void)0)
#endif

#if ET_LOG_MAX_LEVEL <= ET_LOG_LEVEL_DEBUG
#define ET_LOGD(tag, ...)   et_log_output(ET_LOG_LEVEL_DEBUG, (tag), __VA_ARGS__)
#else
#define ET_LOGD(tag, ...)   ((void)0)
#endif

#if ET_LOG_MAX_LEVEL <= ET_LOG_LEVEL_TRACE
#define ET_LOGT(tag, ...)   et_log_output(ET_LOG_LEVEL_TRACE, (tag), __VA_ARGS__)
#else
#define ET_LOGT(tag, ...)   ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_LOG */
#endif /* ET_LOG_H */

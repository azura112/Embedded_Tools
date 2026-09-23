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
 * 格式化支持 (v2.5 加固 / v2.6 修饰面收口):
 *   - **支持**: %d %i %u %x %X %c %s %p %% —— 并支持
 *     标志{- 0 + 空格 #} / 十进制域宽(含 `*` 从实参取) / 精度(`.N` 与 `.*`)
 *     / 长度修饰{h hh l ll z}。语义与 C printf 一致(补零/左对齐/符号/`#` 前缀/
 *     `%.Ns` 截断/`%.Nd` 最少位数/`%c` 的域宽与左对齐——v2.6 CO-7 起生效,
 *     按 C 语义 `%c` 的精度不起作用)。
 *   - **域宽上限**: 域宽与精度均截断至 ET_LOG_FIELD_MAX(超限按上限输出, 不无限刷字符)。
 *   - **已知不支持但按 C 语义消费实参**(标准 C 有定义, 库不格式化):
 *     %o %f %F %e %E %g %G %a %A(按 double 消费)、%n(消费指针, **禁写内存**)、
 *     宽字符 %lc/%ls、长度修饰 **j / t / L**(v2.6 CO-8: 分别按 intmax_t/
 *     ptrdiff_t/long double 消费各一次 —— 此前它们走"未知"分支不消费, 是错位的
 *     最后口子)。这些规格输出**可见占位**: 单字符转换 `<?c>`(如 `%f` → `<?f>`,
 *     `%lc` → `<?lc>`), 含长度修饰者输出完整字形 `<?jc>`(如 `%jd` → `<?jd>`,
 *     `%Lf` → `<?Lf>`)—— 既不是原样回显, 也不会让后续实参错位。
 *   - **未知转换字符**(非上述集合, 如 `%y`): 输出可见占位 `<?y>` 且**不消费实参**
 *     (此为例外, 见下"使用约定")。
 *
 * 使用约定 (v2.5 缺陷清偿的直接动因):
 *   - 旧版对不支持的规格"字面回显 `%02x` 并错位消费后续实参"(静默错误, 板上已咬人);
 *     新版按上述三类处置,**不再有静默错位**。若日志里出现 `<?...>`, 说明该规格
 *     不被支持 —— 请改用受支持规格, 而不要依赖占位字形。
 *   - 占位字形是**文档固定契约**, 全库一致; 字形变化视为行为变更。
 *   - `%n` 被明确拒绝写内存(嵌入式日志场景无此需求, 写内存是危险面)。
 *   - 浮点仍不格式化(`%f` 等只保证"可见占位 + 实参不错位"); 需要浮点请先自行定点化。
 *
 * 并发说明: 默认不加锁, 多上下文并发输出可能交错; 如需原子行输出,
 *          可在上层用临界区包裹调用。
 */
#ifndef ET_LOG_H
#define ET_LOG_H

#include <stdint.h>
#include <stddef.h>
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

/* 域宽/精度上限 (v2.5): 规格里的域宽与精度超过本值即按本值输出 ——
 * 防止 `%9999u` 之类在阻塞式 port_putc 上刷出上万字符。可用 -D 覆盖。 */
#ifndef ET_LOG_FIELD_MAX
#define ET_LOG_FIELD_MAX        255u
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

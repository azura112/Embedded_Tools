/**
 * @file    et_assert.h
 * @brief   轻量断言 (失败行为可插拔: 记录/停机/复位由钩子决定)
 *
 * 用法:
 *   - ET_DBG_ASSERT(cond): 仅在定义了 ET_DEBUG 时生效(零开销发布);
 *   - et_assert_install() 注册失败钩子(如记录日志后触发看门狗复位);
 *   - 未安装钩子时使用内置策略: 经 et_log 输出错误后原地停机,
 *     便于调试器定位/看门狗复位。
 *
 * 注意: 钩子返回后执行流将继续, 是否复位由钩子自行决定。
 */
#ifndef ET_ASSERT_H
#define ET_ASSERT_H

#include <stdbool.h>
#include "et_config.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*et_assert_fail_fn)(const char *file, int line,
                                  const char *expr, void *user);

/* 安装失败钩子(fn=NULL 恢复内置停机策略) */
void et_assert_install(et_assert_fail_fn fn, void *user);

/* 断言失败入口(通常经宏调用) */
void et_assert_fail(const char *file, int line, const char *expr);

#ifdef ET_DEBUG
#define ET_DBG_ASSERT(cond)                                                 \
    do {                                                                    \
        if (!(cond)) {                                                      \
            et_assert_fail(__FILE__, __LINE__, #cond);                      \
        }                                                                   \
    } while (0)
#else
#define ET_DBG_ASSERT(cond)     ((void)0)
#endif

#if defined(__cplusplus)
}
#endif

#endif /* ET_ASSERT_H */

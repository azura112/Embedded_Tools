/**
 * @file    et_test.h
 * @brief   轻量级单元测试框架 (仅宿主机 PC 环境使用, 不随库发布到 MCU)
 *
 * 用法:
 *   1. 各测试文件定义若干 void fn(void) 形式的用例函数;
 *   2. 组装为 static const et_test_case_t 表, 并提供获取接口;
 *   3. main 中调用 et_test_run("套件名", cases, count)。
 */
#ifndef ET_TEST_H
#define ET_TEST_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*et_test_case_fn)(void);

typedef struct {
    const char     *name;
    et_test_case_fn fn;
} et_test_case_t;

/* 框架内部状态(供断言宏与运行器之间传递失败信息) */
extern jmp_buf      et_test_jmp_buf;
extern const char  *et_test_file;
extern long         et_test_line;
extern char         et_test_msg[256];

/* 运行一组用例, 返回失败数 */
int  et_test_run(const char *suite, const et_test_case_t *cases, size_t count);

/* ---- 断言宏 ---- */

#define ET_FAIL(msg)                                                        \
    do {                                                                    \
        et_test_file = __FILE__;                                            \
        et_test_line = __LINE__;                                            \
        snprintf(et_test_msg, sizeof(et_test_msg), "%s", (const char *)(msg)); \
        longjmp(et_test_jmp_buf, 1);                                        \
    } while (0)

#define ET_CHECK(cond)                                                      \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ET_FAIL(#cond);                                                 \
        }                                                                   \
    } while (0)

#define ET_CHECK_U32_EQ(expected, actual)                                   \
    do {                                                                    \
        unsigned long _e = (unsigned long)(uint32_t)(expected);             \
        unsigned long _a = (unsigned long)(uint32_t)(actual);               \
        if (_e != _a) {                                                     \
            et_test_msg[0] = '\0';                                          \
            et_test_file = __FILE__;                                        \
            et_test_line = __LINE__;                                        \
            snprintf(et_test_msg, sizeof(et_test_msg),                      \
                     "%s expected=%lu(0x%lx) actual=%lu(0x%lx)",            \
                     #actual, _e, _e, _a, _a);                              \
            longjmp(et_test_jmp_buf, 1);                                    \
        }                                                                   \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* ET_TEST_H */

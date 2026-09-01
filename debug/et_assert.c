/**
 * @file    et_assert.c
 * @brief   轻量断言实现
 */
#include "et_assert.h"

#include <stddef.h>

#include "et_log.h"
#include "port.h"

static et_assert_fail_fn g_hook = NULL;
static void             *g_user = NULL;

void et_assert_install(et_assert_fail_fn fn, void *user)
{
    PORT_CRITICAL_ENTER();
    g_hook = fn;
    g_user = user;
    PORT_CRITICAL_EXIT();
}

void et_assert_fail(const char *file, int line, const char *expr)
{
    et_assert_fail_fn fn;

    PORT_CRITICAL_ENTER();
    fn     = g_hook;
    g_user = g_user;
    PORT_CRITICAL_EXIT();

    if (fn != NULL) {
        fn(file, line, expr, g_user);       /* 钩子决定后续(可不复位) */
        return;
    }

    /* 内置策略: 记录现场后停机, 便于调试器定位或等待看门狗复位 */
    (void)et_log_output(ET_LOG_LEVEL_ERROR, "ASSERT",
                        "%s:%d expr(%s)", file, line,
                        (expr != NULL) ? expr : "?");
    for (;;) {
        /* 可按平台替换为: 关中断 / WFI / NVIC_SystemReset() */
    }
}

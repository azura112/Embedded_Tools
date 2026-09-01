/**
 * @file    test_assert.c
 * @brief   et_assert 单元测试
 */
#include "et_test.h"
#include "et_assert.h"
#include <string.h>

static char     g_file[32];
static int      g_line;
static char     g_expr[32];
static uint32_t g_fails;
static void    *g_user_seen;

static void hook_rec(const char *file, int line, const char *expr, void *user)
{
    strncpy(g_file, file, sizeof(g_file) - 1u);
    g_line = line;
    strncpy(g_expr, expr, sizeof(g_expr) - 1u);
    g_user_seen = user;
    g_fails++;
}

static void assert_hook_captures(void)
{
    int magic = 7;

    g_fails = 0u;
    et_assert_install(hook_rec, &magic);

    et_assert_fail("somewhere.c", 77, "x == 1");

    ET_CHECK_U32_EQ(1u, g_fails);
    ET_CHECK(strstr(g_file, "somewhere.c") != NULL);
    ET_CHECK_U32_EQ(77u, (uint32_t)g_line);
    ET_CHECK(strcmp(g_expr, "x == 1") == 0);
    ET_CHECK(g_user_seen == &magic);            /* 用户指针透传 */

    et_assert_install(NULL, NULL);              /* 恢复内置策略(不停机验证) */
}

static void assert_dbg_macro_safe(void)
{
    g_fails = 0u;
    et_assert_install(hook_rec, NULL);

    ET_DBG_ASSERT(1 == 1);                      /* 通过路径无副作用 */

#ifdef ET_DEBUG
    {
        int x = 0;

        ET_DBG_ASSERT(x != 0);                  /* 发布构建下此分支不存在 */
        ET_CHECK_U32_EQ(1u, g_fails);
    }
#endif
    ET_CHECK_U32_EQ(0u, g_fails);               /* 未定义 ET_DEBUG 时完全无感 */

    et_assert_install(NULL, NULL);
}

const et_test_case_t *test_assert_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"assert.hook_captures", assert_hook_captures},
        {"assert.dbg_macro",     assert_dbg_macro_safe},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

/**
 * @file    test_shell_tab.c
 * @brief   shell Tab 命令名补全单元测试 (v2.0 P2)
 *
 * 双形态编译: 默认构建(ET_SHELL_TAB=0)断言"直透零变化";
 * `make test-tab`(-DET_SHELL_TAB=1)断言补全矩阵(唯一/全等/多候选/无匹配/
 * 大小写敏感/参数段/ESC 混发)。
 */
#include <stddef.h>
#include <string.h>

#include "et_test.h"
#include "et_atcmd.h"
#include "et_shell.h"

static char            g_out[1024];
static size_t          g_out_len;
static int             g_ping_cnt;
static et_atcmd_proc_t g_at;
static et_shell_t      g_sh;
static char            g_line[64];

static void cmd_ping(char *args, void *user)
{
    (void)args; (void)user;
    g_ping_cnt++;
}

static void out_putc(void *user, char ch)
{
    (void)user;
    if (g_out_len + 1u < sizeof(g_out)) {
        g_out[g_out_len++] = ch;
        g_out[g_out_len]   = '\0';
    }
}

/* 每用例独立夹具: 可写命令表 + atcmd + shell */
static void setup_tab(void)
{
    static et_atcmd_entry_t tbl[3];

    tbl[0].name = "PING"; tbl[0].fn = cmd_ping; tbl[0].help = "p";
    tbl[1].name = "PONG"; tbl[1].fn = cmd_ping; tbl[1].help = "q";
    tbl[2].name = "STAT"; tbl[2].fn = cmd_ping; tbl[2].help = "s";
    g_out_len  = 0u;
    g_out[0]   = '\0';
    g_ping_cnt = 0;
    memset(g_line, 0, sizeof(g_line));
    (void)et_atcmd_init(&g_at, tbl, 3u, g_line, sizeof(g_line), NULL);
    (void)et_shell_init(&g_sh, &g_at, out_putc, NULL);
}

static void feed_str(const char *s)
{
    while (*s != '\0') {
        (void)et_shell_feed(&g_sh, *s++);
    }
}

#if ET_SHELL_TAB
static void tab_unique_complete(void)
{
    setup_tab();
    feed_str("AT+PI");
    (void)et_shell_feed(&g_sh, (char)0x09);          /* Tab */
    ET_CHECK_U32_EQ(7u, (uint32_t)g_at.pos);          /* 补全 AT+PING */
    ET_CHECK(memcmp(g_line, "AT+PING", 7) == 0);
    ET_CHECK(strchr(g_out, (char)0x07) == NULL);      /* 无铃 */
    feed_str("\r\n");
    ET_CHECK_U32_EQ(1u, (uint32_t)g_ping_cnt);        /* 补全行可执行 */
}

static void tab_exact_full_silent(void)
{
    setup_tab();
    feed_str("AT+PING");
    (void)et_shell_feed(&g_sh, (char)0x09);
    ET_CHECK_U32_EQ(7u, (uint32_t)g_at.pos);
    ET_CHECK(strchr(g_out, (char)0x07) == NULL);      /* 全等: 无补无铃 */
}

static void tab_multi_lists(void)
{
    setup_tab();
    feed_str("AT+P");
    (void)et_shell_feed(&g_sh, (char)0x09);
    ET_CHECK(strstr(g_out, "AT+PING") != NULL);
    ET_CHECK(strstr(g_out, "AT+PONG") != NULL);
    ET_CHECK(strstr(g_out, "AT+STAT") == NULL);       /* 非前缀不列 */
    ET_CHECK_U32_EQ(4u, (uint32_t)g_at.pos);          /* 行未被改写 */
}

static void tab_no_match_bell(void)
{
    setup_tab();
    feed_str("AT+Z");
    (void)et_shell_feed(&g_sh, (char)0x09);
    ET_CHECK(strchr(g_out, (char)0x07) != NULL);
}

static void tab_case_sensitive(void)
{
    setup_tab();
    feed_str("at+pi");                                 /* 小写不合 atcmd 规则 */
    (void)et_shell_feed(&g_sh, (char)0x09);
    ET_CHECK(strchr(g_out, (char)0x07) != NULL);       /* 大小写敏感 → 铃 */
    ET_CHECK_U32_EQ(5u, (uint32_t)g_at.pos);
}

static void tab_after_space_noop(void)
{
    setup_tab();
    feed_str("AT+PING 1");
    (void)et_shell_feed(&g_sh, (char)0x09);
    ET_CHECK_U32_EQ(9u, (uint32_t)g_at.pos);           /* 参数段: 原样("AT+PING 1"=9) */
    ET_CHECK(strchr(g_out, (char)0x07) == NULL);
    ET_CHECK(strstr(g_out, "AT+PONG") == NULL);
}

static void tab_with_esc_mix(void)
{
    setup_tab();
    feed_str("AT+PING");
    (void)et_shell_feed(&g_sh, (char)0x1B);            /* ESC 直透入行(无历史) */
    (void)et_shell_feed(&g_sh, (char)0x09);            /* 前缀完整但超全长 → 铃 */
    ET_CHECK(strchr(g_out, (char)0x07) != NULL);
    ET_CHECK_U32_EQ(8u, (uint32_t)g_at.pos);
}
#endif /* ET_SHELL_TAB */

static void tab_compiled_out_default(void)
{
#if !ET_SHELL_TAB
    setup_tab();
    feed_str("AT+PING");
    (void)et_shell_feed(&g_sh, (char)0x09);            /* 关闭态: 直透入行 */
    ET_CHECK_U32_EQ(8u, (uint32_t)g_at.pos);           /* 无补全介入 */
    feed_str("\r\n");
    ET_CHECK(strchr(g_out, (char)0x07) == NULL);       /* 亦无铃 */
#else
    ET_CHECK(1);                                       /* 开关版跳过 */
#endif
}

const et_test_case_t *test_shell_tab_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
#if ET_SHELL_TAB
        {"tab.unique_complete",       tab_unique_complete},
        {"tab.exact_full_silent",     tab_exact_full_silent},
        {"tab.multi_lists",           tab_multi_lists},
        {"tab.no_match_bell",         tab_no_match_bell},
        {"tab.case_sensitive",        tab_case_sensitive},
        {"tab.after_space_noop",      tab_after_space_noop},
        {"tab.with_esc_mix",          tab_with_esc_mix},
#endif
        {"tab.compiled_out_default",  tab_compiled_out_default},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

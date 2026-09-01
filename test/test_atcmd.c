/**
 * @file    test_atcmd.c
 * @brief   et_atcmd 单元测试
 */
#include "et_test.h"
#include "et_atcmd.h"
#include <string.h>

#define LINE_CAP 32

static char            g_line[LINE_CAP];
static et_atcmd_proc_t g_proc;

static char     g_hit_cmd[16];
static char     g_hit_args[24];
static uint32_t g_hit_cnt;
static char     g_unknown_name[16];
static uint32_t g_unknown_cnt;

static void cmd_ping(char *args, void *user)
{
    (void)user;
    g_hit_cnt++;
    strncpy(g_hit_cmd, "PING", sizeof(g_hit_cmd) - 1u);
    if (args != NULL) {
        strncpy(g_hit_args, args, sizeof(g_hit_args) - 1u);
        g_hit_args[sizeof(g_hit_args) - 1u] = '\0';
    }
}

static void cmd_echo(char *args, void *user)
{
    (void)user;
    g_hit_cnt++;
    strncpy(g_hit_cmd, "ECHO", sizeof(g_hit_cmd) - 1u);
    if (args != NULL) {
        strncpy(g_hit_args, args, sizeof(g_hit_args) - 1u);
        g_hit_args[sizeof(g_hit_args) - 1u] = '\0';
    }
}

static void on_unknown(const char *name, uint16_t name_len,
                       const char *args, void *user)
{
    (void)user;
    (void)args;
    g_unknown_cnt++;
    if (name_len < sizeof(g_unknown_name)) {
        memcpy(g_unknown_name, name, name_len);
        g_unknown_name[name_len] = '\0';
    }
}

static const et_atcmd_entry_t g_cmds[] = {
    { "PING", cmd_ping },
    { "ECHO", cmd_echo },
};

static bool feed_str(et_atcmd_proc_t *p, const char *s)
{
    bool hit = false;

    while (*s != '\0') {
        if (et_atcmd_feed(p, *s++)) {
            hit = true;
        }
    }
    return hit;
}

static void setup(void)
{
    ET_CHECK(et_atcmd_init(&g_proc, g_cmds, 2u, g_line, LINE_CAP, NULL));
    /* 挂未知命令回调 */
    g_proc.on_unknown = on_unknown;
    g_hit_cnt = 0u;
    g_unknown_cnt = 0u;
    g_hit_cmd[0] = '\0';
    g_hit_args[0] = '\0';
    g_unknown_name[0] = '\0';
}

static void at_dispatch_basic(void)
{
    setup();
    ET_CHECK(feed_str(&g_proc, "AT+PING hello\r\n"));
    ET_CHECK_U32_EQ(1u, g_hit_cnt);
    ET_CHECK(strcmp(g_hit_cmd, "PING") == 0);
    ET_CHECK(strcmp(g_hit_args, "hello") == 0);
    ET_CHECK_U32_EQ(0u, g_unknown_cnt);
}

static void at_lf_only_terminator(void)
{
    setup();
    ET_CHECK(feed_str(&g_proc, "AT+ECHO world\n"));
    ET_CHECK_U32_EQ(1u, g_hit_cnt);
    ET_CHECK(strcmp(g_hit_args, "world") == 0);
}

static void at_empty_arg_and_line(void)
{
    setup();
    ET_CHECK(feed_str(&g_proc, "AT+ECHO\r\n"));     /* 无参数调用 */
    ET_CHECK_U32_EQ(1u, g_hit_cnt);
    ET_CHECK(strcmp(g_hit_args, "") == 0);

    ET_CHECK(!feed_str(&g_proc, "\r\n"));           /* 空行忽略 */
    ET_CHECK_U32_EQ(1u, g_hit_cnt);
}

static void at_unknown_cb(void)
{
    setup();
    ET_CHECK(feed_str(&g_proc, "AT+NOPE x y\r\n"));
    ET_CHECK_U32_EQ(0u, g_hit_cnt);
    ET_CHECK_U32_EQ(1u, g_unknown_cnt);
    ET_CHECK(strcmp(g_unknown_name, "NOPE") == 0);
}

static void at_case_sensitive(void)
{
    setup();
    /* 小写前缀不识别为 AT 命令 */
    ET_CHECK(!feed_str(&g_proc, "at+ping\r\n"));
    ET_CHECK_U32_EQ(0u, g_hit_cnt);
    /* 大小写敏感的命令名: PINGX 不匹配 PING */
    ET_CHECK(feed_str(&g_proc, "AT+Ping\r\n"));
    ET_CHECK_U32_EQ(0u, g_hit_cnt);
    ET_CHECK_U32_EQ(1u, g_unknown_cnt);
}

static void at_backspace_editing(void)
{
    setup();
    /* 输入 AT+PING 后退格删掉 G 与 N, 再补 NG => 最终仍为 PING */
    ET_CHECK(feed_str(&g_proc, "AT+PING\b\bNG now\r\n"));
    ET_CHECK_U32_EQ(1u, g_hit_cnt);
    ET_CHECK(strcmp(g_hit_cmd, "PING") == 0);
    ET_CHECK(strcmp(g_hit_args, "now") == 0);
}

static void at_non_at_line_rejected(void)
{
    setup();
    ET_CHECK(!feed_str(&g_proc, "reset\r\n"));
    ET_CHECK(!feed_str(&g_proc, "AT no plus\r\n"));
    ET_CHECK_U32_EQ(0u, g_hit_cnt);
}

static void at_overflow_discard_recovers(void)
{
    setup();
    /* 超过行缓冲的行: 丢弃至行尾且不误触发 */
    ET_CHECK(!feed_str(&g_proc, "AT+AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"));
    ET_CHECK_U32_EQ(0u, g_hit_cnt);

    ET_CHECK(feed_str(&g_proc, "AT+PING ok\r\n"));  /* 随后恢复正常 */
    ET_CHECK_U32_EQ(1u, g_hit_cnt);
    ET_CHECK(strcmp(g_hit_args, "ok") == 0);
}

static void at_sequential_commands(void)
{
    setup();
    feed_str(&g_proc, "AT+PING a\r");
    feed_str(&g_proc, "\n");                        /* CRLF 分离到达也正确 */
    feed_str(&g_proc, "AT+ECHO b c d\r\n");
    ET_CHECK_U32_EQ(2u, g_hit_cnt);
    ET_CHECK(strcmp(g_hit_args, "b c d") == 0);
}

static void at_reset_drops_partial(void)
{
    setup();
    feed_str(&g_proc, "AT+PIN");                    /* 半行 */
    et_atcmd_reset(&g_proc);
    ET_CHECK(!feed_str(&g_proc, "G\r\n"));          /* 半行已被丢弃 */
    ET_CHECK_U32_EQ(0u, g_hit_cnt);
}

static void at_next_arg_tokenizer(void)
{
    char  buf[32];
    char *cursor = buf;
    char *a;
    char *b;
    char *c;

    setup();
    strcpy(buf, "one two three");
    a = et_atcmd_next_arg(&cursor);
    b = et_atcmd_next_arg(&cursor);
    c = et_atcmd_next_arg(&cursor);
    ET_CHECK((a != NULL) && (strcmp(a, "one") == 0));
    ET_CHECK((b != NULL) && (strcmp(b, "two") == 0));
    ET_CHECK((c != NULL) && (strcmp(c, "three") == 0));
    ET_CHECK(et_atcmd_next_arg(&cursor) == NULL);   /* 耗尽返回 NULL */

    strcpy(buf, "");
    ET_CHECK(et_atcmd_next_arg(&cursor) == NULL);   /* 空串安全 */

    strcpy(buf, "single");
    cursor = buf;
    a = et_atcmd_next_arg(&cursor);
    ET_CHECK((a != NULL) && (strcmp(a, "single") == 0));
}

const et_test_case_t *test_atcmd_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"atcmd.dispatch_basic",   at_dispatch_basic},
        {"atcmd.lf_terminator",    at_lf_only_terminator},
        {"atcmd.empty_arg_line",   at_empty_arg_and_line},
        {"atcmd.unknown_cb",       at_unknown_cb},
        {"atcmd.case_sensitive",   at_case_sensitive},
        {"atcmd.backspace_edit",   at_backspace_editing},
        {"atcmd.non_at_line",      at_non_at_line_rejected},
        {"atcmd.overflow_recover", at_overflow_discard_recovers},
        {"atcmd.sequential",       at_sequential_commands},
        {"atcmd.reset_partial",    at_reset_drops_partial},
        {"atcmd.next_arg_helper",  at_next_arg_tokenizer},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

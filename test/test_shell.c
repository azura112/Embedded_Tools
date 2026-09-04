/**
 * @file    test_shell.c
 * @brief   et_shell 单元测试 (覆盖 v1.4 计划 P2-1 全部验收点)
 */
#include <stddef.h>
#include <string.h>

#include "et_test.h"
#include "et_atcmd.h"
#include "et_shell.h"

/* ---- 输出捕获 ---- */
static char g_out[1024];
static size_t g_out_len;

static void out_reset(void)
{
    g_out_len = 0u;
    g_out[0]  = '\0';
}

static void out_putc(void *user, char ch)
{
    (void)user;
    if (g_out_len + 1u < sizeof(g_out)) {
        g_out[g_out_len++] = ch;
        g_out[g_out_len]   = '\0';
    }
}

/* ---- 命令记录 ---- */
static int      g_exec_cnt;
static char     g_last_args[32];
static int      g_unknown_cnt;

static void reset_all(void)
{
    out_reset();
    g_exec_cnt    = 0;
    g_unknown_cnt = 0;
    g_last_args[0] = '\0';
}

static void cmd_ping(char *args, void *user)
{
    (void)user;
    g_exec_cnt++;
    if (args != NULL) {
        strncpy(g_last_args, args, sizeof(g_last_args) - 1u);
        g_last_args[sizeof(g_last_args) - 1u] = '\0';
    }
}

static void cmd_on_unknown(const char *name, uint16_t name_len,
                           const char *args, void *user)
{
    (void)name; (void)name_len; (void)args; (void)user;
    g_unknown_cnt++;
}

static const et_atcmd_entry_t g_cmds[] = {
    { "PING", cmd_ping, "ping the shell" },
    { "ECHO2", cmd_ping, NULL },            /* 无帮助文本 */
    { "HELP", et_shell_help_cmd, "list commands" },
};

static et_atcmd_proc_t g_at;
static et_shell_t      g_sh;
static char            g_line[32];

static void sh_fresh(void)
{
    reset_all();
    memset(&g_at, 0, sizeof(g_at));
    memset(&g_sh, 0, sizeof(g_sh));
    (void)et_atcmd_init(&g_at, g_cmds, 3u, g_line, sizeof(g_line), &g_sh);
    g_at.on_unknown = cmd_on_unknown;
    (void)et_shell_init(&g_sh, &g_at, out_putc, NULL);
}

static void feed_str(const char *s)
{
    while (*s != '\0') {
        (void)et_shell_feed(&g_sh, *s++);
    }
}

/* ---- 用例 ---- */

static void init_validation(void)
{
    ET_CHECK(!et_shell_init(NULL, &g_at, out_putc, NULL));
    ET_CHECK(!et_shell_init(&g_sh, NULL, out_putc, NULL));
    ET_CHECK(!et_shell_init(&g_sh, &g_at, NULL, NULL));
    sh_fresh();
    ET_CHECK(g_sh.echo);                    /* 默认回显开 */
    ET_CHECK(g_sh.erase_seq);
    ET_CHECK(g_sh.prompt == NULL);
}

static void echo_printable_and_execution(void)
{
    sh_fresh();
    feed_str("AT+PING\r");
    ET_CHECK_U32_EQ(1, g_exec_cnt);
    ET_CHECK(strcmp(g_out, "AT+PING\r\n") == 0);    /* 回显 + CRLF, 无提示符 */
}

static void echo_off(void)
{
    sh_fresh();
    et_shell_set_echo(&g_sh, false);
    feed_str("AT+PING\r");
    ET_CHECK_U32_EQ(1, g_exec_cnt);
    ET_CHECK_U32_EQ(0, g_out_len);          /* 无任何输出 */
}

static void echo_crlf_pair_single(void)
{
    sh_fresh();
    feed_str("AT+PING\r\n");                /* CRLF 对: 只回显一个换行 */
    ET_CHECK_U32_EQ(1, g_exec_cnt);
    ET_CHECK(strcmp(g_out, "AT+PING\r\n") == 0);
}

static void backspace_erases(void)
{
    sh_fresh();
    feed_str("AB\b\b");                     /* 删掉 B 和 A, 行回到空 */
    /* 回显: 'A','B' 后跟两个擦写序列 "\b \x1b[K" */
    ET_CHECK(strcmp(g_out, "AB\b \x1b[K\b \x1b[K") == 0);
    feed_str("AT+PING\r");
    ET_CHECK_U32_EQ(1, g_exec_cnt);         /* 退格后的行仍正确解析 */
}

static void backspace_at_line_start(void)
{
    sh_fresh();
    (void)et_shell_feed(&g_sh, '\b');
    (void)et_shell_feed(&g_sh, 0x7F);
    ET_CHECK_U32_EQ(0, g_out_len);          /* 行首边界: 无任何回显 */
    feed_str("AT+PING\r");
    ET_CHECK_U32_EQ(1, g_exec_cnt);         /* 功能不受影响 */
}

static void backspace_erase_off(void)
{
    sh_fresh();
    et_shell_set_erase(&g_sh, false);
    feed_str("AB\b");
    ET_CHECK(strcmp(g_out, "AB") == 0);     /* 擦写序列关: 无 \b 回显 */
}

static void long_line_discarded_still_works(void)
{
    sh_fresh();
    feed_str("AT+XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r");  /* > 32B 行缓冲 */
    ET_CHECK_U32_EQ(0, g_exec_cnt);         /* 超长行被丢弃 */
    feed_str("AT+PING\r");
    ET_CHECK_U32_EQ(1, g_exec_cnt);         /* shell 仍正常 */
}

static void help_listing_lines(void)
{
    sh_fresh();
    et_shell_print_help(&g_sh);
    /* 逐行比对: 有 help 输出 " - 文本", NULL help 只输出名称 */
    ET_CHECK(strcmp(g_out,
                    "PING - ping the shell\r\n"
                    "ECHO2\r\n"
                    "HELP - list commands\r\n") == 0);
}

static void help_via_command(void)
{
    sh_fresh();
    out_reset();
    feed_str("AT+HELP\r");                  /* 预置 HELP 命令经 shell 生效 */
    ET_CHECK(strstr(g_out, "PING - ping the shell\r\n") != NULL);
    ET_CHECK(strstr(g_out, "HELP - list commands\r\n") != NULL);
}

static void prompt_print_and_auto(void)
{
    sh_fresh();
    et_shell_set_prompt(&g_sh, "> ");
    ET_CHECK(strcmp(g_out, "> ") == 0);     /* set_prompt 立即打印 */
    out_reset();
    feed_str("AT+PING\r");
    ET_CHECK_U32_EQ(1, g_exec_cnt);
    /* 命令执行后自动补提示符: 回显行 + CRLF + "> " */
    ET_CHECK(strcmp(g_out, "AT+PING\r\n> ") == 0);
}

static void prompt_null_hidden(void)
{
    sh_fresh();
    feed_str("AT+PING\r");
    ET_CHECK(strstr(g_out, "> ") == NULL);  /* 未设置提示符: 无输出 */
}

static void atcmd_unknown_passthrough(void)
{
    sh_fresh();
    feed_str("AT+NOPE\r");
    ET_CHECK_U32_EQ(1, g_unknown_cnt);      /* on_unknown 透传不丢 */
    ET_CHECK_U32_EQ(0, g_exec_cnt);
}

static void args_passthrough(void)
{
    sh_fresh();
    feed_str("AT+PING 1 2 3\r");
    ET_CHECK_U32_EQ(1, g_exec_cnt);
    ET_CHECK(strcmp(g_last_args, "1 2 3") == 0);
}

static void control_bytes_not_echoed(void)
{
    sh_fresh();
    (void)et_shell_feed(&g_sh, 0x03);       /* 非退格控制字节: 不回显 */
    (void)et_shell_feed(&g_sh, 0x1B);
    ET_CHECK_U32_EQ(0, g_out_len);
    (void)et_shell_feed(&g_sh, '\r');       /* atcmd 字节流语义: 冲掉控制字节占位 */
    feed_str("AT+PING\r");
    ET_CHECK_U32_EQ(1, g_exec_cnt);
}

static const et_test_case_t g_cases[] = {
    { "init.validation",        init_validation },
    { "echo.printable_exec",    echo_printable_and_execution },
    { "echo.off",               echo_off },
    { "echo.crlf_pair",         echo_crlf_pair_single },
    { "bs.erases",              backspace_erases },
    { "bs.line_start_boundary", backspace_at_line_start },
    { "bs.erase_off",           backspace_erase_off },
    { "line.long_discarded",    long_line_discarded_still_works },
    { "help.listing",           help_listing_lines },
    { "help.via_command",       help_via_command },
    { "prompt.auto_and_set",    prompt_print_and_auto },
    { "prompt.null_hidden",     prompt_null_hidden },
    { "atcmd.unknown_pass",     atcmd_unknown_passthrough },
    { "atcmd.args_pass",        args_passthrough },
    { "echo.ctrl_not_echoed",   control_bytes_not_echoed },
};

const et_test_case_t *test_shell_cases(size_t *count)
{
    *count = sizeof(g_cases) / sizeof(g_cases[0]);
    return g_cases;
}

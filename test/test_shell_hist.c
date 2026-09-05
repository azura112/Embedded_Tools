/**
 * @file    test_shell_hist.c
 * @brief   et_shell 历史功能单元测试 (ET_SHELL_HISTORY_N>0 构建变体)
 *
 * 覆盖 (计划 P2-2): 上键回放 / 环形覆盖最旧 / 历史中编辑后回车 /
 * 游标复位 / 下键回实时行 / 超长条目截断。
 * 本文件在默认构建 (ET_SHELL_HISTORY_N=0) 下编译为空套件。
 */
#include <stddef.h>
#include <string.h>

#include "et_test.h"
#include "et_atcmd.h"
#include "et_shell.h"

#if ET_SHELL_HISTORY_N > 0

#define HIST_N      3u
#define HIST_CAP    24u

static char              g_hist[HIST_N * HIST_CAP];
static et_atcmd_proc_t   g_at;
static et_shell_t        g_sh;
static char              g_line[32];
static int               g_exec_cnt;

static void out_sink(void *user, char ch)
{
    (void)user;
    (void)ch;                               /* 历史测试不校验回显内容 */
}

static void cmd_cap(char *args, void *user)
{
    (void)args;
    (void)user;
    g_exec_cnt++;
}

static const et_atcmd_entry_t g_cmds[] = {
    { "PING", cmd_cap, NULL },
    { "VER",  cmd_cap, NULL },
    { "INFO", cmd_cap, NULL },
    { "FOO",  cmd_cap, NULL },
};

static void hist_fresh(void)
{
    memset(g_hist, 0, sizeof(g_hist));
    memset(&g_at, 0, sizeof(g_at));
    memset(&g_sh, 0, sizeof(g_sh));
    g_exec_cnt = 0;
    (void)et_atcmd_init(&g_at, g_cmds, 4u, g_line, sizeof(g_line), NULL);
    (void)et_shell_init(&g_sh, &g_at, out_sink, NULL);
    ET_CHECK(et_shell_set_history(&g_sh, g_hist, HIST_N, HIST_CAP));
}

static void feed_str(const char *s)
{
    while (*s != '\0') {
        (void)et_shell_feed(&g_sh, *s++);
    }
}

/* ---- 用例 ---- */

static void hist_up_replay_and_exec(void)
{
    hist_fresh();
    feed_str("AT+PING\r");                  /* 第 1 条入历史 */
    feed_str("\r");                         /* 空行不入历史 */
    feed_str("\x1b[A");                     /* 上键回放 */
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+PING") == 0);
    feed_str("\r");
    ET_CHECK_U32_EQ(2, g_exec_cnt);         /* 首条 1 + 回放条 1 */
}

static void hist_ring_overwrite_oldest(void)
{
    hist_fresh();
    feed_str("AT+PING\r");
    feed_str("AT+VER\r");
    feed_str("AT+INFO\r");
    feed_str("AT+FOO\r");                   /* 第 4 条挤掉最旧 AT+PING */

    feed_str("\x1b[A");                     /* 最新 AT+FOO */
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+FOO") == 0);
    feed_str("\x1b[A");
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+INFO") == 0);
    feed_str("\x1b[A");
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+VER") == 0);
    feed_str("\x1b[A");                     /* 已到最旧: 停住 (PING 已被挤掉) */
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+VER") == 0);
}

static void hist_edit_then_enter(void)
{
    hist_fresh();
    feed_str("AT+VER\r");
    feed_str("\x1b[A");                     /* 回放 AT+VER */
    feed_str("X");                          /* 行内编辑: 追加 X */
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+VERX") == 0);
    feed_str("\r");
    /* "AT+VERX" 未知命令不执行; 计数 = 首条 VER 的 1 次 */
    ET_CHECK_U32_EQ(1, g_exec_cnt);
    feed_str("\x1b[A");
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+VERX") == 0);
}

static void hist_cursor_resets_after_enter(void)
{
    hist_fresh();
    feed_str("AT+PING\r");
    feed_str("AT+VER\r");
    feed_str("AT+INFO\r");
    feed_str("AT+FOO\r");
    feed_str("\x1b[A\x1b[A\x1b[A");         /* 浏览到最旧 AT+VER */
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+VER") == 0);
    feed_str("X\r");                        /* 编辑后回车: 入历史+游标复位 */
    feed_str("\x1b[A");
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+VERX") == 0);   /* 从最新重新开始 */
}

static void hist_down_returns_to_live(void)
{
    hist_fresh();
    feed_str("AT+PING\r");
    feed_str("\x1b[A");                     /* 回放 */
    ET_CHECK(strcmp(g_sh.at->linebuf, "AT+PING") == 0);
    feed_str("\x1b[B");                     /* 下键回实时行 */
    ET_CHECK_U32_EQ(0u, g_sh.at->pos);      /* 行清空 */
    feed_str("\x1b[B\x1b[B");               /* 实时行再下: 无事发生 */
    ET_CHECK_U32_EQ(0u, g_sh.at->pos);
    feed_str("AT+PING\r");
    ET_CHECK_U32_EQ(2, g_exec_cnt);         /* 首条 + 清行后重输的这条 */
}

static void hist_entry_truncated(void)
{
    /* 28 字符行: 在 atcmd 行缓冲(31)内, 但超过 hist_cap-1(23) → 入库截断 */
    hist_fresh();
    feed_str("AT+VERXXXXXXXXXXXXXXXXXXXXXX\r");          /* 6 + 22 = 28 */
    feed_str("\x1b[A");
    /* 条目按 entry_cap-1 截断且 NUL 结尾 */
    ET_CHECK(strlen(g_sh.at->linebuf) == HIST_CAP - 1u);
    feed_str("\r");
    ET_CHECK_U32_EQ(0, g_exec_cnt);         /* 截断行非完整命令名, 不执行 */
}

static const et_test_case_t g_cases[] = {
    { "hist.up_replay_exec",    hist_up_replay_and_exec },
    { "hist.ring_overwrite",    hist_ring_overwrite_oldest },
    { "hist.edit_then_enter",   hist_edit_then_enter },
    { "hist.cursor_reset",      hist_cursor_resets_after_enter },
    { "hist.down_to_live",      hist_down_returns_to_live },
    { "hist.entry_truncated",   hist_entry_truncated },
};

const et_test_case_t *test_shell_hist_cases(size_t *count)
{
    *count = sizeof(g_cases) / sizeof(g_cases[0]);
    return g_cases;
}

#else /* ET_SHELL_HISTORY_N == 0: 默认构建编译空套件保持注册表完整 */

const et_test_case_t *test_shell_hist_cases(size_t *count)
{
    *count = 0u;
    return NULL;
}

#endif

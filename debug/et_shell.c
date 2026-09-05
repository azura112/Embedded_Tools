/**
 * @file    et_shell.c
 * @brief   行式交互壳实现 (回显/退格擦写/提示符/help 自动生成)
 *
 * 行编辑语义(删字/超长丢弃)由底层 et_atcmd 决定, 本模块只决定"回显什么",
 * 见 et_shell.h 头注。
 */
#include "et_shell.h"

#if ET_MODULE_SHELL

#include <string.h>

#if ET_SHELL_HISTORY_N > 0
static bool shell_hist_escape(et_shell_t *sh, char ch);
static void shell_hist_push(et_shell_t *sh);
#endif

/* 退格擦写序列: 光标左移 + 空格覆盖 + ANSI 清行尾(计划 P2-1 指定) */
#define SHELL_ERASE_SEQ     "\b \x1b[K"
#define SHELL_CRLF          "\r\n"

bool et_shell_init(et_shell_t *sh, et_atcmd_proc_t *at,
                   et_shell_putc_fn putc, void *user)
{
    if ((sh == NULL) || (at == NULL) || (putc == NULL)) {
        return false;
    }
    sh->at         = at;
    sh->putc       = putc;
    sh->user       = user;
    sh->prompt     = NULL;
    sh->echo       = true;
    sh->erase_seq  = true;
    sh->cr_pending = false;
    return true;
}

void et_shell_set_echo(et_shell_t *sh, bool on)
{
    if (sh != NULL) {
        sh->echo = on;
    }
}

void et_shell_set_erase(et_shell_t *sh, bool on)
{
    if (sh != NULL) {
        sh->erase_seq = on;
    }
}

void et_shell_set_prompt(et_shell_t *sh, const char *prompt)
{
    if (sh == NULL) {
        return;
    }
    sh->prompt = prompt;
    et_shell_prompt(sh);
}

void et_shell_prompt(et_shell_t *sh)
{
    const char *p;

    if ((sh == NULL) || (sh->prompt == NULL)) {
        return;
    }
    for (p = sh->prompt; *p != '\0'; p++) {
        sh->putc(sh->user, *p);
    }
}

static void shell_out(et_shell_t *sh, const char *s)
{
    while (*s != '\0') {
        sh->putc(sh->user, *s++);
    }
}

bool et_shell_feed(et_shell_t *sh, char ch)
{
    bool done;

    if ((sh == NULL) || (sh->at == NULL)) {
        return false;
    }

#if ET_SHELL_HISTORY_N > 0
    /* ---- 历史转义序列拦截 (ESC [ A/B) ---- */
    if ((sh->hist_buf != NULL) && (sh->esc_state != 0u)) {
        (void)shell_hist_escape(sh, ch);
        return false;
    }
    if ((ch == 0x1B) && (sh->hist_buf != NULL)) {
        sh->esc_state = 1u;
        return false;
    }
#endif

    /* ---- 回显层 ---- */
    if ((ch >= 0x20) && (ch <= 0x7E)) {
        if (sh->echo) {
            sh->putc(sh->user, ch);
        }
        sh->cr_pending = false;
    } else if ((ch == '\r') || (ch == '\n')) {
        if (sh->echo && !(sh->cr_pending && (ch == '\n'))) {
            shell_out(sh, SHELL_CRLF);
        }
        sh->cr_pending = (ch == '\r');
    } else if ((ch == '\b') || (ch == 0x7F)) {
        /* 行首边界: 行内无字符可删时不回显擦写序列 */
        if (sh->echo && sh->erase_seq && (sh->at->pos > 0u)) {
            shell_out(sh, SHELL_ERASE_SEQ);
        }
        sh->cr_pending = false;
    } else {
        sh->cr_pending = false;         /* 其余控制字节不回显 */
    }

    /* ---- 行完成前压入历史 (取本行最终内容; 超长丢弃行不入) ---- */
#if ET_SHELL_HISTORY_N > 0
    if (((ch == '\r') || (ch == '\n')) && (sh->hist_buf != NULL) &&
        (sh->at->pos > 0u) && (!sh->at->discarding)) {
        shell_hist_push(sh);
    }
#endif

    /* ---- 解析透传(行语义全部由 atcmd 决定) ---- */
    done = et_atcmd_feed(sh->at, ch);
    if (done) {
        et_shell_prompt(sh);            /* 命令执行完自动补提示符 */
    }
    return done;
}

void et_shell_print_help(et_shell_t *sh)
{
    const et_atcmd_entry_t *e;
    const char *h;
    uint16_t    i;

    if ((sh == NULL) || (sh->at == NULL)) {
        return;
    }
    for (i = 0u; i < sh->at->cmd_count; i++) {
        e = &sh->at->cmds[i];
        shell_out(sh, e->name);
        if (e->help != NULL) {
            shell_out(sh, " - ");
            h = e->help;
            while (*h != '\0') {
                sh->putc(sh->user, *h++);
            }
        }
        shell_out(sh, SHELL_CRLF);
    }
}

void et_shell_puts(et_shell_t *sh, const char *s)
{
    if (sh != NULL) {
        shell_out(sh, s);
    }
}

bool et_shell_set_history(et_shell_t *sh, char *storage,
                          uint16_t entries, uint16_t entry_cap)
{
    if ((sh == NULL) || (storage == NULL) || (entries == 0u) || (entry_cap < 4u)) {
        return false;
    }
#if ET_SHELL_HISTORY_N > 0
    if (entries > (uint16_t)ET_SHELL_HISTORY_N) {
        return false;
    }
    sh->hist_buf   = storage;
    sh->hist_n     = entries;
    sh->hist_cap   = entry_cap;
    sh->hist_count = 0u;
    sh->hist_head  = 0u;
    sh->hist_view  = -1;
    sh->esc_state  = 0u;
    return true;
#else
    (void)entries;
    (void)entry_cap;
    return false;                       /* 编译期关闭: 字节直透模式 */
#endif
}

#if ET_SHELL_HISTORY_N > 0

/* 用退格序列清掉当前行, 再回放 line (经 atcmd 逐字入行 + 回显)。
 * atcmd 的行缓冲在输入过程中不补 NUL, 回放后须清理尾部残留,
 * 保证 linebuf 与所见字符串一致 (后续 push/编辑依赖此约定) */
static void shell_hist_replace(et_shell_t *sh, const char *line)
{
    uint16_t i;

    while (sh->at->pos > 0u) {
        if (sh->echo && sh->erase_seq) {
            shell_out(sh, SHELL_ERASE_SEQ);
        }
        (void)et_atcmd_feed(sh->at, '\b');
    }
    for (; *line != '\0'; line++) {
        if (sh->echo) {
            sh->putc(sh->user, *line);
        }
        (void)et_atcmd_feed(sh->at, *line);
    }
    for (i = sh->at->pos; i < sh->at->line_cap; i++) {
        sh->at->linebuf[i] = '\0';     /* 清到缓冲尾: 杜绝旧行残留字符 */
    }
}

/* 环形槽号: view 0 = 最新 */
static uint16_t shell_hist_index(const et_shell_t *sh, uint16_t view)
{
    return (uint16_t)((sh->hist_head + sh->hist_n - 1u - view) % sh->hist_n);
}

static void shell_hist_browse(et_shell_t *sh, bool up)
{
    char   *entry;
    uint16_t v;

    if ((sh->hist_buf == NULL) || (sh->hist_count == 0u)) {
        return;                         /* 空历史: 忽略 */
    }
    if (up) {
        if (sh->hist_view < 0) {
            v = 0;                      /* 实时行 → 最新一条 */
        } else if ((uint16_t)sh->hist_view < (uint16_t)(sh->hist_count - 1u)) {
            v = (uint16_t)sh->hist_view + 1u;
        } else {
            return;                     /* 已到最旧 */
        }
    } else {
        if (sh->hist_view <= 0) {
            if (sh->hist_view < 0) {
                return;                 /* 实时行再下: 无事 */
            }
            shell_hist_replace(sh, "");
            sh->hist_view = -1;         /* 回到实时行 */
            return;
        }
        v = (uint16_t)sh->hist_view - 1u;
    }
    sh->hist_view = (int16_t)v;
    entry = sh->hist_buf + (uint32_t)shell_hist_index(sh, v) * sh->hist_cap;
    shell_hist_replace(sh, entry);
}

static void shell_hist_push(et_shell_t *sh)
{
    char    *slot = sh->hist_buf + (uint32_t)sh->hist_head * sh->hist_cap;
    uint16_t n = sh->at->pos;

    /* atcmd 行缓冲输入中不补 NUL: 按 pos 精确拷贝, 不依赖字符串终止 */
    if (n > (uint16_t)(sh->hist_cap - 1u)) {
        n = (uint16_t)(sh->hist_cap - 1u);
    }
    memcpy(slot, sh->at->linebuf, (size_t)n);
    slot[n] = '\0';
    sh->hist_head = (uint16_t)((sh->hist_head + 1u) % sh->hist_n);
    if (sh->hist_count < sh->hist_n) {
        sh->hist_count++;
    }
    sh->hist_view = -1;
}

/* 消费 ESC [ A/B 转义序列; 其他序列整体吞掉 */
static bool shell_hist_escape(et_shell_t *sh, char ch)
{
    if (sh->esc_state == 1u) {
        sh->esc_state = (ch == '[') ? 2u : 0u;
        return false;
    }
    sh->esc_state = 0u;                     /* esc_state == 2 */
    if (ch == 'A') {
        shell_hist_browse(sh, true);
    } else if (ch == 'B') {
        shell_hist_browse(sh, false);
    }
    return false;                           /* 转义字节不进 atcmd */
}

#endif /* ET_SHELL_HISTORY_N > 0 */

void et_shell_help_cmd(char *args, void *user)
{
    (void)args;
    et_shell_print_help((et_shell_t *)user);
}

#endif /* ET_MODULE_SHELL */

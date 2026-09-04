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

void et_shell_help_cmd(char *args, void *user)
{
    (void)args;
    et_shell_print_help((et_shell_t *)user);
}

#endif /* ET_MODULE_SHELL */

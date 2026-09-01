/**
 * @file    et_atcmd.c
 * @brief   行式 AT 命令解析器实现
 */
#include "et_atcmd.h"

#if ET_MODULE_ATCMD

#include <stddef.h>
#include <string.h>

#define AT_PREFIX     "AT+"
#define AT_PREFIX_LEN 3u

bool et_atcmd_init(et_atcmd_proc_t *p,
                   const et_atcmd_entry_t *cmds, uint16_t cmd_count,
                   char *linebuf, uint16_t line_cap,
                   void *user)
{
    ET_ASSERT(p != NULL);
    ET_ASSERT(cmds != NULL);
    ET_ASSERT(linebuf != NULL);
    if ((p == NULL) || (cmds == NULL) || (linebuf == NULL)) {
        return false;
    }
    if (line_cap < 8u) {
        return false;                       /* 至少容得下 "AT+X\r\n" */
    }
    p->cmds       = cmds;
    p->cmd_count  = cmd_count;
    p->linebuf    = linebuf;
    p->line_cap   = line_cap;
    p->user       = user;
    p->pos        = 0u;
    p->discarding = false;
    return true;
}

void et_atcmd_reset(et_atcmd_proc_t *p)
{
    p->pos        = 0u;
    p->discarding = false;
}

static bool at_process_line(et_atcmd_proc_t *p)
{
    const char *line = p->linebuf;
    const char *rest;
    const char *args;
    const char *scan;
    uint16_t    name_len;
    uint16_t    i;

    /* 跳过前导空格后要求 AT+ 前缀 */
    i = 0u;
    while (line[i] == ' ') {
        i++;
    }
    if (strncmp(&line[i], AT_PREFIX, AT_PREFIX_LEN) != 0) {
        return false;                       /* 非 AT+ 行 */
    }

    rest  = &line[i + AT_PREFIX_LEN];
    scan  = rest;
    while ((*scan != '\0') && (*scan != ' ')) {
        scan++;                             /* 名称到空格或行尾为止 */
    }
    name_len = (uint16_t)(scan - rest);
    while (*scan == ' ') {
        scan++;                             /* 跳过参数前导空格 */
    }
    args = scan;

    {
        uint16_t k;

        for (k = 0u; k < p->cmd_count; k++) {
            if ((strlen(p->cmds[k].name) == name_len) &&
                (strncmp(p->cmds[k].name, rest, name_len) == 0)) {
                if (p->cmds[k].fn != NULL) {
                    p->cmds[k].fn((char *)args, p->user);
                }
                return true;
            }
        }
    }

    if (p->on_unknown != NULL) {
        p->on_unknown(rest, name_len, args, p->user);
    }
    return true;
}

bool et_atcmd_feed(et_atcmd_proc_t *p, char ch)
{
    if ((ch == '\r') || (ch == '\n')) {     /* CR/LF 均视为行结束 */
        if (!p->discarding && (p->pos != 0u)) {
            p->linebuf[p->pos] = '\0';
            bool ok = at_process_line(p);

            p->pos        = 0u;
            p->discarding = false;
            return ok;                      /* 仅在本字节完成命令时为真 */
        }
        p->pos        = 0u;
        p->discarding = false;
        return false;
    }

    if (p->discarding) {
        return false;                       /* 超长行: 静默丢弃至行尾 */
    }

    if ((ch == 0x08u) || (ch == 0x7Fu)) {   /* 退格 */
        if (p->pos != 0u) {
            p->pos--;
        }
        return false;
    }

    if (p->pos >= (uint16_t)(p->line_cap - 1u)) {
        p->discarding = true;               /* 将溢出: 丢弃整行 */
        p->pos        = 0u;
        return false;
    }

    p->linebuf[p->pos++] = ch;
    return false;
}

char *et_atcmd_next_arg(char **cursor)
{
    char *s;
    char *start;

    if ((cursor == NULL) || (*cursor == NULL)) {
        return NULL;
    }
    s = *cursor;
    while (*s == ' ') {
        s++;
    }
    if (*s == '\0') {
        *cursor = s;
        return NULL;
    }
    start = s;
    while ((*s != '\0') && (*s != ' ')) {
        s++;
    }
    if (*s == ' ') {
        *s = '\0';
        s++;
    }
    *cursor = s;
    return start;
}

#endif /* ET_MODULE_ATCMD */

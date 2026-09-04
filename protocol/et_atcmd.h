/**
 * @file    et_atcmd.h
 * @brief   行式 AT 命令解析器 (面向串口控制台/模组交互)
 *
 * 语法:
 *   AT+<名称>[空格<参数...>]\r\n
 *   - 名称大小写敏感; 参数为行内剩余原文(去除前导空格),
 *     可用 et_atcmd_next_arg() 按空白切分;
 *   - 空行忽略; 支持 0x08/0x7F 退格(控制台友好);
 *   - 行超长时丢弃整行并返回错误。
 */
#ifndef ET_ATCMD_H
#define ET_ATCMD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_ATCMD

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*et_atcmd_fn)(char *args, void *user);

typedef struct {
    const char *name;                   /* 不含 "AT+" 前缀 */
    et_atcmd_fn fn;
    /* v1.4 追加: 帮助文本(可为 NULL), 供 et_shell 的 help 自动生成。
     * 字段只增不改: 既有 {name, fn} 位置初始化器仍兼容(尾部补零)。 */
    const char *help;
} et_atcmd_entry_t;

typedef void (*et_atcmd_unknown_fn)(const char *name, uint16_t name_len,
                                    const char *args, void *user);

typedef struct {
    const et_atcmd_entry_t *cmds;       /* 命令表(调用方持有)      */
    uint16_t                cmd_count;

    char                   *linebuf;     /* 行缓冲(调用方提供)      */
    uint16_t                line_cap;

    et_atcmd_unknown_fn     on_unknown;  /* 未知命令回调, 可为 NULL */
    void                   *user;

    /* 内部状态 */
    uint16_t                pos;
    bool                    discarding; /* 行超长丢弃中            */
} et_atcmd_proc_t;

/* 初始化: cmds/linebuf 均为调用方持有, line_cap>=8 */
bool et_atcmd_init(et_atcmd_proc_t *p,
                   const et_atcmd_entry_t *cmds, uint16_t cmd_count,
                   char *linebuf, uint16_t line_cap,
                   void *user);

/* 复位行状态(丢弃半行输入) */
void et_atcmd_reset(et_atcmd_proc_t *p);

/*
 * 喂入 1 字节(UART RX 中断安全: 单写者场景)。
 * 返回: 本字节是否完成了一条有效命令行(命令处理已同步执行)。
 */
bool et_atcmd_feed(et_atcmd_proc_t *p, char ch);

/*
 * 参数切分辅助: 从 *cursor 按单个空格切出下一个参数(原地截断),
 * 无更多参数时返回 NULL。典型用法:
 *   char *cursor = args; const char *a;
 *   while ((a = et_atcmd_next_arg(&cursor)) != NULL) { ... }
 */
char *et_atcmd_next_arg(char **cursor);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_ATCMD */
#endif /* ET_ATCMD_H */

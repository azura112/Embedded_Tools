/**
 * @file    et_shell.h
 * @brief   行式交互壳 (et_atcmd 之上的可选增强层)
 *
 * 定位:
 *  - "调试够用"的行式壳: 输入回显 + 退格擦写 + 提示符 + help 自动生成;
 *  - et_atcmd 仍可独立使用 —— shell 是可选增强, 不做解析重复, 行编辑
 *    语义(退格删字/超长丢弃)全部由底层 atcmd 决定, shell 只负责"回显
 *    什么";
 *  - v1.5: 可选命令历史 (ET_SHELL_HISTORY_N + 静态环形缓冲, 上/下键回放);
 *  - 不做: Tab 补全 / 多行编辑 / 模糊历史匹配 (Non-goals 维持)。
 *
 * 回显规则:
 *  - 可见字符(0x20~0x7E)原样回显;
 *  - '\r' 回显 "\r\n"; 紧随的 '\n' 不再回显(CRLF 对只出一行);
 *  - 退格(0x08/0x7F): 仅当行内已有字符(atcmd pos>0)时回显擦写序列
 *    "\b \x1b[K"(ANSI 清行尾, 可关), 行首边界静默;
 *  - 其余控制字节不回显(照常透传给 atcmd)。
 *
 * help 自动生成:
 *  - 扫描 atcmd 命令表逐行输出 "NAME - 帮助文本"; help 字段为 NULL 的
 *    命令只输出名称;
 *  - 应用把 {"HELP", et_shell_help_cmd} 加进命令表并把 atcmd 的 user
 *    指到 et_shell_t 即获得 help 命令(见 et_shell_help_cmd)。
 *
 * 并发约定: 单上下文模块(🏠MAIN); 输出回调在 feed 内同步执行。
 */
#ifndef ET_SHELL_H
#define ET_SHELL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"
#include "et_atcmd.h"

#if ET_MODULE_SHELL

/* 历史条目编译期容量: 默认 0 = 关闭(ESC[A/B 字节直透, 行为零变化);
 * 置 N>0 后可用 et_shell_set_history 挂接静态环形历史缓冲 */
#ifndef ET_SHELL_HISTORY_N
#define ET_SHELL_HISTORY_N      0
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct et_shell {
    et_atcmd_proc_t *at;            /* 底层解析器(调用方先 et_atcmd_init), 勿动 */
    void           (*putc)(void *user, char ch);    /* 输出函数, 勿动 */
    void           *user;                           /* putc 透传, 勿动 */
    const char     *prompt;         /* 提示符(NULL=不打印), 勿动 */
    bool            echo;           /* 回显开关, 勿动 */
    bool            erase_seq;      /* 退格 ANSI 擦写序列开关, 勿动 */
    bool            cr_pending;     /* 内部: 上一字节是回车 */

    /* v1.5 追加: 可选历史 (只增不改; set_history 后生效), 勿动 */
    char    *hist_buf;              /* 环形条目存储(调用方提供) */
    uint16_t hist_n;                /* 条目数 */
    uint16_t hist_cap;              /* 单条容量(含 NUL) */
    uint16_t hist_count;            /* 有效条数 */
    uint16_t hist_head;             /* 下一个写入槽 */
    int16_t  hist_view;             /* 浏览游标: -1=实时行, 0=最新, ... */
    uint8_t  esc_state;             /* 转义序列状态: 0/见ESC/见'[' */
} et_shell_t;

typedef void (*et_shell_putc_fn)(void *user, char ch);

/* 初始化: at 须已完成 et_atcmd_init; putc 为阻塞式单字符输出。🏠MAIN */
bool et_shell_init(et_shell_t *sh, et_atcmd_proc_t *at,
                   et_shell_putc_fn putc, void *user);

/* 回显开关(默认开) */
void et_shell_set_echo(et_shell_t *sh, bool on);

/* 退格擦写序列 "\b \x1b[K" 开关(默认开; 关闭后退格无回显) */
void et_shell_set_erase(et_shell_t *sh, bool on);

/* 提示符: 传 NULL 隐藏; 立即在新行打印一次 */
void et_shell_set_prompt(et_shell_t *sh, const char *prompt);

/* 手动打印提示符(开机/换行后由应用调用; 命令执行后 shell 自动补打) */
void et_shell_prompt(et_shell_t *sh);

/* 喂入 1 字节: 处理回显/擦写后透传 atcmd。
 * 返回 = 是否完成了一条有效命令(即 et_atcmd_feed 的返回)。🏠MAIN */
bool et_shell_feed(et_shell_t *sh, char ch);

/* help 输出: 扫描命令表, 逐行 "NAME - help" (help 为 NULL 只输出 NAME) */
void et_shell_print_help(et_shell_t *sh);

/* 便捷多字符输出(不经回显逻辑, 直接写线路) */
void et_shell_puts(et_shell_t *sh, const char *s);

/* 预置 help 命令处理函数: 用法 = 命令表加 {"HELP", et_shell_help_cmd},
 * 且 et_atcmd_init 的 user 传 et_shell_t* */
void et_shell_help_cmd(char *args, void *user);

/* 可选历史 (v1.5, 仅 ET_SHELL_HISTORY_N>0 时可用):
 * storage 为 entries × entry_cap 字节静态环形缓冲(调用方持有);
 * 未挂接或编译期关闭时, ESC[A/B 字节直透 atcmd (v1.4 行为)。🏠MAIN */
bool et_shell_set_history(et_shell_t *sh, char *storage,
                          uint16_t entries, uint16_t entry_cap);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_SHELL */
#endif /* ET_SHELL_H */

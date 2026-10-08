/**
 * @file    et_metrics.h
 * @brief   命名运行指标注册表 (定容 / 零动态内存 / 一条命令 dump)
 *
 * 定位 (v2.27, REQ-6):
 *  - "现场排障只要一条命令"的读数面: 各模块的 stats/计数(et_sched 耗时与
 *    miss、et_modbus crc_err/discarded、et_kv erase_cnt、et_selftest 报告…)
 *    由**应用侧**选择性登记到本注册表, 板上敲 AT+METRICS 即可一次读出;
 *  - 库不强制改任何既有模块, 也不内部取时基/不自建采样 —— 只做"命名 u32
 *    指标"的存放、聚合与渲染;
 *  - 两类指标: 计数器(只增, c)与仪表(可覆写当前值, g); 仪表可选挂一个
 *    et_hist 实例, observe 时同时入桶(分布 = 值 + 桶计数两看)。
 *
 * 边界 (与 docs/API_GUIDE.md §8.5 一致):
 *  - 不做掉电保持(交 et_kv)、不做浮点/均值聚合(交 et_stats)、
 *    不做动态订阅或广播(无回调注册面);
 *  - 键 = 短字符串(≤ ET_METRICS_KEY_MAX, 大小写敏感), 超长/空键**拒绝**，
 *    不静默截断(沿 core/et_smap.h 口径);
 *  - 容量 = init 时调用方提供的槽数组长度, 满表登记被拒并计入 dropped,
 *    既有指标零改动(拒绝不覆盖)。
 *
 * 存储形态: 自建定容线性表(键内嵌于槽), 非 core/et_smap 复用 —— 指标量级
 *  为个位数到数十, 线性查找足够, 且 dump 顺序 = 登记顺序(渲染可预期)。
 *  槽数组与句柄全部由调用方持有(形态同 et_smap_init), 库内不取动态内存。
 *
 * 并发约定: 逐 API 显式标注(不变式见 docs/architecture.md)。
 *  - 🔒ISR-safe: inc / add_n / set / get —— 单字(32 位)读写, 前提
 *    **同一指标由单一上下文写**(与 core/et_ringbuf.h、sys/et_wdt.h 同族声明);
 *    两上下文写同一键属数据竞争, 本模块不代加锁;
 *  - 🏠MAIN: init / register_* / link_hist / observe / reset / count /
 *    iter / format / dump_cmd —— observe 可能同时改值与入桶(多字更新),
 *    注册/渲染面为配置期与主循环期操作。
 */
#ifndef ET_METRICS_H
#define ET_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_METRICS

#ifdef __cplusplus
extern "C" {
#endif

/* 指标类别(渲染为行尾 'c'/'g' 单字符) */
#define ET_METRIC_COUNTER   0u      /* 计数器: 只增, inc/add_n 写 */
#define ET_METRIC_GAUGE     1u      /* 仪表  : 可覆写, set/observe 写 */

typedef struct {
    char     key[ET_METRICS_KEY_MAX + 1];   /* 登记时拷入, NUL 结尾, 勿动 */
    uint32_t val;                           /* 当前值, 勿动               */
    void    *hist;                          /* 挂接 et_hist_t*, NULL=无   */
    uint8_t  kind;                          /* ET_METRIC_COUNTER/GAUGE    */
    uint8_t  used;                          /* 槽占用标志, 勿动           */
} et_metrics_slot_t;

typedef struct et_metrics {
    et_metrics_slot_t *slots;   /* 调用方槽数组(init 整表清零), 勿动 */
    uint32_t           cap;     /* 槽总数, 勿动                      */
    uint32_t           count;   /* 有效指标数, 勿动                  */
    uint32_t           dropped; /* 被拒登记数(满表/空键/超长/重名), 勿动 */
} et_metrics_t;

/* 初始化: 句柄与槽数组由调用方提供(零分配), 整表清零后 usable。
 * 校验 m/slots 非空、cap ≥ 1、键长上限 ≥ 1。🏠MAIN */
bool et_metrics_init(et_metrics_t *m, et_metrics_slot_t *slots, uint32_t cap);

/* 登记计数器/仪表: 值起算 0, 键拷入表内(调用方可释放原串)。
 * 空键/超长(> ET_METRICS_KEY_MAX)/重名/满表 → 返回 false 并计 dropped,
 * 不做任何写入。🏠MAIN */
bool et_metrics_register_counter(et_metrics_t *m, const char *key);
bool et_metrics_register_gauge(et_metrics_t *m, const char *key);

/* 给已登记的**仪表**挂接 et_hist 实例(hist 生存期由调用方负责, 传 NULL 解绑)。
 * hist 形参取 void* 是刻意的: 本头不依赖 ET_MODULE_HIST, 关掉直方图模块时
 * 调用方传 NULL 即可(入桶路径不编译)。键不存在 → false。🏠MAIN */
bool et_metrics_link_hist(et_metrics_t *m, const char *key, void *hist);

/* 自增 1 / 加 n: 键不存在静默返回(读侧计数不受影响)。🔒ISR-safe */
void et_metrics_inc(et_metrics_t *m, const char *key);
void et_metrics_add_n(et_metrics_t *m, const char *key, uint32_t n);

/* 置为 v(仪表语义, 计数器亦可写但属误用): 键不存在静默返回。🔒ISR-safe */
void et_metrics_set(et_metrics_t *m, const char *key, uint32_t v);

/* 观测一个值: 仪表置为 v, 且若挂接了 et_hist 则同时 push 入桶
 * (ET_MODULE_HIST=0 时只置值, hist 指针永不使用)。键不存在返回 false。🏠MAIN */
bool et_metrics_observe(et_metrics_t *m, const char *key, uint32_t v);

/* 只读查询: 命中返回 true 并写 *out; **未登记如实返回 false 且写 0**
 * (out 可为 NULL, 仅测存在性)。🔒ISR-safe */
bool et_metrics_get(const et_metrics_t *m, const char *key, uint32_t *out);

uint32_t et_metrics_count(const et_metrics_t *m);

/* 按登记顺序迭代: idx < count 时取回第 idx 项(键指针指向表内, 回调后不得
 * 长期保留), 任一输出指针可为 NULL。idx 越界 → false 且输出不写。🏠MAIN */
bool et_metrics_iter(const et_metrics_t *m, uint32_t idx,
                     const char **key, uint32_t *val, uint8_t *kind);

/* 清零全部指标值(登记与挂接保留; dropped 一并清零)。🏠MAIN */
void et_metrics_reset(et_metrics_t *m);

/* ---- 渲染: dump 的唯一渲染源(板上与 host 单测共用同一函数) ----
 * 输出形态(逐行, "\n" 结尾, 无空行):
 *   METRICS <count>/<cap> drop=<dropped>\n
 *     <key>=<val> <c|g>\n          ← 按登记顺序, 两空格缩进
 * 返回实际写入 buf 的字节数(不含终止 NUL)。容量不足 → **截断**到 cap-1
 * 字节并补 NUL(返回值 = 实际写入数, 等于 buf_cap-1 即提示被截断)。
 * buf_cap 预算: 32 + 每槽 32 字节(键上限 16 + 值 10 + 类别与缩进 6)。🏠MAIN */
uint32_t et_metrics_format(const et_metrics_t *m, char *buf, uint32_t buf_cap);

/* 预置命令处理函数(签名 = protocol/et_atcmd.h 的 et_atcmd_fn):
 * 用法 = 命令表加 {"METRICS", et_metrics_dump_cmd, "..."}, 且 et_atcmd_init
 * 的 user 传 et_shell_t*(与 {"HELP", et_shell_help_cmd} 同款形态);
 * 无 shell 时 user 传 NULL, 改经日志面(et_log_raw)输出; 两者皆无则静默。
 * args 忽略(AT+METRICS 无参)。库不持有命令表; 渲染仍走 et_metrics_format
 * 这一唯一渲染源。🏠MAIN */
void et_metrics_dump_cmd(char *args, void *user);

/* 命令面绑定: 告知 dump 命令要读哪一份注册表(🏠MAIN; 传 NULL 解绑)。
 * 库内只存一份**只读句柄**, 存储仍归调用方 —— 多实例并行时其余实例经 API
 * 直读或自备包装函数(形态见 docs/API_GUIDE.md §11.15); 单静态绑定面的先例
 * 同 sys/et_sched.c 的文件静态任务表。 */
void et_metrics_bind(et_metrics_t *m);

/* 预置命令处理函数: 编译期裁剪自描述(AT+FEATURES, REQ-8)。
 * 输出 = et_features_format 的渲染结果, user/输出通道同上。🏠MAIN */
void et_features_cmd(char *args, void *user);

/* 裁剪/特性自描述渲染(与 METRICS 同模块承载, 共用 dump 命令面):
 *   FEATURES v<MAJOR>.<MINOR>.<PATCH> (maj=<M> min=<N> pat=<P>) sw=<开关数>\n
 *     <MODULE>=<0|1>\n             ← 按 et_config.h 声明顺序, 两空格缩进
 * 表为**编译期**构建: 值取各 ET_MODULE_* 宏, 未启用模块如实报 0(不是省略行)。
 * 返回值与截断口径同 et_metrics_format。🏠MAIN */
uint32_t et_features_format(char *buf, uint32_t buf_cap);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_METRICS */
#endif /* ET_METRICS_H */

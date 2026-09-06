/**
 * @file    et_selftest.h
 * @brief   板上自测组件 (v1.7, 验证金字塔封顶: PC 单测 → CI 仿真 → 板上自测)
 *
 * 定位:
 *  - 任何 port 接入后, 一条命令获得全模块冒烟能力 (源自 G474 工程 AT+SELFTEST
 *    实践的库化, 见 移植stm32实机记录.md);
 *  - 与 host 侧 test/et_test.c 的关系: 两套独立实现 —— host 侧追求用例数与
 *    注入能力 (掉电矩阵/时间注入), 本组件追求"零依赖可上板"; 覆盖边界:
 *    冒烟非对等 279+ host 用例, 掉电注入类 host-only 用例不移植;
 *  - 纯增量模块: ET_MODULE_SELFTEST 默认 0, 发布可整体裁剪;
 *  - 零动态内存: 套件表为编译期常量 + ET_SELFTEST_MAX_EXTRA 个动态注册槽。
 *
 * 存储门控 (破坏性警告):
 *  - kv/bootctl 套件会【擦写】所配置扇区 —— 默认未配置即跳过并上报 SKIP;
 *    上板验证存储链路时由应用显式 set(通常传应用自己的 kv 布局, 接受自测
 *    数据覆盖)。
 *
 * 并发策略: 全部 API 仅限 🏠MAIN (自测本身不是并发对象)。
 */
#ifndef ET_SELFTEST_H
#define ET_SELFTEST_H

#include <stdint.h>
#include <stdbool.h>
#include "et_config.h"

#if ET_MODULE_SELFTEST

#ifdef __cplusplus
extern "C" {
#endif

#if ET_MODULE_KV
#include "et_kv.h"
#endif
#if ET_MODULE_BOOTCTL
#include "et_bootctl.h"
#endif

/* ---- 报告事件 (结构化, 不在组件内做格式化; 由 report 侧接 et_log/shell) ---- */
typedef enum {
    ET_SELFTEST_BEGIN = 0,      /* num = 套件总数 (将执行的)              */
    ET_SELFTEST_SUITE_PASS,     /* suite = 套件名                          */
    ET_SELFTEST_SUITE_FAIL,     /* suite, num = 失败断言数                 */
    ET_SELFTEST_SUITE_SKIP,     /* suite = 套件名 (存储门控未配置)         */
    ET_SELFTEST_CHECK_FAIL,     /* suite, num = 失败断言行号               */
    ET_SELFTEST_DONE            /* num = 通过的套件数                      */
} et_selftest_evt_t;

typedef void (*et_selftest_report_fn)(void *user, et_selftest_evt_t evt,
                                      const char *suite, uint32_t num);

/* ---- 套件 ---- */
typedef bool (*et_selftest_suite_fn)(et_selftest_report_fn report, void *user);

/* 动态注册额外套件 (应用自有套件/测试注入用); 容量
 * ET_SELFTEST_MAX_EXTRA(默认 4), 满或重名返回 false。🏠MAIN */
bool et_selftest_register(const char *name, et_selftest_suite_fn fn);

/* 动态套件内部上报断言失败详情 (可选; name 传注册名) */
void et_selftest_note_fail(et_selftest_report_fn report, void *user,
                           const char *suite, uint32_t line);

/* ---- 存储门控 (v1.7): 不配置则 kv/bootctl 套件跳过 ---- */
#if ET_MODULE_KV
/* 传应用的 kv 双扇区布局; 自测会 format 并写入这两扇区 (破坏性) */
void et_selftest_set_kv_layout(const et_kv_layout_t *lay);
#endif
#if ET_MODULE_BOOTCTL
/* 传应用的 bootctl 配置 (state/slot 扇区); 自测会擦写这些扇区 (破坏性) */
void et_selftest_set_bootctl_cfg(const et_bootctl_cfg_t *cfg);
#endif

/* 跑全部套件 (存储套件按门控执行/跳过); 返回 false = 存在失败套件。
 * report 可为 NULL (静默跑, 仅取返回值)。🏠MAIN */
bool et_selftest_run_all(et_selftest_report_fn report, void *user);

/* 单套件: 按名查找 (含动态注册); 未找到返回 false。🏠MAIN */
bool et_selftest_run_suite(const char *name, et_selftest_report_fn report,
                           void *user);

/* 套件总数 (内建 + 动态注册, 含被门控跳过的) */
uint16_t et_selftest_suite_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_SELFTEST */
#endif /* ET_SELFTEST_H */

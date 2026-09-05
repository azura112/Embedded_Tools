/**
 * @file    et_wdt.h
 * @brief   看门狗封装 + 阻塞段保护 helper (port_wdt 三件套之上)
 *
 * 定位:
 *  - port_wdt_enable/feed/disable 的库层封装: 统一契约下限校验
 *    (timeout ≥ PORT_FLASH_ERASE_MS_MAX×2, 与 flash 擦除喂狗指引闭环),
 *    应用不再直接触碰 port 看门狗符号;
 *  - et_wdt_guard(): 长阻塞操作 (flash 擦除批量写/外部器件应答) 前后
 *    自动喂狗的保护 helper;
 *  - feed 🔒ISR-safe; enable/disable/guard 仅 🏠MAIN。
 *
 * 契约要点 (v1.5 决议, 详见 API_GUIDE §9):
 *  - F103 IWDG 启动后不可停: et_wdt_disable 返回 false 即"仍在运行",
 *    应用不得以返回值判断"狗已停"来执行危险动作;
 *  - 重复 enable = 重新配置并重置喂狗窗口。
 *
 * 与 flash 擦除组合配方 (见 API_GUIDE §11.7):
 *   et_wdt_guard(flash_bulk_job, ctx)  —— 看门狗超时 ≥ 2×单扇区擦除上限,
 *   guard 前后各喂一次, 批量任务内部每扇区再自行喂狗。
 */
#ifndef ET_WDT_H
#define ET_WDT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_WDT

#ifdef __cplusplus
extern "C" {
#endif

/* 启动看门狗: timeout < PORT_FLASH_ERASE_MS_MAX*2 拒绝 (返回 false 且
 * 不改变 port 状态)。重复调用 = 重新配置。🏠MAIN */
bool et_wdt_enable(uint32_t timeout_ms);

/* 喂狗。🔒ISR-safe */
void et_wdt_feed(void);

/* 尽力而为停狗: IWDG 类返回 false (仍在运行)。🏠MAIN */
bool et_wdt_disable(void);

/* 阻塞段保护任务: 执行前喂狗 → fn(user) → 执行后喂狗。
 * 返回 fn 的返回值; fn 为 NULL 返回 false 且不喂狗。🏠MAIN */
typedef bool (*et_wdt_job_fn)(void *user);
bool et_wdt_guard(et_wdt_job_fn fn, void *user);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_WDT */
#endif /* ET_WDT_H */

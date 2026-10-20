/**
 * @file    port_host.h
 * @brief   宿主机(PC)平台适配 —— 仅测试用, 不随库发布到 MCU
 *
 * 核心能力: 虚拟时间注入。sys 层单测通过手动推进时基获得完全确定的行为,
 * 无需真实等待, 也天然覆盖 tick 回绕等边界场景。
 */
#ifndef PORT_HOST_H
#define PORT_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 直接设置虚拟时基当前值(毫秒), 可用于构造回绕前的临界位置 */
void port_host_tick_set(uint32_t ms);

/* 将虚拟时基前进 delta 毫秒 */
void port_host_tick_advance(uint32_t delta);

/* 读取虚拟时基当前值 */
uint32_t port_host_tick_now(void);

/* ---- 输出捕获(测试用): 开启后 port_putc 写入缓冲而非 stdout ---- */
void     port_host_capture_start(char *buf, uint32_t cap);

/* 停止捕获并返回写入长度(缓冲内自动补 NUL) */
uint32_t port_host_capture_stop(void);

/* ===================== flash 参数区模拟 (et_kv 测试用) ===================== */

#ifndef PORT_FLASH_SECTOR_SIZE
#define PORT_FLASH_SECTOR_SIZE      1024u    /* host 模拟: 1KB x 16 = 16KB 静态 RAM */
#endif
#ifndef PORT_FLASH_SECTOR_COUNT
#define PORT_FLASH_SECTOR_COUNT     16u
#endif
#ifndef PORT_FLASH_ERASE_MS_MAX
#define PORT_FLASH_ERASE_MS_MAX     1u       /* host 模拟擦除瞬时完成 */
#endif

/* ===================== flash 区域(分区表)模拟 (v2.29 P2-1/P2-2, REQ-7) =====================
 * host 的"绝对地址"= 同一块 g_flash 镜像内的字节偏移(参数区基址取 0), 于是掉电注入、
 * 擦除半途失败注入这些既有门禁机制在**区域位形下同样可复现**(计划 P2-2 要求沿用既有
 * host 机制, 不另起一套)。分区表 = 参数区 + A/B 双槽 + 状态页, 顺序即 index。
 */
#ifndef PORT_HOST_SLOT_BYTES
#define PORT_HOST_SLOT_BYTES  8192u          /* 单槽模拟容量(须为页大小整数倍) */
#endif
#define PORT_HOST_PARAM_BYTES (PORT_FLASH_SECTOR_SIZE * PORT_FLASH_SECTOR_COUNT)
#define PORT_HOST_FLASH_BYTES (PORT_HOST_PARAM_BYTES + 2u * PORT_HOST_SLOT_BYTES + \
                               PORT_FLASH_SECTOR_SIZE)

#ifdef PORT_FLASH_REGIONS
#undef PORT_FLASH_REGIONS                     /* port.h 的默认 0 只服务无分区表的平台 */
#endif
#define PORT_FLASH_REGIONS    4u              /* param / slot_a / slot_b / state */

#define PORT_HOST_REGION_PARAM   0u           /* 区域序号, 用例可读 */
#define PORT_HOST_REGION_SLOT_A  1u
#define PORT_HOST_REGION_SLOT_B  2u
#define PORT_HOST_REGION_STATE   3u

/* 模拟区整体置 0xFF(等效新芯片), 写入计数/故障注入清零 */
void     port_host_flash_reset(void);

/* 白盒访问模拟区(测试直接构造坏页/乱序 seq 等), 返回区内偏移处的字节指针 */
uint8_t *port_host_flash_mem(uint32_t offset);

/* 掉电注入: 自调用起累计写入 n 字节后模拟掉电, 后续 port_flash_write 不再写
 * (部分写入截断); 传 0xFFFFFFFF 关闭注入。精确断点可先查 port_host_flash_written() */
void     port_host_flash_fail_after(uint32_t n);

/* 擦除故障注入: 下一次 port_flash_erase_sector 只擦除扇区前半(模拟擦除中断) */
void     port_host_flash_erase_fail_once(void);

/* 累计写入字节数(验证"del 不存在 key 零写入"等) */
uint32_t port_host_flash_written(void);

/* ===================== 看门狗软件模拟 (et_wdt 测试用) ===================== */

typedef void (*port_host_wdt_cb_t)(void *user);

/* 安装超时回调(触发一次后不再重复, 重新 enable 后复位); 传 NULL 卸载 */
void     port_host_wdt_install(port_host_wdt_cb_t cb, void *user);

/* 测试驱动: 按虚拟时基检查超时窗口 (真实硬件由 IWDG 硬件自主计时) */
void     port_host_wdt_poll(void);

/* 累计喂狗次数 (guard 前后喂狗断言用) */
uint32_t port_host_wdt_feeds(void);

/* 清零模拟状态 (enable/feed 计数/触发标记) */
void     port_host_wdt_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HOST_H */

/**
 * @file    port.h
 * @brief   平台适配层接口契约 (唯一允许接触硬件的层)
 *
 * 分层契约:
 *  - core/ algorithm/ 层【禁止】包含本文件, 保证纯 C 零硬件依赖;
 *  - sys/ drivers/ debug/ storage/ 层仅通过本头文件访问硬件能力;
 *  - 各平台在 port/<platform>/ 下提供实现(如 port/stm32f103/, port/host/)。
 *
 * 实现清单:
 *  - PORT_CRITICAL_ENTER / PORT_CRITICAL_EXIT : 主循环与 ISR 共享数据临界区, 必须可嵌套;
 *  - port_tick_get_ms()  : 毫秒级单调递增时基(SysTick 等), 由平台中断维护;
 *  - port_putc()         : 阻塞式单字符输出(debug 日志使用), 平台自行重定向到串口等;
 *  - port_flash_read/write/erase_sector : flash 参数区访问, 仅在 ET_MODULE_KV=1
 *    时为必选契约(仅 et_kv 使用); 关闭该模块的老平台无需实现。
 */
#ifndef PORT_H
#define PORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t port_tick_ms_t;

/* tickless 语义: "无近期到期者"的哨兵值 (et_sched_next_due / et_stimer_next_due)。
 * 睡眠时长须另受唤醒源约束 —— 串口 RX 中断等必须保持使能, 见 API_GUIDE 配方。 */
#define PORT_TICK_WAIT_FOREVER  0xFFFFFFFFu

/* 进入临界区: 保存并关闭中断(或等效机制), 与 EXIT 必须成对且可嵌套 */
#define PORT_CRITICAL_ENTER()   port_critical_enter()
#define PORT_CRITICAL_EXIT()    port_critical_exit()

void    port_critical_enter(void);
void    port_critical_exit(void);

/* 获取毫秒时基(单调递增, 允许自然回绕, 上层用无符号减法比较) */
port_tick_ms_t port_tick_get_ms(void);

/* 日志底层输出: 输出单个字符, 需阻塞直到发送完成 */
void    port_putc(char c);

/* ===================== flash 参数区 (v1.2 契约演进, 纯增量) =====================
 * 定位: 仅供 et_kv 的"参数掉电保存"场景, 不是通用文件系统接口。
 * 参数区 = 平台划出的连续扇区段, 大小 = PORT_FLASH_SECTOR_SIZE * PORT_FLASH_SECTOR_COUNT;
 * offset 一律相对参数区首字节, sector_index ∈ [0, PORT_FLASH_SECTOR_COUNT)。
 *
 * 平台须定义 (#ifndef 保护, 建议放平台 port 头或编译选项):
 *   PORT_FLASH_SECTOR_SIZE   单扇区字节数(如 F103 中容量 1024)
 *   PORT_FLASH_SECTOR_COUNT  参数区扇区数(建议 4~16, et_kv 用其中两扇区)
 *   PORT_FLASH_ERASE_MS_MAX  单扇区擦除耗时上限(ms, 看门狗超时配置参考)
 *
 * 语义约定 (v1.2 评审决议, 见 docs/proposals/et_kv_flash_contract.md §5):
 *  - 写入粒度 4B 对齐: offset/len 均须 4B 对齐, port 内部负责平台编程粒度补齐;
 *  - 位写约束: 只能把 1 写成 0(擦除后全 0xFF), 调用方保证目标区已擦除;
 *  - erase/write 仅限 🏠MAIN 上下文, port 内部以临界区包住完整擦写序列;
 *  - 擦除为 ms 级阻塞操作, 看门狗喂狗/调度暂停由【调用方】负责;
 *  - write 返回实际写入字节数(掉电/故障可部分写入, 调用方校验)。
 *  - 编程粒度是平台特性: F1 为 16 位半字且允许位 1→0 重复编程; G4/L4 类为
 *    64 位双字且单次编程(目标双字非全 1 → 故障截断)。storage(et_kv/bootctl)
 *    已按 8B 槽适配两种粒度, 见 port/stm32g474/README.md "flash 约束"。
 */
bool     port_flash_read(uint32_t offset, void *buf, uint32_t len);
uint32_t port_flash_write(uint32_t offset, const void *buf, uint32_t len);
bool     port_flash_erase_sector(uint32_t sector_index);

/* ===================== 看门狗 (v1.5 契约演进, 纯增量) =====================
 * 仅 ET_MODULE_WDT=1 时为必选契约 (et_wdt 使用); 关闭该模块的老平台零影响。
 * 语义约定 (v1.5 评审决议, 见 docs/API_GUIDE.md §9):
 *  - port_wdt_enable(timeout_ms): 启动并配置超时; timeout 低于
 *    PORT_FLASH_ERASE_MS_MAX*2 时拒绝(返回 false)——保证 flash 擦除期间
 *    有足够的喂狗窗口; 重复 enable = 重新配置并重置窗口;
 *  - port_wdt_feed(): 喂狗; 🔒ISR-safe (单写寄存器/单调用);
 *  - port_wdt_disable(): 尽力而为——IWDG 类硬件启动后不可停, 返回 false
 *    表示"实际仍在运行"(F103 语义); enable/disable 仅 🏠MAIN。
 */
bool port_wdt_enable(uint32_t timeout_ms);
void port_wdt_feed(void);
bool port_wdt_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_H */

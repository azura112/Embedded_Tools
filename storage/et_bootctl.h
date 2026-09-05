/**
 * @file    et_bootctl.h
 * @brief   安全升级控制 (镜像头校验 + A/B 试运行/确认/回滚状态机)
 *
 * 定位:
 *  - v1.4 通路 (et_xmodem 收镜像 → port_flash_write 写槽位) 之上的"安全"闭环;
 *  - 交付的是【库组件】: 可复用的镜像头校验 + 升级状态记录; 跳转/搬运/
 *    双区链接脚本属 bootloader 工程, 由应用侧组合 (见 API_GUIDE 配方);
 *  - 状态记录独立于 et_kv —— bootloader 场景可不启用 kv;
 *  - 安全边界: 只做 CRC32 完整性, 不做加密/签名/防回滚强制 (Non-goal)。
 *
 * ============================ 镜像头 (32B, 槽位首部) ============================
 *
 *   偏移  大小  字段         说明
 *   0x00   4   magic        'ETBI' (0x49425445)
 *   0x04   2   hdr_ver      头格式版本, 当前 = 1 (版本化: 未来只增不改,
 *                            新字段经 hdr_size 定位, 旧固件拒绝更高 ver)
 *   0x06   2   hdr_size     = 32
 *   0x08   4   img_size     镜像字节数 (不含头)
 *   0x0C   4   img_crc32    镜像区 [32, 32+img_size) 的 et_crc32
 *   0x10   4   img_ver      镜像版本号 (应用语义, 仅记录不强制)
 *   0x14   4   load_addr    加载/入口地址 (应用语义, 仅记录)
 *   0x18   4   reserved     0
 *   0x1C   4   hdr_crc32    头 [0, 28) 的 et_crc32
 *
 * ============================ 状态扇区布局 (1 扇区) ============================
 *
 *   偏移  大小  内容
 *   0x00   8   magic 'ETBS'(0x53425445) | ver u16(=1) | rsvd u16(=0)
 *   0x08   4   crc32 over [0x00, 0x08) —— 头半写断电 → CRC 坏 → init 自愈
 *   0x0C  ...  事件记录区, 每条 8B: { u32 val; u32 inv = ~val; }
 *
 *   事件 val 编码 (高 3 字节为 ASCII 标识, 低字节为槽号 0/1):
 *     0x53544, 0x00|slot   'STG'  试运行标记
 *     0x434E4600|slot      'CNF'  确认标记
 *     0x41545400|slot      'ATT'  启动尝试 (每开机 +1 条)
 *
 *   可靠性模型 (append-only + 0→1 位写):
 *     - 记录只追加, 写入顺序严格单调; 读侧遇到第一条无效记录即停
 *       (断电不可能产生"后面的记录先于前面落盘");
 *     - 任意一条 8B 记录半写 (val/inv 任一半字未落盘) → inv != ~val
 *       → 该条无效 → 读侧丢弃: 断电只丢"最后一步", 前态完好;
 *     - 全部写入满足 1→0 位写约束 (val 非 0xFFFFFFFF), 无需擦除即可追加;
 *     - 记录区容量 = 扇区/8 - 12B 头, 满时写入拒绝 (et_bootctl_abandon
 *       重建, 见下)。
 *
 * ============================ 状态机 ============================
 *
 *      (无记录/初始)
 *          │ stage(B)                     应用侧升级: 收镜像→校验→置试运行
 *          ▼
 *      staged=B ──boot_attempt(B)×n──▶ attempts≥max?
 *          │   (bootloader 开机计数)        │ 是
 *          │ confirm(B)                    ▼ should_rollback=true
 *          ▼                             应用回滚: abandon() → 回初始态
 *      confirmed=B
 *          │ abandon() / 新一轮升级前
 *          ▼
 *      (擦状态扇区重建, 回初始态)
 *
 *   规则:
 *     - 双槽互斥: 已 staged 槽 A 时 stage(B) 返回 false (换槽先 abandon);
 *     - confirm 仅对 staged 槽有效; confirm 后 stage 拒绝 (先 abandon);
 *     - should_rollback 仅在 "staged 且未 confirm 且 attempts≥max" 为真;
 *     - boot_attempt 仅对 staged 槽计数并返回新计数值, 其余返回 0。
 *
 * 并发约定: 全部 API 仅限 🏠MAIN (flash 擦写为 ms 级阻塞, 喂狗见 et_wdt)。
 */
#ifndef ET_BOOTCTL_H
#define ET_BOOTCTL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_BOOTCTL

#ifdef __cplusplus
extern "C" {
#endif

#define ET_BOOT_IMG_MAGIC       0x49425445u     /* 'ETBI' */
#define ET_BOOT_HDR_VER         1u              /* 头格式版本 (版本化) */
#define ET_BOOT_HDR_SIZE        32u
#define ET_BOOT_STATE_MAGIC     0x53425445u     /* 'ETBS' */

/* 镜像头 (32B, 上位机/打包侧写入槽位首部; 本库只读校验) */
typedef struct {
    uint32_t magic;                         /* 'ETBI' */
    uint16_t hdr_ver;                       /* 头格式版本 (=1) */
    uint16_t hdr_size;                      /* = 32 */
    uint32_t img_size;                      /* 镜像字节数 (不含头) */
    uint32_t img_crc32;                     /* 镜像区 CRC32 */
    uint32_t img_ver;                       /* 镜像版本号 (应用语义) */
    uint32_t load_addr;                     /* 加载/入口地址 (应用语义) */
    uint32_t reserved;                      /* 0 */
    uint32_t hdr_crc32;                     /* 头 [0,28) CRC32 */
} et_boot_img_hdr_t;

/* 编译期断言: 镜像头必须 32B (C99 技巧: 负数组尺寸即编译错) */
typedef char et_boot_hdr32_check_[(sizeof(et_boot_img_hdr_t) == ET_BOOT_HDR_SIZE) ? 1 : -1];

/* 升级状态快照 (查询用) */
typedef struct {
    int32_t  staged_slot;                   /* 0/1 = 试运行槽; -1 = 无 */
    int32_t  confirmed_slot;                /* 0/1 = 已确认槽; -1 = 无 */
    uint32_t attempts;                      /* staged 槽启动尝试计数 */
    bool     state_ok;                      /* 状态扇区头 CRC 有效 */
} et_bootctl_state_t;

typedef struct {
    uint32_t state_sector;                  /* 状态扇区号 (< PORT_FLASH_SECTOR_COUNT) */
    uint32_t slot_sector[2];                /* A/B 槽位扇区号, 互异且 != state_sector */
    uint32_t slot_size;                     /* 单槽容量字节 (校验 img_size 上限) */
    uint8_t  max_attempts;                  /* 回滚阈值, ≥1 */
} et_bootctl_cfg_t;

typedef struct et_bootctl {
    et_bootctl_cfg_t cfg;                   /* 初始化副本, 勿动 */
    uint32_t rec_off;                       /* 状态扇区记录区写游标, 勿动 */
    bool     dirty_log;                     /* 记录区含半写记录待修复, 勿动 */
    bool     inited;                        /* 勿动 */
} et_bootctl_t;

/* 初始化: 校验 cfg 合法性 + 状态扇区自愈 (头无效 → 擦除重建)。
 * erase 失败 (host 注入/硬件故障) 返回 false。🏠MAIN */
bool et_bootctl_init(et_bootctl_t *bc, const et_bootctl_cfg_t *cfg);

/* 镜像校验: 头 (magic/ver/size/头CRC) + 全镜像 CRC, 分块流式零大栈。🏠MAIN */
bool et_bootctl_verify_image(et_bootctl_t *bc, uint32_t slot);

/* 置试运行: 写 STG 记录; 同槽重复调用幂等(不再写); 换槽/已确认 → false。🏠MAIN */
bool et_bootctl_stage(et_bootctl_t *bc, uint32_t slot);

/* 确认: 应用自检通过后调用; 仅对 staged 槽有效。🏠MAIN */
bool et_bootctl_confirm(et_bootctl_t *bc, uint32_t slot);

/* bootloader 开机调用: staged 槽尝试计数 +1 并返回新计数; 非 staged 返回 0。🏠MAIN */
uint32_t et_bootctl_boot_attempt(et_bootctl_t *bc, uint32_t slot);

/* 回滚判定: staged 且未 confirm 且 attempts ≥ max_attempts → true */
bool et_bootctl_should_rollback(const et_bootctl_t *bc, uint32_t slot);

/* 查询状态快照 (staged/confirmed/attempts) */
void et_bootctl_state(const et_bootctl_t *bc, et_bootctl_state_t *st);

/* 放弃/重置升级状态: 擦状态扇区并重建头 (确认后开新一轮也走此入口)。
 * 擦除失败返回 false 且状态不变。🏠MAIN */
bool et_bootctl_abandon(et_bootctl_t *bc);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_BOOTCTL */
#endif /* ET_BOOTCTL_H */

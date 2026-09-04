/**
 * @file    et_kv.h
 * @brief   flash 键值掉电存储 (双扇区乒乓 + 追加写 + CRC32 校验)
 *
 * 定位: "参数掉电保存", 不是通用文件系统/数据存储引擎。
 * 可靠性模型 (评审决议, 见 docs/proposals/et_kv_flash_contract.md §5):
 *  - 页头 magic|seq|state|crc32, state 从 MOVING(0xFFFFFFFF) 编程为
 *    COMMITTED(0x00000000) 后页才有效 —— 搬迁中途掉电对页自动弃用, 源页无损;
 *  - 记录追加写, 读侧取 key 最新有效版本; 单记录 CRC 损坏只影响该记录,
 *    get 跳过后可回退旧版本, 不影响其他 key;
 *  - 追加写把页头写坏(脏尾)时, init 自动搬迁修复;
 *  - 任何操作即时持久化, 无"丢数据窗口"; commit() 为手动压实(去重死记录)。
 *
 * 资源与寿命:
 *  - 双扇区固定乒乓, 无磨损均衡: 每次页切换擦对扇区一次。寿命 ≈
 *    扇区擦写上限 × 扇区容量 / 每次净写入量, 高频写入请评估或上层合并;
 *  - et_kv_stats() 暴露 seq 与本次上电擦除计数 (决议 E: 计数不做掉电保持)。
 *
 * 并发约定: 与 et_mempool 同级 —— 全部 API 仅限 🏠MAIN 上下文
 * (flash 擦写期间 port 内部有临界区, ISR 依赖契约禁止取指同区)。
 */
#ifndef ET_KV_H
#define ET_KV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_KV

#ifdef __cplusplus
extern "C" {
#endif

/* 用户 key 上限: [1, ET_KV_KEY_MAX]; 0x7FFF 保留(0xFFFF 为"未写"哨兵, 见决议 C) */
#define ET_KV_KEY_MAX           ((uint16_t)0x7FFEu)

typedef struct {
    uint32_t sector_a;      /* 参数区内扇区序号, < PORT_FLASH_SECTOR_COUNT */
    uint32_t sector_b;      /* 与 sector_a 不同 */
} et_kv_layout_t;

typedef struct {
    uint32_t seq;           /* 活跃页版本号(每次页切换 +1) */
    uint32_t erase_cnt_a;   /* 扇区 a 本次上电擦除次数 */
    uint32_t erase_cnt_b;
    uint32_t used_bytes;    /* 活跃页已写字节(含页头/记录开销) */
    uint32_t free_bytes;    /* 活跃页剩余可写 */
    uint16_t record_count;  /* 记录槽位数(含 tombstone 与 CRC 失效槽位) */
    uint16_t key_count;     /* 有效 key 数(去重, 不含已删) */
} et_kv_stats_t;

typedef struct et_kv {
    uint32_t sec_a;         /* 布局扇区 a 物理序号(擦除计数归属), 勿动 */
    uint32_t sec_b;         /* 布局扇区 b 物理序号, 勿动 */
    uint32_t act_sector;    /* 活跃页扇区号, 勿动 */
    uint32_t alt_sector;    /* 备用页扇区号, 勿动 */
    uint32_t act_seq;       /* 活跃页 seq, 勿动 */
    uint32_t write_off;     /* 活跃页下一写入偏移(页内), 勿动 */
    uint32_t erase_cnt_a;   /* 扇区 a 本次上电擦除计数, 勿动 */
    uint32_t erase_cnt_b;
    uint16_t record_cnt;    /* 槽位计数, 勿动 */
} et_kv_t;

/* 初始化: 双扇区扫描仲裁选活跃页; 活跃页有脏尾时自动搬迁修复。
 * 两页均无效(坏)时返回 false —— 可用 et_kv_format 恢复。🏠MAIN */
bool et_kv_init(et_kv_t *kv, const et_kv_layout_t *layout);

/* 格式化: 擦除两扇区, 写 seq=1 空白活跃页。-existing 数据全部丢弃。🏠MAIN */
bool et_kv_format(et_kv_t *kv, const et_kv_layout_t *layout);

/* 写入(即时持久化): 追加新版本记录; 空间不足自动压实后重试;
 * val 为 NULL 且 len>0 非法; len 超过单记录上限返回 false。🏠MAIN */
bool et_kv_set(et_kv_t *kv, uint16_t key, const void *val, uint16_t len);

/* 读取: 写入 out_len(可 NULL)实际长度; cap < len 时返回 false 且不触碰 buf。
 * 不存在/已删除返回 false (可用 et_kv_size 区分)。🏠MAIN */
bool et_kv_get(et_kv_t *kv, uint16_t key, void *buf, uint16_t cap,
               uint16_t *out_len);

/* 删除: 写入 tombstone 记录; key 本就不存在返回 false 且零写入。🏠MAIN */
bool et_kv_del(et_kv_t *kv, uint16_t key);

/* 查询 key 当前值长度; 不存在/已删/记录损坏返回 0。🏠MAIN */
uint16_t et_kv_size(et_kv_t *kv, uint16_t key);

/* 手动压实: 把活跃页存活记录(去重)搬去对页, 释放死记录空间。🏠MAIN */
bool et_kv_commit(et_kv_t *kv);

void et_kv_stats(et_kv_t *kv, et_kv_stats_t *st);

/* ===================== 枚举迭代 (v1.4) ===================== */
/*
 * 枚举活跃页上的有效 key(诊断导出/上位机批量读取场景), 只读: 不触碰页状态。
 * 快照语义: init 时固定"当时活跃页"扇区; 迭代期间 set() 触发压实换页也不影响
 * 快照页的可读性(其数据在下一次压实前有效), 多版本只出最新, tombstone 跳过。
 * 迭代期间向活跃页追加的新记录会被游标自然覆盖(游标只在已写区内前移)。
 */
typedef struct {
    uint32_t sector;        /* 快照页扇区, 勿动 */
    uint32_t off;           /* 下一记录页内偏移, 勿动 */
} et_kv_iter_t;

/* 初始化迭代器: kv 须已 init/format; 零句柄/空参数返回 false。🏠MAIN */
bool et_kv_iter_init(const et_kv_t *kv, et_kv_iter_t *it);

/* 取下一个有效 key(及其值长度, len 可传 NULL); 无更多返回 false。
 * 仅输出 CRC 有效、非 tombstone、且无更新版本(去重)的记录。🏠MAIN */
bool et_kv_iter_next(const et_kv_t *kv, et_kv_iter_t *it,
                     uint16_t *key, uint16_t *len);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_KV */
#endif /* ET_KV_H */

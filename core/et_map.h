/**
 * @file    et_map.h
 * @brief   定容开放寻址哈希表 (u32 键值映射, 零动态内存)
 *
 * 定位:
 *  - 小容量(≤ 数百槽)键值映射: 查询/更新 O(1) 均摊, 表可驻任意存储
 *    (调用方决定 SRAM/内存池/静态区, init 时整表清零);
 *  - 开放寻址 + 线性探测 + 探测上限 —— 嵌入式无界查找不可接受, 退化时
 *    提前拒绝而非无限探测;
 *  - 零动态内存, 多实例句柄化。
 *
 * 键空间约定 (⚠ 使用前必读):
 *  - 键 0            = 空槽标记 (EMPTY);
 *  - 键 0xFFFFFFFF   = 删除墓碑 (TOMB, 保探测链不断);
 *  - 用户可用键域    = [1, 0xFFFFFFFE], init/put 对保留键显式拒绝。
 *
 * 容量与退化指引:
 *  - 负载因子建议 ≤ 0.7 (count/cap), 超过后线性探测链显著变长;
 *  - 容量建议取质数 (哈希为乘法散列 + 取模, 质数容量分散更均匀);
 *  - probe_limit = 单次操作最多探测的槽位数: 语义为"超限即拒绝"——
 *    负载健康时取 cap 即永不误伤; 对延迟敏感场景可调小, 代价是退化
 *    (长墓碑链/高负载)时操作返回 false, 由调用方决定扩容或清理;
 *  - 删除采用墓碑: 不破坏探测链, 墓碑可被后续 put 复用 (缩短链);
 *  - ⚠ 全表无空槽时 put 一律拒绝 —— 即使存在墓碑: 新键"不存在"无法在
 *    无空槽终止的探测中证明 (经典开放寻址语义), 故负载指引 ≤0.7。
 *
 * 并发策略: 全部 API 仅限 🏠MAIN 单上下文 (与 et_mempool 同级标注);
 * ISR 使用需调用方临界区包裹。
 */
#ifndef ET_MAP_H
#define ET_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_MAP

#ifdef __cplusplus
extern "C" {
#endif

#define ET_MAP_KEY_EMPTY        0u              /* 空槽标记(保留键)   */
#define ET_MAP_KEY_TOMB         0xFFFFFFFFu     /* 删除墓碑(保留键)   */

typedef struct {
    uint32_t key;               /* 键(0/0xFFFFFFFF 保留, 见头注) */
    uint32_t val;               /* 值(可存放指针位模式)          */
} et_map_slot_t;

typedef void (*et_map_visit_fn)(et_map_slot_t *slot, void *user);

typedef struct et_map {
    et_map_slot_t *slots;       /* 调用方存储(init 整表清零), 勿动 */
    uint32_t       cap;         /* 槽总数, 勿动                    */
    uint32_t       probe_limit; /* 单次操作探测上限, 勿动          */
    uint32_t       count;       /* 有效键值数, 勿动                */
} et_map_t;

/* 初始化: 整表清零(空槽), 校验 cap≥1 / probe_limit≥1。
 * 表可驻任意存储 —— 存储内容在 init 后不保留。🏠MAIN */
bool et_map_init(et_map_t *m, et_map_slot_t *storage, uint32_t cap,
                 uint32_t probe_limit);

/* 插入/覆盖: 键已存在则覆盖值并返回 true; 保留键拒绝;
 * 探测超限(满表/退化)返回 false, 不做任何写入。🏠MAIN */
bool et_map_put(et_map_t *m, uint32_t key, uint32_t val);

/* 查询: 命中返回 true 并写 *val; 不命中/超限/保留键返回 false */
bool et_map_get(const et_map_t *m, uint32_t key, uint32_t *val);

/* 删除: 命中置墓碑返回 true; 不命中/保留键返回 false (探测链保持) */
bool et_map_del(et_map_t *m, uint32_t key);

uint32_t et_map_count(const et_map_t *m);

/* 清空: 整表恢复空槽 (存储仍归调用方) */
void et_map_clear(et_map_t *m);

/* 全量遍历(跳过空槽/墓碑); 回调中删除当前槽安全(开放寻址游标独立)。
 * 回调返回前对槽的修改即生效 (val 可改, key 置墓碑=删除)。🏠MAIN */
bool et_map_foreach(const et_map_t *m, et_map_visit_fn fn, void *user);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_MAP */
#endif /* ET_MAP_H */

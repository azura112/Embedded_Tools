/**
 * @file    et_smap.h
 * @brief   定容字符串键映射 (开放寻址哈希, 零动态内存, 内嵌键存储池)
 *
 * 定位:
 *  - 命令路由表 / 配置项查表 / 名称→句柄注册等小容量字符串键场景
 *    (≤ 数百槽), 查询 O(1) 均摊;
 *  - 与 et_map (u32 键) 是两个独立轻量模块: 键型分离, 不共享实现,
 *    避免 union/双分支拖慢 u32 热路径 (v1.8 计划决议)。
 *
 * 语义与 et_map 完全一致的部分不再重述, 直接交叉引用 core/et_map.h:
 *  - 探测/拒绝规则:      线性探测 + probe_limit"超限即拒绝"(et_map.h 容量与退化指引);
 *  - 墓碑删除与复用:     put 沿途记录首个墓碑, 空槽终止时优先复用墓碑;
 *  - 满表无空槽 put 一律拒绝 (即使有墓碑, 经典开放寻址语义);
 *  - 负载因子建议 ≤0.7, 容量建议质数;
 *  - 并发策略: 全部 API 仅限 🏠MAIN 单上下文 (与 et_mempool 同级)。
 *
 * 键约束:
 *  - 非空、NUL 结尾、长度 ≤ ET_SMAP_KEY_MAX (默认 16, 可 -D 覆盖),
 *    大小写敏感 (折叠列为 v2.0 候选评估);
 *  - 键内容在 put 时**拷入内嵌存储池**, 调用方可释放/复用原串;
 *  - 存储池用尽 → put 拒绝 (不静默截断); 墓碑不回收池空间,
 *    et_smap_clear 整体回收 (见实现注)。
 *
 * 哈希: FNV-1a 32 位 (2166136261 / 16777619), 键长显式入槽做
 * 长度预比较 + 哈希缓存加速碰撞链。
 *
 * 最小可用示例 (3 行):
 *   static et_smap_slot_t slots[31];
 *   static uint8_t pool[512];
 *   et_smap_t m;
 *   et_smap_init(&m, slots, 31u, pool, sizeof(pool), 31u);
 *   et_smap_put(&m, "led", 0x20000008u);
 *   if (et_smap_get(&m, "led", &v)) { ... }
 */
#ifndef ET_SMAP_H
#define ET_SMAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_SMAP

#ifdef __cplusplus
extern "C" {
#endif

#define ET_SMAP_KEY_MAX_DEFAULT 16u     /* -DET_SMAP_KEY_MAX=N 可覆盖 */

#ifndef ET_SMAP_KEY_MAX
#define ET_SMAP_KEY_MAX         ET_SMAP_KEY_MAX_DEFAULT
#endif

#define ET_SMAP_KOFF_EMPTY      0u          /* 槽未占用 (池偏移 0..3 保留) */
#define ET_SMAP_KOFF_TOMB       0xFFFFFFFFu /* 删除墓碑 */

typedef struct {
    uint32_t koff;              /* 池偏移(4B 对齐); EMPTY/TOMB 见上    */
    uint32_t hash;              /* FNV-1a 缓存: 碰撞链先比哈希再比内容 */
    uint32_t val;               /* 值(可存放指针位模式)                */
    uint32_t len;               /* 键字节长 (不含 NUL)                 */
} et_smap_slot_t;

/* 遍历回调: key 指向池内 NUL 结尾串, 回调返回后不得保留指针;
 * val 为槽值指针 (可改, 即生效)。遍历中删除用 et_smap_del(m, key),
 * 安全 (游标独立, 同 et_map foreach 语义)。 */
typedef void (*et_smap_visit_fn)(void *user, const char *key, uint32_t *val);

typedef struct et_smap {
    et_smap_slot_t *slots;      /* 调用方槽存储(init 整表清零), 勿动 */
    uint32_t        cap;        /* 槽总数, 勿动                      */
    uint32_t        probe_limit;/* 单次操作探测上限, 勿动            */
    uint8_t        *pool;       /* 内嵌键存储池(调用方提供), 勿动    */
    uint32_t        pool_size;  /* 池字节总数, 勿动                  */
    uint32_t        pool_used;  /* 池高水位, 勿动                    */
    uint32_t        count;      /* 有效键值数, 勿动                  */
} et_smap_t;

/* 初始化: 整表清零 + 池清零。校验 slots/pool 非空、cap/probe_limit ≥ 1、
 * 池至少装得下一条最坏键 (4B 保留头 + 4B 对齐(KEY_MAX+1) + 对齐余量)。
 * 池总容量与负载指引见头注 —— 池不满足最坏全表属调用方预算决策, 运行时
 * 表现为 put 池满拒绝。🏠MAIN */
bool et_smap_init(et_smap_t *m, et_smap_slot_t *storage, uint32_t cap,
                  uint8_t *keybuf, uint32_t keybuf_size, uint32_t probe_limit);

/* 插入/覆盖: 键已存在则覆盖值返回 true (不重复占池);
 * 键非法(空/超长>ET_SMAP_KEY_MAX)或探测超限或池满 → 拒绝, 不做任何写入
 * (实现按"先探链定位置→再预留池位→最后提交"顺序, 拒绝路径不推进池)。🏠MAIN */
bool et_smap_put(et_smap_t *m, const char *key, uint32_t val);

/* 查询: 命中返回 true 并写 *val (val 可 NULL 仅测存在性) */
bool et_smap_get(const et_smap_t *m, const char *key, uint32_t *val);

/* 删除: 命中置墓碑返回 true (池空间不回收, clear 才回收) */
bool et_smap_del(et_smap_t *m, const char *key);

uint32_t et_smap_count(const et_smap_t *m);
uint32_t et_smap_pool_free(const et_smap_t *m);

/* 清空: 整表回空槽 + 池水位复位 (存储仍归调用方) */
void et_smap_clear(et_smap_t *m);

/* 全量遍历(跳过空槽/墓碑); 语义见 et_smap_visit_fn 注。🏠MAIN */
bool et_smap_foreach(const et_smap_t *m, et_smap_visit_fn fn, void *user);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_SMAP */
#endif /* ET_SMAP_H */

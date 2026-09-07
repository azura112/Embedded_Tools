/**
 * @file    et_map.c
 * @brief   定容开放寻址哈希表实现 (线性探测 + 探测上限 + 墓碑删除)
 *
 * 状态编码: 键 0 = 空槽; 键 0xFFFFFFFF = 墓碑 (二者均为保留键, 见 et_map.h)。
 * 哈希: 乘法散列 (Knuth, 2654435761 = 斐波那契常量) + 取模 —— 容量建议质数。
 * 探测: 线性 +1, 上限 probe_limit; put 沿途记录首个墓碑, 命中空槽时优先
 *       复用墓碑位 (缩短链), 两者都未遇且超限 → 拒绝 (满/退化)。
 */
#include "et_map.h"

#if ET_MODULE_MAP

#include <string.h>

#define ET_MAP_HASH_MULT        2654435761u     /* Knuth 斐波那契散列常量 */

static uint32_t map_hash(uint32_t key, uint32_t cap)
{
    return (uint32_t)(((uint64_t)key * ET_MAP_HASH_MULT) % cap);
}

bool et_map_init(et_map_t *m, et_map_slot_t *storage, uint32_t cap,
                 uint32_t probe_limit)
{
    if ((m == NULL) || (storage == NULL) ||
        (cap == 0u) || (probe_limit == 0u)) {
        return false;
    }
    memset(storage, 0, sizeof(et_map_slot_t) * cap);    /* 全空槽 */
    m->slots       = storage;
    m->cap         = cap;
    m->probe_limit = probe_limit;
    m->count       = 0u;
    return true;
}

static bool map_key_usable(uint32_t key)
{
    return (key != ET_MAP_KEY_EMPTY) && (key != ET_MAP_KEY_TOMB);
}

bool et_map_put(et_map_t *m, uint32_t key, uint32_t val)
{
    uint32_t idx;
    uint32_t probes;
    int32_t  tomb = -1;

    if ((m == NULL) || !map_key_usable(key)) {
        return false;
    }
    idx = map_hash(key, m->cap);
    for (probes = 0u; probes < m->probe_limit; probes++) {
        et_map_slot_t *s = &m->slots[idx];

        if (s->key == key) {
            s->val = val;                   /* 已存在: 覆盖 */
            return true;
        }
        if (s->key == ET_MAP_KEY_EMPTY) {
            /* 空槽终止探测链: 键必不在表内, 落首个墓碑(若有)否则此处 */
            if (tomb >= 0) {
                s = &m->slots[tomb];
                m->count++;                 /* 墓碑位本来就占着表容量 */
                s->key = key;
                s->val  = val;
                return true;
            }
            s->key = key;
            s->val  = val;
            m->count++;
            return true;
        }
        if ((s->key == ET_MAP_KEY_TOMB) && (tomb < 0)) {
            tomb = idx;                     /* 记录首个墓碑, 继续找可能的重键 */
        }
        idx = (idx + 1u) % m->cap;
    }
    return false;                           /* 探测超限: 满/退化, 拒绝 */
}

bool et_map_get(const et_map_t *m, uint32_t key, uint32_t *val)
{
    uint32_t idx;
    uint32_t probes;

    if ((m == NULL) || !map_key_usable(key)) {
        return false;
    }
    idx = map_hash(key, m->cap);
    for (probes = 0u; probes < m->probe_limit; probes++) {
        const et_map_slot_t *s = &m->slots[idx];

        if (s->key == key) {
            if (val != NULL) {
                *val = s->val;
            }
            return true;
        }
        if (s->key == ET_MAP_KEY_EMPTY) {
            return false;                   /* 链终止: 必不在表内 */
        }
        idx = (idx + 1u) % m->cap;          /* 墓碑/他键: 继续 */
    }
    return false;                           /* 超限 */
}

bool et_map_del(et_map_t *m, uint32_t key)
{
    uint32_t idx;
    uint32_t probes;

    if ((m == NULL) || !map_key_usable(key)) {
        return false;
    }
    idx = map_hash(key, m->cap);
    for (probes = 0u; probes < m->probe_limit; probes++) {
        et_map_slot_t *s = &m->slots[idx];

        if (s->key == key) {
            s->key = ET_MAP_KEY_TOMB;       /* 墓碑: 保探测链 */
            s->val = 0u;
            m->count--;
            return true;
        }
        if (s->key == ET_MAP_KEY_EMPTY) {
            return false;
        }
        idx = (idx + 1u) % m->cap;
    }
    return false;
}

uint32_t et_map_count(const et_map_t *m)
{
    return (m != NULL) ? m->count : 0u;
}

void et_map_clear(et_map_t *m)
{
    if (m == NULL) {
        return;
    }
    memset(m->slots, 0, sizeof(et_map_slot_t) * m->cap);
    m->count = 0u;
}

bool et_map_foreach(const et_map_t *m, et_map_visit_fn fn, void *user)
{
    uint32_t i;

    if ((m == NULL) || (fn == NULL)) {
        return false;
    }
    for (i = 0u; i < m->cap; i++) {
        if ((m->slots[i].key != ET_MAP_KEY_EMPTY) &&
            (m->slots[i].key != ET_MAP_KEY_TOMB)) {
            fn(&m->slots[i], user);         /* 回调中删除当前槽安全(索引独立) */
        }
    }
    return true;
}

#endif /* ET_MODULE_MAP */

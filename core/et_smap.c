/**
 * @file    et_smap.c
 * @brief   定容字符串键映射实现 (FNV-1a + 线性探测 + 墓碑 + 内嵌键池)
 *
 * 探测/墓碑/拒绝语义与 et_map.c 同构 (决议见 et_smap.h 头注), 差异点:
 *  - 键比较: 哈希缓存预筛 → len 比较 → memcmp;
 *  - put 新键: 目标槽确定后才向池申请 4B 对齐空间 (池满则整体拒绝,
 *    先定位后分配, 保证失败无副作用);
 *  - 键含终止 NUL 一并拷入池, 遍历回调可直接把池内指针当 C 字符串;
 *  - 池偏移 0..3 永久保留 (koff==0 = EMPTY 哨兵, 真实键从 4 起分配)。
 */
#include "et_smap.h"

#if ET_MODULE_SMAP

#include <string.h>

#define ET_SMAP_FNV_OFFSET  2166136261u
#define ET_SMAP_FNV_PRIME   16777619u

/* 键字节数上界 (含 NUL 与 4B 对齐进位) 的最坏池占用 */
#define SMAP_KEY_WORST_BYTES    (((ET_SMAP_KEY_MAX + 1u) + 3u) & ~3u)

static uint32_t smap_hash(const char *key, uint32_t len)
{
    uint32_t h = ET_SMAP_FNV_OFFSET;
    uint32_t i;

    for (i = 0u; i < len; i++) {
        h ^= (uint32_t)(uint8_t)key[i];
        h *= ET_SMAP_FNV_PRIME;
    }
    return h;
}

/* 键合法性: 非空、NUL 结尾、len ≤ KEY_MAX。返回长度; 非法返回 0
 * (len 0 与非法同值域, 调用方以 0 判非法 —— 空键本就不允许) */
static uint32_t smap_key_len_ok(const char *key)
{
    uint32_t i;

    if (key == NULL) {
        return 0u;
    }
    for (i = 0u; i <= ET_SMAP_KEY_MAX; i++) {
        if (key[i] == '\0') {
            return i;                 /* i==0 → 空键 → 上层判非法 */
        }
    }
    return 0u;                        /* 超长: 前缀 KEY_MAX 内无 NUL */
}

static bool smap_slot_live(const et_smap_slot_t *s)
{
    return (s->koff != ET_SMAP_KOFF_EMPTY) && (s->koff != ET_SMAP_KOFF_TOMB);
}

bool et_smap_init(et_smap_t *m, et_smap_slot_t *storage, uint32_t cap,
                  uint8_t *keybuf, uint32_t keybuf_size, uint32_t probe_limit)
{
    if ((m == NULL) || (storage == NULL) || (keybuf == NULL) ||
        (cap == 0u) || (probe_limit == 0u) ||
        (keybuf_size < 4u + SMAP_KEY_WORST_BYTES)) {   /* 保留头 + 一条最坏键 */
        return false;
    }
    memset(storage, 0, sizeof(et_smap_slot_t) * cap);  /* 全空槽 */
    memset(keybuf, 0, keybuf_size);
    m->slots       = storage;
    m->cap         = cap;
    m->probe_limit = probe_limit;
    m->pool        = keybuf;
    m->pool_size   = keybuf_size;
    m->pool_used   = 4u;                               /* koff==0 哨兵保留 */
    m->count       = 0u;
    return true;
}

/* 探测: 返回命中槽; 未命中返回 NULL, *free_idx 给出可插入位置
 * (首个墓碑优先, 否则首个空槽; -1 = 探测超限或满表)。
 * 与 et_map_put 的"沿途记录首个墓碑"规则一致。 */
static et_smap_slot_t *smap_probe(et_smap_t *m, const char *key, uint32_t len,
                                  uint32_t hash, int32_t *free_idx)
{
    uint32_t idx = hash % m->cap;
    uint32_t probes;
    int32_t  tomb = -1;

    *free_idx = -1;
    for (probes = 0u; probes < m->probe_limit; probes++) {
        et_smap_slot_t *s = &m->slots[idx];

        if (s->koff == ET_SMAP_KOFF_EMPTY) {
            *free_idx = (tomb >= 0) ? tomb : (int32_t)idx;
            return NULL;                /* 链终止: 键必不在表内 */
        }
        if (smap_slot_live(s) &&
            (s->hash == hash) && (s->len == len) &&
            (memcmp(&m->pool[s->koff], key, len) == 0)) {
            return s;                   /* 命中 */
        }
        if ((s->koff == ET_SMAP_KOFF_TOMB) && (tomb < 0)) {
            tomb = (int32_t)idx;
        }
        idx = (idx + 1u) % m->cap;
    }
    return NULL;                        /* 超限: 满/退化 */
}

bool et_smap_put(et_smap_t *m, const char *key, uint32_t val)
{
    uint32_t       len;
    uint32_t       hash;
    int32_t        fi;
    et_smap_slot_t *hit;
    et_smap_slot_t *s;
    uint32_t       need;

    if (m == NULL) {
        return false;
    }
    len = smap_key_len_ok(key);
    if (len == 0u) {
        return false;                   /* 空键/NULL/超长 */
    }
    hash = smap_hash(key, len);
    hit  = smap_probe(m, key, len, hash, &fi);
    if (hit != NULL) {
        hit->val = val;                 /* 已存在: 覆盖, 不动池 */
        return true;
    }
    if (fi < 0) {
        return false;                   /* 探测超限 */
    }
    need  = (len + 1u + 3u) & ~3u;      /* 含 NUL, 4B 对齐 */
    if (m->pool_used + need > m->pool_size) {
        return false;                   /* 池满: 拒绝, 不推进水位 */
    }
    s = &m->slots[fi];
    m->count++;                         /* 空槽/墓碑插入均入数 (et_map 同规则:
                                         * 墓碑在 del 时已出数, 复用即新表项) */
    memcpy(&m->pool[m->pool_used], key, len + 1u);
    s->koff = m->pool_used;
    s->hash = hash;
    s->len  = len;
    s->val  = val;
    m->pool_used += need;
    return true;
}

bool et_smap_get(const et_smap_t *m, const char *key, uint32_t *val)
{
    uint32_t len;
    uint32_t hash;
    uint32_t idx;
    uint32_t probes;

    if ((m == NULL) || ((len = smap_key_len_ok(key)) == 0u)) {
        return false;
    }
    hash = smap_hash(key, len);
    idx  = hash % m->cap;
    for (probes = 0u; probes < m->probe_limit; probes++) {
        const et_smap_slot_t *s = &m->slots[idx];

        if (s->koff == ET_SMAP_KOFF_EMPTY) {
            return false;               /* 链终止 */
        }
        if (smap_slot_live(s) &&
            (s->hash == hash) && (s->len == len) &&
            (memcmp(&m->pool[s->koff], key, len) == 0)) {
            if (val != NULL) {
                *val = s->val;
            }
            return true;
        }
        idx = (idx + 1u) % m->cap;
    }
    return false;
}

bool et_smap_del(et_smap_t *m, const char *key)
{
    uint32_t len;
    uint32_t hash;
    uint32_t idx;
    uint32_t probes;

    if ((m == NULL) || ((len = smap_key_len_ok(key)) == 0u)) {
        return false;
    }
    hash = smap_hash(key, len);
    idx  = hash % m->cap;
    for (probes = 0u; probes < m->probe_limit; probes++) {
        et_smap_slot_t *s = &m->slots[idx];

        if (s->koff == ET_SMAP_KOFF_EMPTY) {
            return false;
        }
        if (smap_slot_live(s) &&
            (s->hash == hash) && (s->len == len) &&
            (memcmp(&m->pool[s->koff], key, len) == 0)) {
            s->koff = ET_SMAP_KOFF_TOMB;    /* 池空间不回收 (头注) */
            s->hash = 0u;
            s->val  = 0u;
            s->len  = 0u;
            m->count--;
            return true;
        }
        idx = (idx + 1u) % m->cap;
    }
    return false;
}

uint32_t et_smap_count(const et_smap_t *m)
{
    return (m != NULL) ? m->count : 0u;
}

uint32_t et_smap_pool_free(const et_smap_t *m)
{
    if ((m == NULL) || (m->pool_used >= m->pool_size)) {
        return 0u;
    }
    return m->pool_size - m->pool_used;
}

void et_smap_clear(et_smap_t *m)
{
    if (m == NULL) {
        return;
    }
    memset(m->slots, 0, sizeof(et_smap_slot_t) * m->cap);
    m->pool_used = 4u;
    m->count     = 0u;
}

bool et_smap_foreach(const et_smap_t *m, et_smap_visit_fn fn, void *user)
{
    uint32_t i;

    if ((m == NULL) || (fn == NULL)) {
        return false;
    }
    for (i = 0u; i < m->cap; i++) {
        const et_smap_slot_t *s = &m->slots[i];

        if (smap_slot_live(s)) {
            /* 非 const 视图: val 可改即生效 (语义同 et_map foreach);
             * 回调内 et_smap_del 安全 —— 本循环以独立索引推进 */
            fn(user, (const char *)&m->pool[s->koff],
               (uint32_t *)&((et_smap_t *)m)->slots[i].val);
        }
    }
    return true;
}

#endif /* ET_MODULE_SMAP */

/**
 * @file    et_params.c
 * @brief   参数版本化与迁移 (见 et_params.h 的可靠性模型)
 */
#include "et_params.h"

#if ET_MODULE_PARAMS

#include <string.h>

/* 工作区槽: [0] = 值长度, [1] = flags(bit0 = present), [2..] = 载荷 */
#define SLOT_LEN(s)        ((s)[0])
#define SLOT_FLAGS(s)      ((s)[1])
#define SLOT_PRESENT       0x01u
#define SLOT_VAL(s)        ((s) + 2)

/* 版本键: gen g 的数据版本 (与 GEN 提交点交叉校验) */
#define KEY_VER(g)         ((uint16_t)(ET_PARAMS_KEY_BASE + 2u * ET_PARAMS_KEY_STRIDE + (g)))
#define KEY_FIELD(g, i)    ((uint16_t)(ET_PARAMS_KEY_BASE + (uint16_t)(g) * ET_PARAMS_KEY_STRIDE + (i)))

static uint8_t *slot_of(const et_params_t *p, uint16_t field)
{
    return p->work + (size_t)field * ET_PARAMS_SLOT_BYTES;
}

static uint16_t type_width(uint8_t type)
{
    switch (type) {
        case ET_PARAM_U8:  return 1u;
        case ET_PARAM_U16: return 2u;
        case ET_PARAM_U32:
        case ET_PARAM_I32: return 4u;
        case ET_PARAM_BLOB: return 0u;   /* 由 desc->len 决定 */
        default:            return 0xFFFFu;
    }
}

static uint16_t field_width(const et_param_desc_t *d)
{
    uint16_t w = type_width(d->type);
    return (d->type == ET_PARAM_BLOB) ? d->len : w;
}

static int32_t load_i32(const uint8_t *v, uint8_t len)
{
    if (len == 1u) {
        return (int32_t)v[0];
    }
    if (len == 2u) {
        return (int32_t)((uint16_t)v[0] | ((uint16_t)v[1] << 8));
    }
    return (int32_t)((uint32_t)v[0] | ((uint32_t)v[1] << 8) |
                     ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24));
}

/* 描述符合法性 (init 时一次性校验) */
static bool desc_ok(const et_param_desc_t *d)
{
    uint16_t w;
    if (d == NULL) {
        return false;
    }
    w = type_width(d->type);
    if (w == 0xFFFFu) {
        return false;
    }
    if (d->type == ET_PARAM_BLOB) {
        if ((d->len == 0u) || (d->len > ET_PARAMS_VAL_MAX)) {
            return false;
        }
        if ((d->def == NULL) || (d->def_len != d->len)) {
            return false;
        }
    } else {
        if ((d->def == NULL) || (d->def_len < (uint8_t)w)) {
            return false;
        }
        if (d->min_i > d->max_i) {
            return false;
        }
    }
    return true;
}

static void slot_set_default(const et_param_desc_t *d, uint8_t *s)
{
    uint16_t w = field_width(d);
    SLOT_FLAGS(s) = SLOT_PRESENT;
    SLOT_LEN(s) = (uint8_t)w;
    memcpy(SLOT_VAL(s), d->def, w);
}

/* 单字段合法性: 长度 + 数值范围 */
static bool field_value_ok(const et_param_desc_t *d, const uint8_t *val, uint16_t len)
{
    uint16_t w = field_width(d);
    int32_t v;
    if (len != w) {
        return false;
    }
    if (d->type == ET_PARAM_BLOB) {
        return true;
    }
    v = load_i32(val, (uint8_t)w);
    if ((d->type != ET_PARAM_I32) && (v < 0)) {
        return false;
    }
    return (v >= d->min_i) && (v <= d->max_i);
}

static void work_defaults(et_params_t *p)
{
    uint16_t i;
    for (i = 0u; i < p->schema->count; i++) {
        slot_set_default(&p->schema->desc[i], slot_of(p, i));
    }
}

/* 尝试加载代际 g: 版本键 + GEN 交叉校验 + 全字段校验。
 * 成功返回 true; ver_out = 数据版本。缺字段处理: since_ver > ver 的字段取默认。 */
static bool load_gen(et_params_t *p, uint8_t g, uint16_t gen_ver, uint16_t *ver_out)
{
    uint8_t vbuf[2];
    uint16_t vlen = 0u;
    uint16_t ver;
    uint16_t i;

    if (!et_kv_get(p->kv, KEY_VER(g), vbuf, sizeof(vbuf), &vlen) || (vlen != 2u)) {
        return false;
    }
    ver = (uint16_t)((uint16_t)vbuf[0] | ((uint16_t)vbuf[1] << 8));
    if (ver != gen_ver) {
        return false;
    }
    for (i = 0u; i < p->schema->count; i++) {
        const et_param_desc_t *d = &p->schema->desc[i];
        uint8_t *s = slot_of(p, i);
        uint8_t tmp[ET_PARAMS_VAL_MAX];
        uint16_t got = 0u;

        if (!et_kv_get(p->kv, KEY_FIELD(g, i), tmp, (uint16_t)sizeof(tmp), &got)) {
            if (d->since_ver > ver) {
                slot_set_default(d, s);      /* 旧版本尚无该字段 */
                continue;
            }
            return false;
        }
        if (!field_value_ok(d, tmp, got)) {
            return false;
        }
        SLOT_FLAGS(s) = SLOT_PRESENT;
        SLOT_LEN(s) = (uint8_t)got;
        memcpy(SLOT_VAL(s), tmp, got);
    }
    *ver_out = ver;
    return true;
}

bool et_params_init(et_params_t *p, et_kv_t *kv, const et_params_schema_t *schema,
                    void *work, size_t work_cap)
{
    uint16_t i;

    if ((p == NULL) || (kv == NULL) || (schema == NULL) || (schema->desc == NULL) ||
        (work == NULL)) {
        return false;
    }
    if ((schema->count == 0u) || (schema->count > ET_PARAMS_MAX_FIELDS) ||
        (schema->schema_ver == 0u)) {
        return false;
    }
    if (work_cap < ET_PARAMS_WORK_SIZE(schema->count)) {
        return false;
    }
    for (i = 0u; i < schema->count; i++) {
        if (!desc_ok(&schema->desc[i])) {
            return false;
        }
    }
    memset(p, 0, sizeof(*p));
    p->kv = kv;
    p->schema = schema;
    p->work = (uint8_t *)work;
    p->work_cap = (uint16_t)(work_cap > 0xFFFFu ? 0xFFFFu : work_cap);
    p->gen = 0u;
    p->loaded = false;
    p->loaded_ver = 0u;
    (void)et_params_load(p);        /* 空 kv/坏数据 → 默认值 (loaded=false) */
    return true;
}

bool et_params_load(et_params_t *p)
{
    uint8_t gbuf[3];
    uint16_t glen = 0u;
    uint8_t g;
    uint16_t ver;
    uint16_t loaded = 0u;

    if ((p == NULL) || (p->kv == NULL)) {
        return false;
    }
    p->loaded = false;
    p->loaded_ver = 0u;
    if (!et_kv_get(p->kv, ET_PARAMS_KEY_GEN, gbuf, sizeof(gbuf), &glen) || (glen != 3u) ||
        (gbuf[0] > 1u)) {
        work_defaults(p);
        return false;
    }
    g = gbuf[0];
    ver = (uint16_t)((uint16_t)gbuf[1] | ((uint16_t)gbuf[2] << 8));
    if (ver == 0u) {
        work_defaults(p);
        return false;
    }
    if (load_gen(p, g, ver, &loaded)) {
        p->gen = g;
        p->loaded_ver = loaded;
        p->loaded = true;
        return true;
    }
    /* 当前代际损坏: 回退另一代际 (版本键自证, 不假定为当前 schema) */
    if (load_gen(p, (uint8_t)(1u - g), ver, &loaded)) {
        p->gen = (uint8_t)(1u - g);
        p->loaded_ver = loaded;
        p->loaded = true;
        return true;
    }
    work_defaults(p);
    return false;
}

uint16_t et_params_validate(const et_params_t *p)
{
    uint16_t i;
    if ((p == NULL) || (p->schema == NULL)) {
        return 0xFFFFu;
    }
    for (i = 0u; i < p->schema->count; i++) {
        const uint8_t *s = p->work + (size_t)i * ET_PARAMS_SLOT_BYTES;
        if ((SLOT_FLAGS(s) & SLOT_PRESENT) == 0u) {
            return i;
        }
        if (!field_value_ok(&p->schema->desc[i], SLOT_VAL(s), SLOT_LEN(s))) {
            return i;
        }
    }
    return p->schema->count;
}

bool et_params_save(et_params_t *p)
{
    uint8_t g;
    uint8_t vbuf[2];
    uint8_t gbuf[3];
    uint16_t i;

    if ((p == NULL) || (p->kv == NULL) || (p->schema == NULL)) {
        return false;
    }
    if (et_params_validate(p) != p->schema->count) {
        return false;
    }
    g = (uint8_t)(1u - p->gen);
    for (i = 0u; i < p->schema->count; i++) {
        const uint8_t *s = slot_of(p, i);
        if (!et_kv_set(p->kv, KEY_FIELD(g, i), SLOT_VAL(s), SLOT_LEN(s))) {
            return false;
        }
    }
    vbuf[0] = (uint8_t)(p->schema->schema_ver & 0xFFu);
    vbuf[1] = (uint8_t)((p->schema->schema_ver >> 8) & 0xFFu);
    if (!et_kv_set(p->kv, KEY_VER(g), vbuf, 2u)) {
        return false;
    }
    if (!et_kv_commit(p->kv)) {
        return false;
    }
    gbuf[0] = g;
    gbuf[1] = vbuf[0];
    gbuf[2] = vbuf[1];
    if (!et_kv_set(p->kv, ET_PARAMS_KEY_GEN, gbuf, 3u)) {   /* 提交点 */
        return false;
    }
    p->gen = g;
    p->loaded_ver = p->schema->schema_ver;
    p->loaded = true;
    return true;
}

bool et_params_get(const et_params_t *p, uint16_t field, void *buf, uint16_t cap,
                   uint16_t *out_len)
{
    const uint8_t *s;
    if ((p == NULL) || (p->schema == NULL) || (buf == NULL) || (field >= p->schema->count)) {
        return false;
    }
    s = p->work + (size_t)field * ET_PARAMS_SLOT_BYTES;
    if ((SLOT_FLAGS(s) & SLOT_PRESENT) == 0u) {
        return false;
    }
    if (cap < SLOT_LEN(s)) {
        return false;
    }
    memcpy(buf, SLOT_VAL(s), SLOT_LEN(s));
    if (out_len != NULL) {
        *out_len = SLOT_LEN(s);
    }
    return true;
}

bool et_params_set(et_params_t *p, uint16_t field, const void *val, uint16_t len)
{
    uint8_t *s;
    const et_param_desc_t *d;
    if ((p == NULL) || (p->schema == NULL) || (val == NULL) || (field >= p->schema->count)) {
        return false;
    }
    d = &p->schema->desc[field];
    if (!field_value_ok(d, (const uint8_t *)val, len)) {
        return false;
    }
    s = slot_of(p, field);
    SLOT_FLAGS(s) = SLOT_PRESENT;
    SLOT_LEN(s) = (uint8_t)len;
    memcpy(SLOT_VAL(s), val, len);
    return true;
}

bool et_params_factory_reset(et_params_t *p)
{
    uint16_t i;
    if ((p == NULL) || (p->schema == NULL)) {
        return false;
    }
    for (i = 0u; i < p->schema->count; i++) {
        const et_param_desc_t *d = &p->schema->desc[i];
        if (!d->keep_on_factory_reset) {
            slot_set_default(d, slot_of(p, i));
        }
    }
    if (!et_params_save(p)) {
        (void)et_params_load(p);        /* 回滚到已提交代际 */
        return false;
    }
    return true;
}

bool et_params_migrate(et_params_t *p)
{
    uint16_t v;
    uint16_t i;

    if ((p == NULL) || (p->schema == NULL)) {
        return false;
    }
    if (!p->loaded) {
        return false;
    }
    if (p->loaded_ver > p->schema->schema_ver) {
        return false;                   /* 数据版本高于代码: 不降级 */
    }
    if (p->loaded_ver == p->schema->schema_ver) {
        return true;
    }
    if (p->schema->migrate == NULL) {
        return false;
    }
    for (v = p->loaded_ver; v < p->schema->schema_ver; v++) {
        uint16_t to = (uint16_t)(v + 1u);
        for (i = 0u; i < p->schema->count; i++) {
            const et_param_desc_t *d = &p->schema->desc[i];
            uint8_t *s = slot_of(p, i);
            uint8_t nv[ET_PARAMS_VAL_MAX];
            uint16_t nlen = 0u;

            if (d->since_ver > v) {
                slot_set_default(d, s);     /* 该步新增字段: 默认值 */
                continue;
            }
            if (!p->schema->migrate(v, to, i, d, SLOT_VAL(s), SLOT_LEN(s),
                                    nv, &nlen, p->schema->migrate_ctx)) {
                (void)et_params_load(p);    /* 回滚旧配置 */
                return false;
            }
            if (!field_value_ok(d, nv, nlen)) {
                (void)et_params_load(p);
                return false;
            }
            SLOT_FLAGS(s) = SLOT_PRESENT;
            SLOT_LEN(s) = (uint8_t)nlen;
            memcpy(SLOT_VAL(s), nv, nlen);
        }
    }
    p->loaded_ver = p->schema->schema_ver;
    return true;
}

uint16_t et_params_loaded_version(const et_params_t *p)
{
    return (p == NULL) ? 0u : p->loaded_ver;
}

#endif /* ET_MODULE_PARAMS */

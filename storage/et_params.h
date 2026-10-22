/**
 * @file    et_params.h
 * @brief   参数版本化与迁移 (定容 schema + 代际原子保存 + 逐版迁移 + 恢复出厂)
 *
 * 定位: 建在 et_kv 之上的"参数表"层——schema 由调用方静态提供, 值按字段定长存放;
 * 不是通用配置框架 (无字符串键/无动态类型/无导出导入)。
 *
 * 可靠性模型 (代际原子, REQ-12 验收 3):
 *  - 值按"代际"存放: gen g 的字段 i 存于 key = ET_PARAMS_KEY_BASE + g*ET_PARAMS_KEY_STRIDE + i;
 *    已提交代际与数据版本存于单键 ET_PARAMS_KEY_GEN (值 = {gen, ver_lo, ver_hi});
 *  - save() 顺序 = 先写非活跃代际的全部字段 → et_kv_commit() 压实 → **最后**写 GEN 键;
 *    GEN 写入即提交点: 之前掉电 ⇒ 加载仍选旧代际 (完整旧配置); 之后掉电 ⇒ 新代际完整;
 *  - load() 按 GEN 选代际, 逐字段校验 (存在/长度/范围); 当前代际损坏 ⇒ 回退另一代际,
 *    但**仅限同版本代际** (另一代际的 KEY_VER 须等于 GEN 记录的版本; 跨版本损坏 ⇒ 不回退,
 *    落默认值并返回 false); 两代际均不可用 ⇒ false (调用方可 et_params_factory_reset)。
 *
 * 版本与迁移 (REQ-12 验收 1/2):
 *  - schema_ver 为当前代码的 schema 版本; 加载后若数据版本 < schema_ver, et_params_migrate()
 *    逐版步进 (v→v+1): 该步新增字段 (desc->since_ver > v) 取默认值, 既有字段交
 *    schema->migrate 回调换算; 任一步/任字段失败 ⇒ 丢弃工作区并重载旧配置 (回滚);
 *  - 迁移只改工作区, 持久化由调用方随后 save() 完成 (save 失败亦保留旧代际)。
 *
 * 资源: 零动态内存。schema 表与工作区由调用方提供 —— 工作区大小 =
 *    ET_PARAMS_WORK_SIZE(count) 字节 (每字段 1 个定长槽); 句柄为定长结构。
 *
 * 并发约定: 全部 API 仅限 🏠MAIN (flash 擦写与迁移不可 ISR 调用)。
 */
#ifndef ET_PARAMS_H
#define ET_PARAMS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "et_config.h"

#if ET_MODULE_PARAMS

#include "et_kv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== 容量与键空间 ===================== */
#define ET_PARAMS_MAX_FIELDS     32u     /* 字段数上限 (工作区/键空间按此定界) */
#define ET_PARAMS_VAL_MAX        30u     /* 单字段值上限 (槽内 2B 头 + 30B 载荷) */
#define ET_PARAMS_SLOT_BYTES     32u     /* 工作区每字段槽宽 */
#define ET_PARAMS_WORK_SIZE(n)   ((size_t)(n) * ET_PARAMS_SLOT_BYTES)

#define ET_PARAMS_KEY_GEN        0x7FF0u /* 提交点键 (值 = gen, ver_lo, ver_hi) */
#define ET_PARAMS_KEY_BASE       0x6000u /* gen 0 字段 0 */
#define ET_PARAMS_KEY_STRIDE     0x0100u /* gen 1 字段 0 = BASE + STRIDE */

/* ===================== schema ===================== */
typedef enum {
    ET_PARAM_U8 = 1,
    ET_PARAM_U16,
    ET_PARAM_U32,
    ET_PARAM_I32,
    ET_PARAM_BLOB
} et_param_type_t;

typedef struct {
    uint8_t     type;       /* et_param_type_t */
    uint8_t     len;        /* 数值类型自动 = 宽度; BLOB 为定长 (1..ET_PARAMS_VAL_MAX) */
    uint8_t     group;      /* 调用方自定义分组 (诊断/导出用) */
    bool        keep_on_factory_reset;  /* true = 设备身份/校准类, 恢复出厂保留 */
    uint16_t    since_ver;  /* 引入该字段的 schema 版本 (迁移用) */
    int32_t     min_i;      /* 数值下界 (含); BLOB 忽略; U32 按 uint32_t 位型解释 (max_i = -1 即 0xFFFFFFFF) */
    int32_t     max_i;      /* 数值上界 (含); BLOB 忽略 */
    const void *def;        /* 默认值 (数值: 指向对应宽度整数; BLOB: def_len 字节) */
    uint8_t     def_len;
    const char *name;       /* 诊断名 (可为 NULL) */
} et_param_desc_t;

/* 迁移回调: 把字段从 from_ver 的旧值换成 to_ver (= from_ver+1) 的新值。
 * old_val/old_len 为旧值, new_val 容量 ET_PARAMS_VAL_MAX, 写回 *new_len。
 * 返回 true = 已换算; false = 无法迁移 (整次迁移失败并回滚)。🏠MAIN */
typedef bool (*et_params_migrate_fn)(uint16_t from_ver, uint16_t to_ver,
                                     uint16_t field,
                                     const et_param_desc_t *desc,
                                     const void *old_val, uint16_t old_len,
                                     void *new_val, uint16_t *new_len,
                                     void *ctx);

typedef struct {
    const et_param_desc_t *desc;   /* 字段表 (count 项, 按 field index 顺序) */
    uint16_t               count;
    uint16_t               schema_ver;   /* 当前代码 schema 版本 (>=1) */
    et_params_migrate_fn   migrate;      /* 无迁移需求可为 NULL */
    void                  *migrate_ctx;
} et_params_schema_t;

/* ===================== 句柄 ===================== */
typedef struct {
    et_kv_t                  *kv;      /* 由调用方 init/format, 生命周期覆盖句柄 */
    const et_params_schema_t *schema;
    uint8_t                  *work;    /* 调用方工作区, 大小 ET_PARAMS_WORK_SIZE(count) */
    uint16_t                  work_cap;
    uint8_t                   gen;     /* 已提交代际 0/1 */
    uint16_t                  loaded_ver;  /* 已加载数据的 schema 版本 */
    bool                      loaded;  /* 是否从存储成功加载 (false = 默认值/出厂态) */
} et_params_t;

/* 初始化: 校验 schema/工作区后 load()。空 kv 视为出厂态 (loaded=false, 工作区=默认值)。
 * 参数非法 (count=0/>MAX、slot 与 len 不合法、工作区不足) 返回 false 且不触碰 kv。🏠MAIN */
bool et_params_init(et_params_t *p, et_kv_t *kv, const et_params_schema_t *schema,
                    void *work, size_t work_cap);

/* 加载: 按 GEN 选代际并逐字段校验; 当前代际坏则回退另一代际(仅同版本代际;
 * 跨版本代际不参与回退)。成功 loaded=true/loaded_ver=数据版本; 两代际均坏返回 false 且工作区置默认值。🏠MAIN */
bool et_params_load(et_params_t *p);

/* 保存 (代际原子): 写非活跃代际 → 压实 → 写 GEN 提交点。任一步失败返回 false,
 * 已提交代际不变 (旧配置仍可加载)。🏠MAIN */
bool et_params_save(et_params_t *p);

/* 校验工作区全部字段 (长度 + 数值范围); 返回首个非法字段 index, count = 全部合法。🏠MAIN */
uint16_t et_params_validate(const et_params_t *p);

/* 读取字段值: 拷贝到 buf (cap 不足返回 false 且不写 buf); out_len 可 NULL。🏠MAIN */
bool et_params_get(const et_params_t *p, uint16_t field, void *buf, uint16_t cap,
                   uint16_t *out_len);

/* 写入字段值: 校验类型/长度/范围后写工作区 (不落盘); 非法返回 false。🏠MAIN */
bool et_params_set(et_params_t *p, uint16_t field, const void *val, uint16_t len);

/* 恢复出厂: keep 字段保留当前值, 其余回默认值并 save(); 失败时工作区回滚。🏠MAIN */
bool et_params_factory_reset(et_params_t *p);

/* 逐版迁移: loaded_ver → schema_ver 步进; 失败则重载旧配置 (回滚) 并返回 false。
 * 成功只改工作区, 需调用方 save() 持久化。🏠MAIN */
bool et_params_migrate(et_params_t *p);

/* 当前已加载数据的 schema 版本 (未加载成功时为 0)。🏠MAIN */
uint16_t et_params_loaded_version(const et_params_t *p);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_PARAMS */
#endif /* ET_PARAMS_H */

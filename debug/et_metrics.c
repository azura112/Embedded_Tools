/**
 * @file    et_metrics.c
 * @brief   命名运行指标注册表 + 裁剪/特性自描述实现 (v2.27, REQ-6/REQ-8)
 */
#include "et_metrics.h"

#if ET_MODULE_METRICS

#include <stddef.h>

#if ET_MODULE_SHELL
#include "et_shell.h"
#endif
#if ET_MODULE_LOG
#include "et_log.h"
#endif
#if ET_MODULE_HIST
#include "et_hist.h"
#endif

/* ---- 命令面输出缓冲预算(两份渲染共用: 命令在主循环内串行执行) ----
 * 指标行最坏 = 2 缩进 + 键(ET_METRICS_KEY_MAX=16) + '=' + 值(10) + ' ' + 类别 + LF = 31
 * 特性行最坏 = 2 缩进 + 名(≤10) + '=' + '0' + LF = 15, 行数 = 开关数(本版 32)
 * 768 覆盖"默认 ET_METRICS_MAX=16 槽"与"32 开关"两种渲染的上界(留余量)。
 * 截断口径: 绑定实例的 cap 大于 ET_METRICS_MAX 时 dump 行可能被截断——
 * 调用方可自备更大缓冲直调 et_metrics_format/et_features_format(语义不变)。 */
#define ET_METRICS_IO_MAX   768u

/* ---------------- 键与写入器小工具(零 libc 依赖, 零动态内存) ---------------- */

/* 键合法性: 非空、NUL 结尾、长度 ≤ ET_METRICS_KEY_MAX。
 * 扫描至多读 ET_METRICS_KEY_MAX+1 字节(超长键在第 +1 字节处即判失败),
 * 因此对"恰好 16 字符 + NUL"与"17 字符"两种调用方缓冲都安全。 */
static bool m_key_ok(const char *key)
{
    uint32_t n;

    if (key == NULL) {
        return false;
    }
    for (n = 0u; n <= ET_METRICS_KEY_MAX; n++) {
        if (key[n] == '\0') {
            break;
        }
    }
    return (n >= 1u) && (n <= ET_METRICS_KEY_MAX);
}

static void m_key_copy(char *dst, const char *src)
{
    uint32_t i;

    for (i = 0u; i < ET_METRICS_KEY_MAX; i++) {
        if (src[i] == '\0') {
            break;
        }
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

/* 边界受控写入器: pos 为已写入字节数, 空间不足即丢弃(截断), 永不越界 */
typedef struct {
    char    *buf;
    uint32_t cap;
    uint32_t pos;
} m_sink_t;

static void m_ch(m_sink_t *s, char c)
{
    if ((s->cap != 0u) && (s->pos + 1u < s->cap)) {
        s->buf[s->pos] = c;
        s->pos++;
    }
}

static void m_str(m_sink_t *s, const char *str)
{
    while (*str != '\0') {
        m_ch(s, *str);
        str++;
    }
}

static void m_u32(m_sink_t *s, uint32_t v)
{
    char tmp[10];
    uint32_t n = 0u;

    do {
        tmp[n] = (char)('0' + (v % 10u));
        n++;
        v /= 10u;
    } while ((v != 0u) && (n < sizeof(tmp)));

    while (n != 0u) {
        n--;
        m_ch(s, tmp[n]);
    }
}

static void m_finish(m_sink_t *s)
{
    if (s->cap != 0u) {
        s->buf[s->pos] = '\0';
    }
}

/* ---------------- 注册表 ---------------- */

static et_metrics_slot_t *m_find(const et_metrics_t *m, const char *key)
{
    uint32_t i;
    uint32_t j;

    if ((m == NULL) || (m->slots == NULL) || (key == NULL)) {
        return NULL;
    }
    for (i = 0u; i < m->cap; i++) {
        if (m->slots[i].used == 0u) {
            continue;
        }
        /* 键已在登记时校验过长度上限, 这里含终止符一起比到首个差异 */
        for (j = 0u; j <= ET_METRICS_KEY_MAX; j++) {
            if (m->slots[i].key[j] != key[j]) {
                break;
            }
            if (key[j] == '\0') {
                return &m->slots[i];
            }
        }
    }
    return NULL;
}

bool et_metrics_init(et_metrics_t *m, et_metrics_slot_t *slots, uint32_t cap)
{
    uint32_t i;

    if ((m == NULL) || (slots == NULL) || (cap == 0u)) {
        return false;
    }
    for (i = 0u; i < cap; i++) {
        slots[i].key[0] = '\0';
        slots[i].val    = 0u;
        slots[i].hist   = NULL;
        slots[i].kind   = ET_METRIC_COUNTER;
        slots[i].used   = 0u;
    }
    m->slots   = slots;
    m->cap     = cap;
    m->count   = 0u;
    m->dropped = 0u;
    return true;
}

static bool m_register(et_metrics_t *m, const char *key, uint8_t kind)
{
    uint32_t i;

    if ((m == NULL) || (m->slots == NULL) || !m_key_ok(key)) {
        if ((m != NULL) && (m->slots != NULL)) {
            m->dropped++;
        }
        return false;
    }
    if (m_find(m, key) != NULL) {          /* 重名拒绝: 既有指标零改动 */
        m->dropped++;
        return false;
    }
    for (i = 0u; i < m->cap; i++) {
        if (m->slots[i].used == 0u) {
            m_key_copy(m->slots[i].key, key);
            m->slots[i].val  = 0u;
            m->slots[i].hist = NULL;
            m->slots[i].kind = kind;
            m->slots[i].used = 1u;
            m->count++;
            return true;
        }
    }
    m->dropped++;                          /* 满表: 不覆盖任何既有指标 */
    return false;
}

bool et_metrics_register_counter(et_metrics_t *m, const char *key)
{
    return m_register(m, key, (uint8_t)ET_METRIC_COUNTER);
}

bool et_metrics_register_gauge(et_metrics_t *m, const char *key)
{
    return m_register(m, key, (uint8_t)ET_METRIC_GAUGE);
}

bool et_metrics_link_hist(et_metrics_t *m, const char *key, void *hist)
{
    et_metrics_slot_t *s = m_find(m, key);

    if (s == NULL) {
        return false;
    }
    s->hist = hist;
    return true;
}

void et_metrics_inc(et_metrics_t *m, const char *key)
{
    et_metrics_slot_t *s = m_find(m, key);

    if (s != NULL) {
        s->val++;
    }
}

void et_metrics_add_n(et_metrics_t *m, const char *key, uint32_t n)
{
    et_metrics_slot_t *s = m_find(m, key);

    if (s != NULL) {
        s->val += n;
    }
}

void et_metrics_set(et_metrics_t *m, const char *key, uint32_t v)
{
    et_metrics_slot_t *s = m_find(m, key);

    if (s != NULL) {
        s->val = v;
    }
}

bool et_metrics_observe(et_metrics_t *m, const char *key, uint32_t v)
{
    et_metrics_slot_t *s = m_find(m, key);

    if (s == NULL) {
        return false;
    }
    s->val = v;
#if ET_MODULE_HIST
    if (s->hist != NULL) {
        et_hist_push((et_hist_t *)s->hist, (int32_t)v);
    }
#endif
    return true;
}

bool et_metrics_get(const et_metrics_t *m, const char *key, uint32_t *out)
{
    et_metrics_slot_t *s = m_find(m, key);

    if (s == NULL) {
        if (out != NULL) {
            *out = 0u;                /* 未登记如实报 0 */
        }
        return false;
    }
    if (out != NULL) {
        *out = s->val;
    }
    return true;
}

uint32_t et_metrics_count(const et_metrics_t *m)
{
    return (m != NULL) ? m->count : 0u;
}

bool et_metrics_iter(const et_metrics_t *m, uint32_t idx,
                     const char **key, uint32_t *val, uint8_t *kind)
{
    uint32_t i;
    uint32_t seen = 0u;

    if ((m == NULL) || (m->slots == NULL)) {
        return false;
    }
    for (i = 0u; i < m->cap; i++) {
        if (m->slots[i].used != 0u) {
            if (seen == idx) {
                if (key != NULL) {
                    *key = m->slots[i].key;
                }
                if (val != NULL) {
                    *val = m->slots[i].val;
                }
                if (kind != NULL) {
                    *kind = m->slots[i].kind;
                }
                return true;
            }
            seen++;
        }
    }
    return false;
}

void et_metrics_reset(et_metrics_t *m)
{
    uint32_t i;

    if ((m == NULL) || (m->slots == NULL)) {
        return;
    }
    for (i = 0u; i < m->cap; i++) {
        if (m->slots[i].used != 0u) {
            m->slots[i].val = 0u;
        }
    }
    m->dropped = 0u;
}

uint32_t et_metrics_format(const et_metrics_t *m, char *buf, uint32_t buf_cap)
{
    m_sink_t s;
    uint32_t i;

    if ((buf == NULL) || (buf_cap == 0u)) {
        return 0u;
    }
    s.buf = buf;
    s.cap = buf_cap;
    s.pos = 0u;

    m_str(&s, "METRICS ");
    m_u32(&s, (m != NULL) ? m->count : 0u);
    m_ch(&s, '/');
    m_u32(&s, (m != NULL) ? m->cap : 0u);
    m_str(&s, " drop=");
    m_u32(&s, (m != NULL) ? m->dropped : 0u);
    m_ch(&s, '\n');

    if (m != NULL) {
        for (i = 0u; i < m->cap; i++) {
            if (m->slots[i].used != 0u) {
                m_str(&s, "  ");
                m_str(&s, m->slots[i].key);
                m_ch(&s, '=');
                m_u32(&s, m->slots[i].val);
                m_ch(&s, ' ');
                m_ch(&s, (m->slots[i].kind == ET_METRIC_GAUGE) ? 'g' : 'c');
                m_ch(&s, '\n');
            }
        }
    }
    m_finish(&s);
    return s.pos;
}

/* ---------------- 裁剪/特性自描述 (REQ-8) ---------------- */
/* 表在**编译期**构建: 每行的值取对应 ET_MODULE_* 宏并经三目折叠成 0/1 常量,
 * 未启用模块如实入表报 0(不是省略行)。新增开关必须在此登记一行 ——
 * tools/docsync.sh 断言"本表行数 == et_config.h 开关数", 漏登记即红
 * (REQ-8 原文的 34/31 数字失实即栽在此对上)。 */
#define ET_FEAT(UPPER)  { #UPPER, (uint8_t)(ET_MODULE_##UPPER ? 1u : 0u) },

typedef struct {
    const char *name;
    uint8_t     on;
} et_feat_entry_t;

static const et_feat_entry_t g_features[] = {
    ET_FEAT(RINGBUF)
    ET_FEAT(QUEUE)
    ET_FEAT(MEMPOOL)
    ET_FEAT(LIST)
    ET_FEAT(MAP)
    ET_FEAT(SMAP)
    ET_FEAT(FILTER)
    ET_FEAT(PID)
    ET_FEAT(STATS)
    ET_FEAT(MEDFILT)
    ET_FEAT(HIST)
    ET_FEAT(FSM)
    ET_FEAT(STIMER)
    ET_FEAT(SCHED)
    ET_FEAT(WDT)
    ET_FEAT(EVENT)
    ET_FEAT(CRC)
    ET_FEAT(BYTES)
    ET_FEAT(MODBUS)
    ET_FEAT(FRAME)
    ET_FEAT(ATCMD)
    ET_FEAT(XMODEM)
    ET_FEAT(KEY)
    ET_FEAT(LED)
    ET_FEAT(SPWM)
    ET_FEAT(KV)
    ET_FEAT(BOOTCTL)
    ET_FEAT(SOFTCLOCK)
    ET_FEAT(SHELL)
    ET_FEAT(SELFTEST)
    ET_FEAT(LOG)
    ET_FEAT(METRICS)
};

#define ET_FEATURES_N ((uint32_t)(sizeof(g_features) / sizeof(g_features[0])))

uint32_t et_features_format(char *buf, uint32_t buf_cap)
{
    m_sink_t s;
    uint32_t i;

    if ((buf == NULL) || (buf_cap == 0u)) {
        return 0u;
    }
    s.buf = buf;
    s.cap = buf_cap;
    s.pos = 0u;

    m_str(&s, "FEATURES v");
    m_u32(&s, ET_VERSION_MAJOR);
    m_ch(&s, '.');
    m_u32(&s, ET_VERSION_MINOR);
    m_ch(&s, '.');
    m_u32(&s, ET_VERSION_PATCH);
    m_str(&s, " (maj=");
    m_u32(&s, ET_VERSION_MAJOR);
    m_str(&s, " min=");
    m_u32(&s, ET_VERSION_MINOR);
    m_str(&s, " pat=");
    m_u32(&s, ET_VERSION_PATCH);
    m_str(&s, ") sw=");
    m_u32(&s, ET_FEATURES_N);
    m_ch(&s, '\n');

    for (i = 0u; i < ET_FEATURES_N; i++) {
        m_str(&s, "  ");
        m_str(&s, g_features[i].name);
        m_str(&s, "=");
        m_ch(&s, (g_features[i].on != 0u) ? '1' : '0');
        m_ch(&s, '\n');
    }
    m_finish(&s);
    return s.pos;
}

/* ---------------- 命令面 (库不持有命令表; 沿 et_shell_help_cmd 先例) ---------------- */

static const et_metrics_t *g_dump_m;                /* dump 命令绑定的实例(命令面单例) */
static char               g_io_buf[ET_METRICS_IO_MAX]; /* 两命令共用; 主循环内串行执行 */

/* 把"敲 AT+METRICS 要看哪一份注册表"告知库(🏠MAIN)。
 * 多实例场景 = 调用方自备包装函数直调 et_metrics_format(形态见
 * docs/API_GUIDE.md §11.15), 本绑定只服务命令面这一条读数路径。 */
void et_metrics_bind(et_metrics_t *m)
{
    g_dump_m = m;
}

/* 输出一段渲染结果: 优先经 shell(user = et_shell_t*, 与 {"HELP",
 * et_shell_help_cmd} 同款 user 约定), 无 shell 时经日志面, 两者皆关则静默 */
static void m_emit(const char *text, void *user)
{
    bool done = false;

#if ET_MODULE_SHELL
    if (user != NULL) {
        et_shell_puts((et_shell_t *)user, text);
        done = true;
    }
#else
    (void)user;
#endif
#if ET_MODULE_LOG
    if (!done) {
        et_log_raw("%s", text);
    }
#else
    (void)text;
    (void)done;
#endif
}

void et_metrics_dump_cmd(char *args, void *user)
{
    (void)args;

    if (g_dump_m == NULL) {
        m_emit("METRICS unbound (et_metrics_bind 未调用)\n", user);
        return;
    }
    (void)et_metrics_format(g_dump_m, g_io_buf, (uint32_t)sizeof(g_io_buf));
    m_emit(g_io_buf, user);
}

void et_features_cmd(char *args, void *user)
{
    (void)args;

    (void)et_features_format(g_io_buf, (uint32_t)sizeof(g_io_buf));
    m_emit(g_io_buf, user);
}

#endif /* ET_MODULE_METRICS */

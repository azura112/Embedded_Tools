/**
 * @file    test_metrics.c
 * @brief   et_metrics 单元测试 (v2.27, REQ-6/REQ-8)
 *
 * 覆盖面: 定容登记表(登记/拒绝/满表/重名/超长键)、两类指标读写语义、
 * 迭代顺序、reset 口径、format 唯一渲染源的**逐字**断言与截断口径、
 * hist 挂接入桶、以及 features 表的版本三件 + 32 开关逐行 0/1。
 */
#include "et_test.h"
#include <string.h>
#include "et_metrics.h"
#include "et_hist.h"
#include "et_atcmd.h"
#include "et_shell.h"

#define MT_SLOTS   4u

static et_metrics_slot_t g_slots[MT_SLOTS];
static et_metrics_t      g_m;

/* ---- 测试内小工具(零 libc 依赖, 与被测实现的比对口径一致) ---- */
static uint32_t mt_len(const char *s)
{
    uint32_t n = 0u;

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static bool mt_has_prefix(const char *s, const char *pre)
{
    uint32_t i;

    for (i = 0u; i < mt_len(pre); i++) {
        if (s[i] != pre[i]) {
            return false;
        }
    }
    return true;
}

static bool mt_eq(const char *a, const char *b)
{
    uint32_t i;

    for (i = 0u; ; i++) {
        if (a[i] != b[i]) {
            return false;
        }
        if (a[i] == '\0') {
            return true;
        }
    }
}

static void setup(void)
{
    ET_CHECK(et_metrics_init(&g_m, g_slots, MT_SLOTS));
}

static void mt_init_rejects_bad(void)
{
    et_metrics_slot_t slots[1];
    et_metrics_t      m;

    ET_CHECK(!et_metrics_init(NULL, slots, 1u));
    ET_CHECK(!et_metrics_init(&m, NULL, 1u));
    ET_CHECK(!et_metrics_init(&m, slots, 0u));
    ET_CHECK(et_metrics_init(&m, slots, 1u));
    ET_CHECK_U32_EQ(0u, m.count);
    ET_CHECK_U32_EQ(0u, m.dropped);
}

static void mt_register_and_get_basics(void)
{
    uint32_t v = 0xDEADu;

    setup();
    ET_CHECK(et_metrics_register_counter(&g_m, "rx_ok"));
    ET_CHECK(et_metrics_register_gauge(&g_m, "loop_ms"));
    ET_CHECK_U32_EQ(2u, et_metrics_count(&g_m));

    /* 新登记项值起算 0 */
    ET_CHECK(et_metrics_get(&g_m, "rx_ok", &v));
    ET_CHECK_U32_EQ(0u, v);
    /* 未登记: 如实报 0 且返回 false */
    v = 7u;
    ET_CHECK(!et_metrics_get(&g_m, "absent", &v));
    ET_CHECK_U32_EQ(0u, v);
    /* out 可为 NULL(仅测存在性) */
    ET_CHECK(et_metrics_get(&g_m, "loop_ms", NULL));
}

static void mt_register_rejects_bad_keys(void)
{
    setup();
    ET_CHECK(et_metrics_register_counter(&g_m, "a"));

    ET_CHECK(!et_metrics_register_counter(&g_m, "a"));    /* 重名 */
    ET_CHECK(!et_metrics_register_counter(&g_m, ""));     /* 空键 */
    ET_CHECK(!et_metrics_register_counter(&g_m, NULL));   /* NULL 键 */
    ET_CHECK(!et_metrics_register_counter(&g_m,
                 "01234567890123456"));                   /* 17 > KEY_MAX=16 */
    ET_CHECK_U32_EQ(4u, g_m.dropped);
    ET_CHECK_U32_EQ(1u, et_metrics_count(&g_m));
    ET_CHECK(et_metrics_get(&g_m, "a", NULL));             /* 拒绝路径不动既有项 */
    /* 恰好 16 字符合法 */
    ET_CHECK(et_metrics_register_counter(&g_m, "0123456789012345"));
}

static void mt_full_table_rejects_without_overwrite(void)
{
    uint32_t v;

    setup();
    ET_CHECK(et_metrics_register_counter(&g_m, "k0"));
    ET_CHECK(et_metrics_register_gauge(&g_m, "k1"));
    ET_CHECK(et_metrics_register_counter(&g_m, "k2"));
    ET_CHECK(et_metrics_register_gauge(&g_m, "k3"));
    ET_CHECK(!et_metrics_register_counter(&g_m, "k4"));    /* 满表 */
    ET_CHECK_U32_EQ(MT_SLOTS, et_metrics_count(&g_m));
    ET_CHECK_U32_EQ(1u, g_m.dropped);

    et_metrics_set(&g_m, "k1", 55u);
    ET_CHECK(et_metrics_get(&g_m, "k1", &v));
    ET_CHECK_U32_EQ(55u, v);
    ET_CHECK(!et_metrics_get(&g_m, "k4", &v));             /* 被拒项不存在 */
}

static void mt_inc_add_set_semantics(void)
{
    uint32_t v;

    setup();
    ET_CHECK(et_metrics_register_counter(&g_m, "hits"));
    ET_CHECK(et_metrics_register_gauge(&g_m, "level"));

    et_metrics_inc(&g_m, "hits");
    et_metrics_inc(&g_m, "hits");
    et_metrics_add_n(&g_m, "hits", 8u);
    ET_CHECK(et_metrics_get(&g_m, "hits", &v));
    ET_CHECK_U32_EQ(10u, v);

    et_metrics_set(&g_m, "level", 4242u);
    ET_CHECK(et_metrics_get(&g_m, "level", &v));
    ET_CHECK_U32_EQ(4242u, v);

    /* 未登记键: 写操作静默(不建项、不计 dropped 之外的副作用) */
    et_metrics_inc(&g_m, "ghost");
    et_metrics_add_n(&g_m, "ghost", 5u);
    et_metrics_set(&g_m, "ghost", 5u);
    ET_CHECK_U32_EQ(MT_SLOTS, g_m.cap);
    ET_CHECK_U32_EQ(2u, et_metrics_count(&g_m));
}

static void mt_observe_gauge_and_hist(void)
{
    static uint32_t bins[4];
    et_hist_t       h;
    uint32_t        v;

    setup();
    ET_CHECK(et_metrics_register_gauge(&g_m, "dur"));
    /* 未挂接: observe 只置值 */
    ET_CHECK(et_metrics_observe(&g_m, "dur", 12u));
    ET_CHECK(et_metrics_get(&g_m, "dur", &v));
    ET_CHECK_U32_EQ(12u, v);

    /* 挂接直方图: observe 同时入桶([0,3] 四桶 → 桶宽 1, 值 2 落第 2 桶) */
    ET_CHECK(et_hist_init(&h, bins, 4u, 0, 3));
    ET_CHECK(et_metrics_link_hist(&g_m, "dur", &h));
    ET_CHECK(et_metrics_observe(&g_m, "dur", 2u));
    ET_CHECK(et_metrics_get(&g_m, "dur", &v));
    ET_CHECK_U32_EQ(2u, v);
    ET_CHECK_U32_EQ(1u, et_hist_count(&h));
    ET_CHECK_U32_EQ(1u, et_hist_bin(&h, 2u));

    /* 键不存在: link/observe 均 false */
    ET_CHECK(!et_metrics_link_hist(&g_m, "ghost", &h));
    ET_CHECK(!et_metrics_observe(&g_m, "ghost", 1u));
    /* 解绑后不再入桶 */
    ET_CHECK(et_metrics_link_hist(&g_m, "dur", NULL));
    ET_CHECK(et_metrics_observe(&g_m, "dur", 3u));
    ET_CHECK_U32_EQ(1u, et_hist_count(&h));
}

static void mt_iter_follows_register_order(void)
{
    const char *key;
    uint32_t    v;
    uint8_t     kind;

    setup();
    ET_CHECK(et_metrics_register_gauge(&g_m, "g1"));
    ET_CHECK(et_metrics_register_counter(&g_m, "c1"));
    et_metrics_set(&g_m, "g1", 9u);
    et_metrics_inc(&g_m, "c1");

    ET_CHECK(et_metrics_iter(&g_m, 0u, &key, &v, &kind));
    ET_CHECK(key[0] == 'g' && key[1] == '1' && key[2] == '\0');
    ET_CHECK_U32_EQ(9u, v);
    ET_CHECK_U32_EQ(ET_METRIC_GAUGE, kind);

    ET_CHECK(et_metrics_iter(&g_m, 1u, &key, &v, &kind));
    ET_CHECK(key[0] == 'c' && key[1] == '1' && key[2] == '\0');
    ET_CHECK_U32_EQ(1u, v);
    ET_CHECK_U32_EQ(ET_METRIC_COUNTER, kind);

    ET_CHECK(!et_metrics_iter(&g_m, 2u, &key, &v, &kind));
    /* 输出指针全部可为 NULL */
    ET_CHECK(et_metrics_iter(&g_m, 0u, NULL, NULL, NULL));
    ET_CHECK(!et_metrics_iter(NULL, 0u, &key, &v, &kind));
}

static void mt_reset_keeps_registrations(void)
{
    uint32_t v;

    setup();
    ET_CHECK(et_metrics_register_counter(&g_m, "a"));
    et_metrics_inc(&g_m, "a");
    ET_CHECK(!et_metrics_register_counter(&g_m, "a"));   /* 制造 dropped=1 */
    ET_CHECK_U32_EQ(1u, g_m.dropped);

    et_metrics_reset(&g_m);
    ET_CHECK(et_metrics_get(&g_m, "a", &v));             /* 登记仍在 */
    ET_CHECK_U32_EQ(0u, v);
    ET_CHECK_U32_EQ(1u, et_metrics_count(&g_m));
    ET_CHECK_U32_EQ(0u, g_m.dropped);
    et_metrics_reset(NULL);                              /* NULL 安全 */
}

static void mt_format_verbatim(void)
{
    char buf[128];

    setup();
    ET_CHECK(et_metrics_register_counter(&g_m, "rx"));
    ET_CHECK(et_metrics_register_gauge(&g_m, "ms"));
    et_metrics_add_n(&g_m, "rx", 1024u);
    et_metrics_set(&g_m, "ms", 7u);

    /* 逐字断言(唯一渲染源的既定输出形态): 表头 + 按登记顺序两空格缩进行,
     * 行尾类别字符 'c'/'g', 全 LF 结尾无空行 */
    ET_CHECK_U32_EQ(40u, et_metrics_format(&g_m, buf, sizeof(buf)));
    ET_CHECK(mt_eq(buf, "METRICS 2/4 drop=0\n  rx=1024 c\n  ms=7 g\n"));

    /* 空表: 只有表头 */
    {
        et_metrics_t        m2;
        et_metrics_slot_t   s2[2];

        ET_CHECK(et_metrics_init(&m2, s2, 2u));
        ET_CHECK_U32_EQ(19u, et_metrics_format(&m2, buf, sizeof(buf)));
        ET_CHECK(mt_eq(buf, "METRICS 0/2 drop=0\n"));
    }
}

static void mt_format_truncation(void)
{
    char buf[16];
    char big[128];

    setup();
    ET_CHECK(et_metrics_register_counter(&g_m, "abcdefgh"));
    et_metrics_inc(&g_m, "abcdefgh");

    /* 全量渲染长度作为对照 */
    (void)et_metrics_format(&g_m, big, sizeof(big));

    /* 截断: 返回值 = 实际写入 = buf_cap-1, 且始终 NUL 结尾、前缀与全量一致 */
    ET_CHECK_U32_EQ(15u, et_metrics_format(&g_m, buf, sizeof(buf)));
    ET_CHECK(buf[15] == '\0');
    {
        uint32_t i;
        for (i = 0u; i < 15u; i++) {
            ET_CHECK(buf[i] == (char)big[i]);
        }
    }
    /* 0 容量与非空表: 返回 0 且不写 */
    ET_CHECK_U32_EQ(0u, et_metrics_format(&g_m, buf, 0u));
    /* NULL 缓冲: 返回 0 */
    ET_CHECK_U32_EQ(0u, et_metrics_format(&g_m, NULL, sizeof(buf)));
    /* NULL 句柄: 表头照出, 计数与容量均报 0(用大缓冲避免截断) */
    ET_CHECK_U32_EQ(19u, et_metrics_format(NULL, big, sizeof(big)));
    ET_CHECK(mt_eq(big, "METRICS 0/0 drop=0\n"));
}

static void mt_features_header_and_rows(void)
{
    char     buf[1024];
    char     want[96];
    uint32_t len;
    uint32_t lines = 0u;
    uint32_t i;

    len = et_features_format(buf, sizeof(buf));
    ET_CHECK(len > 0u);
    ET_CHECK(len < sizeof(buf));
    ET_CHECK(buf[len] == '\0');
    for (i = 0u; i < len; i++) {
        if (buf[i] == '\n') {
            lines++;
        }
    }
    /* 32 个开关逐行(行计数口径) + 1 行表头 */
    ET_CHECK_U32_EQ(33u, lines);

    /* 表头逐字: 版本三件以 maj/min/pat 显式在档 + 版本串 + 开关数 */
    (void)snprintf(want, sizeof(want),
                   "FEATURES v%s (maj=%d min=%d pat=%d) sw=32\n",
                   ET_VERSION_STRING, ET_VERSION_MAJOR, ET_VERSION_MINOR,
                   ET_VERSION_PATCH);
    ET_CHECK(mt_has_prefix(buf, want));
}

/* 在渲染结果中找 "  NAME=<c>" 行的值字符(未找到返回 '?') */
static char mt_feat_at(const char *buf, const char *name)
{
    const char *p = buf;
    uint32_t    nlen = mt_len(name);

    while (*p != '\0') {
        if ((p[0] == ' ') && (p[1] == ' ')
            && mt_has_prefix(p + 2, name)
            && (p[2 + nlen] == '=')
            && ((p[3 + nlen] == '0') || (p[3 + nlen] == '1'))
            && (p[4 + nlen] == '\n')) {
            return p[3 + nlen];
        }
        p++;
    }
    return '?';
}

static void mt_features_table_tracks_macros(void)
{
    char buf[1024];

    (void)et_features_format(buf, sizeof(buf));

    /* 表值断言(AC-5 载体): 每行的 0/1 **取自对应 ET_MODULE_* 宏**而非硬编码
     * 1 —— 断言右侧按同一宏求值, 故本构建下 SELFTEST 为 1(Makefile 传
     * -DET_MODULE_SELFTEST=1)时该行必须报 1, 反之报 0; 用 -DET_MODULE_STATS=0
     * 的变体构建复跑本用例即得"未启用如实报 0"的红绿对(交付 §2 AC-5 留痕)。 */
    ET_CHECK_U32_EQ((uint32_t)(ET_MODULE_SELFTEST ? '1' : '0'),
                    (uint32_t)mt_feat_at(buf, "SELFTEST"));
    ET_CHECK_U32_EQ((uint32_t)(ET_MODULE_STATS ? '1' : '0'),
                    (uint32_t)mt_feat_at(buf, "STATS"));
    ET_CHECK_U32_EQ((uint32_t)(ET_MODULE_METRICS ? '1' : '0'),
                    (uint32_t)mt_feat_at(buf, "METRICS"));
    ET_CHECK_U32_EQ((uint32_t)(ET_MODULE_SCHED ? '1' : '0'),
                    (uint32_t)mt_feat_at(buf, "SCHED"));
    /* 表内没有的名字 → '?'(证明断言不是"任意字符都算过") */
    ET_CHECK(mt_feat_at(buf, "NO_SUCH_MODULE") == '?');
}

static void mt_features_truncation(void)
{
    char buf[32];

    ET_CHECK_U32_EQ(31u, et_features_format(buf, sizeof(buf)));
    ET_CHECK(buf[31] == '\0');
    ET_CHECK_U32_EQ(0u, et_features_format(buf, 0u));
    ET_CHECK_U32_EQ(0u, et_features_format(NULL, sizeof(buf)));
}

static void mt_count_and_null_safety(void)
{
    uint32_t v;

    setup();
    ET_CHECK_U32_EQ(0u, et_metrics_count(NULL));
    ET_CHECK(!et_metrics_register_counter(NULL, "a"));
    ET_CHECK(!et_metrics_register_gauge(&g_m, NULL));
    ET_CHECK(!et_metrics_get(NULL, "a", &v));
    ET_CHECK(!et_metrics_observe(NULL, "a", 1u));
    et_metrics_inc(NULL, "a");
    et_metrics_add_n(NULL, "a", 1u);
    et_metrics_set(NULL, "a", 1u);
    ET_CHECK_U32_EQ(0u, et_metrics_count(&g_m));
}

/* ---- 命令面执行路径: 两 handler 经 atcmd + shell 真跑(注册形态 = {"NAME", fn, help}) ---- */
static char              g_out[2048];
static uint32_t          g_out_len;
static et_atcmd_proc_t   g_at;
static et_shell_t        g_sh;
static char              g_line[32];
static et_metrics_slot_t g_cslots[4];
static et_metrics_t      g_cm;

static void cap_putc(void *user, char ch)
{
    (void)user;
    if (g_out_len + 1u < sizeof(g_out)) {
        g_out[g_out_len] = ch;
        g_out_len++;
        g_out[g_out_len] = '\0';
    }
}

static void cmd_noop(char *args, void *user)
{
    (void)args;
    (void)user;
}

static const et_atcmd_entry_t g_cmds[] = {
    { "METRICS",  et_metrics_dump_cmd, "dump named metrics" },
    { "FEATURES", et_features_cmd,     "modules self-describe" },
    { "NOOP",     cmd_noop,            NULL },
};

static void cmd_fresh(void)
{
    g_out_len = 0u;
    g_out[0]  = '\0';
    ET_CHECK(et_metrics_init(&g_cm, g_cslots, 4u));
    (void)et_metrics_register_counter(&g_cm, "rx");
    (void)et_metrics_register_gauge(&g_cm, "ms");
    et_metrics_inc(&g_cm, "rx");
    et_metrics_set(&g_cm, "ms", 9u);
    et_metrics_bind(&g_cm);

    (void)memset(&g_at, 0, sizeof(g_at));
    (void)memset(&g_sh, 0, sizeof(g_sh));
    ET_CHECK(et_atcmd_init(&g_at, g_cmds, 3u, g_line, sizeof(g_line), &g_sh));
    ET_CHECK(et_shell_init(&g_sh, &g_at, cap_putc, NULL));
    et_shell_set_echo(&g_sh, false);        /* 回显关掉, 捕获面只剩命令输出 */
}

static void cmd_feed(const char *s)
{
    while (*s != '\0') {
        (void)et_shell_feed(&g_sh, *s);
        s++;
    }
}

static bool mt_contains(const char *hay, const char *needle)
{
    uint32_t i;

    for (i = 0u; hay[i] != '\0'; i++) {
        if (mt_has_prefix(&hay[i], needle)) {
            return true;
        }
    }
    return false;
}

static void mt_dump_cmd_through_shell(void)
{
    /* 换绑定实例的句柄与槽表: 生命周期须覆盖绑定使用期, 故声明在函数作用域
       (v2.27 发版后 CI 补丁: 原置于内层块作用域, 块外经 AT+METRICS 读到越域栈对象
        —— Linux ASAN 判 stack-use-after-scope, MinGW 无 ASAN 故本地静默通过) */
    et_metrics_slot_t other[2];
    et_metrics_t      m2;

    cmd_fresh();
    cmd_feed("AT+METRICS\r");
    ET_CHECK(mt_contains(g_out, "METRICS 2/4 drop=0"));
    ET_CHECK(mt_contains(g_out, "  rx=1 c"));
    ET_CHECK(mt_contains(g_out, "  ms=9 g"));

    ET_CHECK(et_metrics_init(&m2, other, 2u));
    (void)et_metrics_register_counter(&m2, "only");
    et_metrics_bind(&m2);

    g_out_len = 0u;
    g_out[0]  = '\0';
    cmd_feed("AT+METRICS\r");
    ET_CHECK(mt_contains(g_out, "METRICS 1/2 drop=0"));
    ET_CHECK(!mt_contains(g_out, "rx=1"));

    /* 未绑定: 如实报 unbound, 不静默(句柄出作用域前解绑, 与头文件生命周期契约一致) */
    et_metrics_bind(NULL);
    g_out_len = 0u;
    g_out[0]  = '\0';
    cmd_feed("AT+METRICS\r");
    ET_CHECK(mt_contains(g_out, "METRICS unbound"));
}

static void mt_features_cmd_through_shell(void)
{
    cmd_fresh();
    cmd_feed("AT+FEATURES\r");
    ET_CHECK(mt_contains(g_out, "FEATURES v"));
    ET_CHECK(mt_contains(g_out, ") sw=32\n"));
    ET_CHECK(mt_contains(g_out, "  SCHED=1\n"));
    ET_CHECK(mt_contains(g_out, "  METRICS=1\n"));
}

const et_test_case_t *test_metrics_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"metrics.init_rejects_bad",            mt_init_rejects_bad},
        {"metrics.register_and_get_basics",     mt_register_and_get_basics},
        {"metrics.register_rejects_bad_keys",   mt_register_rejects_bad_keys},
        {"metrics.full_table_rejects",          mt_full_table_rejects_without_overwrite},
        {"metrics.inc_add_set_semantics",       mt_inc_add_set_semantics},
        {"metrics.observe_gauge_and_hist",      mt_observe_gauge_and_hist},
        {"metrics.iter_register_order",         mt_iter_follows_register_order},
        {"metrics.reset_keeps_registrations",   mt_reset_keeps_registrations},
        {"metrics.format_verbatim",             mt_format_verbatim},
        {"metrics.format_truncation",           mt_format_truncation},
        {"metrics.features_header_and_rows",    mt_features_header_and_rows},
        {"metrics.features_table_tracks_macros",mt_features_table_tracks_macros},
        {"metrics.features_truncation",         mt_features_truncation},
        {"metrics.null_safety",                 mt_count_and_null_safety},
        {"metrics.dump_cmd_through_shell",      mt_dump_cmd_through_shell},
        {"metrics.features_cmd_through_shell",  mt_features_cmd_through_shell},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

/**
 * @file    test_log.c
 * @brief   et_log 单元测试 (捕获 port_putc 输出做精确断言)
 */
#include "et_test.h"
#include "et_log.h"
#include "port_host.h"
#include <string.h>
#include <stdbool.h>

static char     g_buf[1024];
static uint32_t g_len;

static void cap_start(void)
{
    port_host_tick_advance(1u);                 /* 时间戳随之变化 */
    port_host_capture_start(g_buf, sizeof(g_buf));
}

static void cap_stop(void)
{
    g_len = port_host_capture_stop();
}

/* 输出中是否包含子串 */
static bool has(const char *s)
{
    return strstr(g_buf, s) != NULL;
}

static void log_level_filter(void)
{
    et_log_set_level(ET_LOG_LEVEL_WARN);

    cap_start();
    ET_LOGI("t", "info should be filtered");
    cap_stop();
    ET_CHECK_U32_EQ(0u, g_len);                 /* 低于过滤线: 无输出 */

    cap_start();
    ET_CHECK(ET_LOGW("t", "warn here") > 0);
    cap_stop();
    ET_CHECK(has("[W][t] warn here"));

    et_log_set_level(ET_LOG_LEVEL_ERROR);
    cap_start();
    ET_LOGW("t", "now filtered");
    cap_stop();
    ET_CHECK_U32_EQ(0u, g_len);

    /* 运行期级别读写回环与边界收敛 */
    et_log_set_level(ET_LOG_LEVEL_DEBUG);
    ET_CHECK(et_log_get_level() == ET_LOG_LEVEL_DEBUG);
    et_log_set_level((et_log_level_t)99);
    ET_CHECK(et_log_get_level() == ET_LOG_LEVEL_NONE);
    et_log_set_level(ET_LOG_LEVEL_INFO);
}

static void log_prefix_shape(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);

    cap_start();
    ET_LOGE("uart", "hello %d", 7);
    cap_stop();

    ET_CHECK(g_len > 0u);
    ET_CHECK(g_buf[0] == '[');
    ET_CHECK(has("][E][uart] hello 7\n"));      /* 级别/标签/正文结构 */
}

static void log_int_formats(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);

    port_host_tick_set(20000u);                 /* 固定时间戳便于全文比对 */
    cap_start();
    ET_LOGD("f", "[%d] [%u] [%x] [%X]", -7, 42u, 255u, 3735928559u);
    cap_stop();
    ET_CHECK(strcmp(g_buf,
                    "[20001][D][f] [-7] [42] [ff] [DEADBEEF]\n") == 0);

    port_host_tick_set(20002u);
    cap_start();
    ET_LOGD("f", "%ld %lx", 123456789L, 0xBEEFL);
    cap_stop();
    ET_CHECK(strcmp(g_buf, "[20003][D][f] 123456789 beef\n") == 0);
}

static void log_str_char_percent(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);

    port_host_tick_set(21000u);
    cap_start();
    ET_LOGT("s", "%s|%c|%c|%%", "ab", 'Z', '%');
    cap_stop();
    ET_CHECK(strcmp(g_buf, "[21001][T][s] ab|Z|%|%\n") == 0);

    port_host_tick_set(21002u);
    cap_start();
    ET_LOGT("s", "%s", NULL);                   /* NULL 字符串安全 */
    cap_stop();
    ET_CHECK(strcmp(g_buf, "[21003][T][s] (null)\n") == 0);
}

static void log_longlong(void)
{
#if ET_LOG_MAX_LEVEL <= ET_LOG_LEVEL_TRACE
    et_log_set_level(ET_LOG_LEVEL_TRACE);

    port_host_tick_set(22000u);
    cap_start();
    ET_LOGT("n", "%llu", 5000000000ULL);
    cap_stop();
    ET_CHECK(strcmp(g_buf, "[22001][T][n] 5000000000\n") == 0);
#else
    ET_CHECK(1);                                /* 已被编译裁剪 */
#endif
}

static void log_hexdump_layout(void)
{
    uint8_t data[17];
    uint8_t i;

    for (i = 0u; i < 17u; i++) {
        data[i] = i;
    }
    et_log_set_level(ET_LOG_LEVEL_TRACE);

    cap_start();
    et_log_hexdump(ET_LOG_LEVEL_TRACE, "rx", data, 17u);
    cap_stop();

    ET_CHECK(has("0000: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"));
    ET_CHECK(has("|................|"));
    ET_CHECK(has("0010: 10 "));
    ET_CHECK(has("|.|"));                       /* 第 17 字节 ASCII 列 */
    ET_CHECK(has("][H][rx] "));
}

static void log_raw_passthrough(void)
{
    et_log_set_level(ET_LOG_LEVEL_NONE);        /* 过滤线不影响 raw */

    cap_start();
    ET_CHECK(et_log_raw("x=%d;", 5) == 4);
    cap_stop();
    ET_CHECK(strcmp(g_buf, "x=5;") == 0);
}

/* =====================================================================
 * v2.5 规格解析加固用例 (P1-3)
 *
 * 手法: **每条规格用例后随哨兵实参**(SENT), 断言哨兵值正确渲染 —— 这是
 * "实参错位"的检出关键(v2.4 的 `%02x` 缺陷正是靠它暴露: 后续实参整体
 * 前移一格)。受支持规格额外与 host `snprintf` **逐字节对拍**; 不支持/未知
 * 规格不做对拍(那在 C 里是 UB 或会被 host 格式化), 改与固定期望字面量比对。
 * ===================================================================== */

#define SENT        0x5A5A5A5Au                 /* 1515870810 */
#define SENT_TAIL   " S=1515870810"

static int  g_fail;
static char g_spec[256];                        /* 运行时拼接哨兵规格的格式串 */

/* 受支持规格: 与 host snprintf 逐字节对拍 + 哨兵断言 */
#define CHECK_FMT(name, fmt, ...)                                       \
    do {                                                                \
        char  ref_[1024];                                               \
        size_t ln_;                                                     \
        (void)snprintf(g_spec, sizeof(g_spec), "%s S=%%u", (fmt));      \
        cap_start();                                                    \
        (void)et_log_raw(g_spec, __VA_ARGS__, (unsigned)SENT);          \
        cap_stop();                                                     \
        (void)snprintf(ref_, sizeof(ref_), g_spec, __VA_ARGS__,         \
                       (unsigned)SENT);                                 \
        ln_ = strlen(g_buf);                                            \
        if ((strcmp(g_buf, ref_) != 0) || (ln_ < sizeof(SENT_TAIL) - 1u) || \
            (strcmp(g_buf + ln_ - (sizeof(SENT_TAIL) - 1u), SENT_TAIL) != 0)) { \
            printf("  FAIL %s\n    lib: [%s]\n    ref: [%s]\n",          \
                   (name), g_buf, ref_);                                \
            g_fail++;                                                   \
        }                                                               \
    } while (0)

/* 不支持/未知规格: 与固定期望字面量比对(不做 snprintf 对拍) */
#define CHECK_LIT(name, want, fmt, ...)                                 \
    do {                                                                \
        cap_start();                                                    \
        (void)et_log_raw(fmt, __VA_ARGS__);                             \
        cap_stop();                                                     \
        if (strcmp(g_buf, want) != 0) {                                 \
            printf("  FAIL %s\n    lib: [%s]\n    ref: [%s]\n",          \
                   (name), g_buf, want);                                \
            g_fail++;                                                   \
        }                                                               \
    } while (0)

/* ---- 域宽 / 补零 (AC-6) ---- */
static void log_field_width(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("width.dec_zero_pad",  "%04u|%02u", 42u, 7u);
    CHECK_FMT("width.hex_zero_pad",  "%02x|%08X", 17u, 0xDEADBEEFu);
    CHECK_FMT("width.space_pad",     "%5u|%8x", 42u, 0xABu);
    CHECK_FMT("width.string",        "%5s|%8s", "ab", "cd");
    CHECK_FMT("width.zero_hex_wide", "%010x|%06u", 0x1FFu, 42u);
    ET_CHECK(g_fail == 0);
}

/* ---- 左对齐 ---- */
static void log_left_align(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("left.uint",   "%-5u|", 42u);
    CHECK_FMT("left.string", "%-8s|", "abc");
    CHECK_FMT("left.zero_ignored", "%-05u|", 42u);   /* '-' 存在时 '0' 失效(C) */
    CHECK_FMT("left.hex",    "%-6x|", 0xABCu);
}

/* ---- 符号标志 (+ / 空格) ---- */
static void log_sign_flags(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("sign.plus",       "%+d|%+d", 7, -7);
    CHECK_FMT("sign.space",      "% d|% d", 7, -7);
    CHECK_FMT("sign.plus_width", "%+6d|%-+6d|", 7, 7);
    CHECK_FMT("sign.space_width", "% 6d|", 7);
    CHECK_FMT("sign.plus_zero",  "%+06d|", 42);
    CHECK_FMT("sign.unsigned_ignored", "%+u|% u", 7u, 7u);  /* 无符号: 标志无效 */
}

/* ---- '#' 备用形式 ---- */
static void log_alt_prefix(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("alt.hex",       "%#x|%#X", 0xBEEFu, 0xBEEFu);
    CHECK_FMT("alt.hex_zero",  "%#010x|", 255u);
    CHECK_FMT("alt.hex_alt0",  "%#x|", 0u);          /* 值 0: 不加前缀(C) */
    CHECK_FMT("alt.dec_noop",  "%#u|%#d", 42u, 42);  /* 十进制: '#' 无效 */
}

/* ---- '*' 域宽 ---- */
static void log_star_width(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("star.uint",      "%*u|", 6, 42u);
    CHECK_FMT("star.neg_left",  "%*u|", -6, 42u);    /* 负域宽 = 左对齐(C) */
    CHECK_FMT("star.string",    "%*s|", 6, "ab");
    CHECK_FMT("star.zero",      "%0*u|", 6, 42u);
}

/* ---- 精度 (.N) ---- */
static void log_precision_int(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("prec.int_min_digits", "%.3d|%.5u", 5, 42u);
    CHECK_FMT("prec.int_zero_value", "%.0d|", 0);     /* C: 精度0+值0 → 空 */
    CHECK_FMT("prec.int_with_width", "%8.3d|", 5);
    CHECK_FMT("prec.int_zero_flag_off", "%08.3d|", 5); /* 有精度时 '0' 失效(C) */
    CHECK_FMT("prec.hex", "%.4x|", 0xABu);
}

/* ---- 精度 (.Ns / %.*s) ---- */
static void log_precision_str(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("prec.str_trunc",     "%.3s|", "abcdef");
    CHECK_FMT("prec.str_width",     "%8.3s|", "abcdef");
    CHECK_FMT("prec.str_left",      "%-8.3s|", "abcdef");
    CHECK_FMT("prec.str_zero",      "%.0s|", "abc");
    CHECK_FMT("prec.str_longer",    "%.10s|", "abc");
    CHECK_FMT("prec.star_str",      "%.*s|", 3, "ABCDEF");
    CHECK_FMT("prec.star_str_full", "%.*s|", 6, "ABCDEF");
    CHECK_FMT("prec.star_int",      "%.*d|", 4, 7);
}

/* ---- 长度修饰 ---- */
static void log_length_mods(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("len.llu", "%llu|", 5000000000ULL);
    CHECK_FMT("len.lld", "%lld|", -5000000000LL);
    CHECK_FMT("len.llx", "%llx|", 0x123456789ABCDEFULL);
    CHECK_FMT("len.lu",  "%lu|%lx", 123456789UL, 0xBEEFUL);
    CHECK_FMT("len.hu",  "%hu|", 65535u);
    CHECK_FMT("len.hhd", "%hhd|%hhu", -1, 255u);
    CHECK_FMT("len.hhd_trunc", "%hhd|", 300);          /* C: 截断为 signed char */
    CHECK_FMT("len.h_hex", "%hx|", 0x1234u);
}

/* ---- size_t 修饰 (z) ---- */
static void log_size_mod(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("size.zu",   "%zu|", (size_t)123456u);
    CHECK_FMT("size.zu_big", "%zu|", (size_t)4294967296ULL);
    CHECK_FMT("size.zd_neg", "%zd|", (ptrdiff_t)-5);
    CHECK_FMT("size.zu_width", "%06zu|", (size_t)42u);
}

/* ---- 已知不支持: 浮点族 (HC-3 / AC-7) ---- */
static void log_unsupported_float(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_LIT("unsup.f", "<?f>" SENT_TAIL, "%f S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.e", "<?e>" SENT_TAIL, "%e S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.g", "<?g>" SENT_TAIL, "%g S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.E", "<?E>" SENT_TAIL, "%E S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.G", "<?G>" SENT_TAIL, "%G S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.F", "<?F>" SENT_TAIL, "%F S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.a", "<?a>" SENT_TAIL, "%a S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.A", "<?A>" SENT_TAIL, "%A S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.f_width", "<?f>" SENT_TAIL, "%10.3f S=%u", 1.5, (unsigned)SENT);
    CHECK_LIT("unsup.f_two", "<?f>|<?f>" SENT_TAIL, "%f|%f S=%u", 1.0, 2.0, (unsigned)SENT);

    /* 负向: 不得原样回显字面规格 */
    cap_start();
    (void)et_log_raw("%f %e %g", 1.0, 2.0, 3.0);
    cap_stop();
    ET_CHECK(strstr(g_buf, "%f") == NULL);
    ET_CHECK(strstr(g_buf, "%e") == NULL);
    ET_CHECK(strstr(g_buf, "%g") == NULL);
    ET_CHECK(strcmp(g_buf, "<?f> <?e> <?g>") == 0);
    ET_CHECK(g_fail == 0);
}

/* ---- 已知不支持: 八进制 / %n / 宽字符 ---- */
static void log_unsupported_others(void)
{
    int probe  = 0x1234;
    int probe2 = 0x5678;
    static const char wbuf[4] = "AB";

    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_LIT("unsup.o",  "<?o>" SENT_TAIL, "%o S=%u", 255u, (unsigned)SENT);
    CHECK_LIT("unsup.o_alt", "<?o>" SENT_TAIL, "%#o S=%u", 255u, (unsigned)SENT);
    CHECK_LIT("unsup.o_width", "<?o>" SENT_TAIL, "%5o S=%u", 255u, (unsigned)SENT);
    CHECK_LIT("unsup.lo", "<?o>" SENT_TAIL, "%lo S=%u", 255UL, (unsigned)SENT);
    CHECK_LIT("unsup.lc", "<?lc>" SENT_TAIL, "%lc S=%u", (int)'A', (unsigned)SENT);
    CHECK_LIT("unsup.ls", "<?ls>" SENT_TAIL, "%ls S=%u", (const void *)wbuf,
              (unsigned)SENT);

    /* %n: 占位 + 哨兵正确 + **禁写内存**(目标变量哨兵值不变) */
    CHECK_LIT("unsup.n", "<?n>|<?n>" SENT_TAIL, "%n|%n S=%u",
              &probe, &probe2, (unsigned)SENT);
    ET_CHECK(probe == 0x1234);
    ET_CHECK(probe2 == 0x5678);
    CHECK_LIT("unsup.n_len", "<?n>" SENT_TAIL, "%ln S=%u",
              &probe, (unsigned)SENT);
    ET_CHECK(probe == 0x1234);

    /* 负向: 不得原样回显 */
    cap_start();
    (void)et_log_raw("%o %n %lc %ls", 1u, &probe, (int)'x', (const void *)wbuf);
    cap_stop();
    ET_CHECK(strcmp(g_buf, "<?o> <?n> <?lc> <?ls>") == 0);
    ET_CHECK(probe == 0x1234);
    ET_CHECK(g_fail == 0);
}

/* ---- 未知转换字符: 占位且**不消费实参** (HC-3 例外, 头注明示) ---- */
static void log_unknown_conv(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    /* %y 不消费 → 随后的 %u 取到同一实参 7 */
    CHECK_LIT("unknown.y_no_consume", "<?y>|7" SENT_TAIL, "%y|%u S=%u",
              7u, (unsigned)SENT);
    CHECK_LIT("unknown.Q", "<?Q>" SENT_TAIL, "%Q S=%u", (unsigned)SENT);
    CHECK_LIT("unknown.j_mod", "<?j>d" SENT_TAIL, "%jd S=%u", (unsigned)SENT);
    CHECK_LIT("unknown.t_mod", "<?t>u" SENT_TAIL, "%tu S=%u", (unsigned)SENT);
    CHECK_LIT("unknown.L_mod", "<?L>f" SENT_TAIL, "%Lf S=%u", (unsigned)SENT);
    CHECK_LIT("unknown.trailing_pct", "abc%" SENT_TAIL, "abc%% S=%u", (unsigned)SENT);
    ET_CHECK(g_fail == 0);
}

/* 无实参规格(尾部孤 '%' 与 '%%'): 单独手工捕获(宏需至少一个实参) */
static void log_no_arg_specs(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    cap_start();
    (void)et_log_raw("x%");
    cap_stop();
    ET_CHECK(strcmp(g_buf, "x%") == 0);             /* 尾部孤 '%': 原样输出 */

    cap_start();
    (void)et_log_raw("100%%");
    cap_stop();
    ET_CHECK(strcmp(g_buf, "100%") == 0);

    cap_start();
    (void)et_log_raw("%");
    cap_stop();
    ET_CHECK(strcmp(g_buf, "%") == 0);

    cap_start();
    (void)et_log_raw("a%%b%c", 'Z');
    cap_stop();
    ET_CHECK(strcmp(g_buf, "a%bZ") == 0);

    ET_CHECK(g_fail == 0);
}

/* ---- 域宽上限 (AC-8): 超限截断, 不无限刷字符 ---- */
static void log_field_cap(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    cap_start();
    (void)et_log_raw("%9999u", 42u);
    cap_stop();
    ET_CHECK(g_len == ET_LOG_FIELD_MAX);            /* 42 左补空格至上限 */
    ET_CHECK(g_buf[0] == ' ');
    ET_CHECK(strcmp(g_buf + g_len - 2u, "42") == 0);

    cap_start();
    (void)et_log_raw("%*u", 100000, 42u);           /* 实参域宽同样截断 */
    cap_stop();
    ET_CHECK(g_len == ET_LOG_FIELD_MAX);

    cap_start();
    (void)et_log_raw("%.9999d", 1);                 /* 精度同样截断 */
    cap_stop();
    ET_CHECK(g_len == ET_LOG_FIELD_MAX);

    cap_start();
    (void)et_log_raw("%9999s", "ab");               /* 字符串域宽截断 */
    cap_stop();
    ET_CHECK(g_len == ET_LOG_FIELD_MAX);

    cap_start();
    (void)et_log_raw("%200u|%200u", 1u, 2u);        /* 上限内正常 */
    cap_stop();
    ET_CHECK(g_len == 401u);                        /* 200 + 1 + 200 */
    ET_CHECK(g_fail == 0);
}

/* ---- 历史缺陷复现(CO-9 三处受害点): 必须不再错位 ---- */
static void log_misalign_regression(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    /* ① v2.4 板侧: "addr=0x%02x (kv regs %u..%u)" 曾输出
     *    "addr=0x%02x (kv regs 17..1000)" —— 字面回显 + 实参整体前移一格 */
    CHECK_FMT("misalign.hex_pad", "addr=0x%02x (kv regs %u..%u)",
              17u, 1000u, 1003u);
    /* ② "n=%zu tail=%u": 曾输出 "n=%zu tail=7" —— %u 取到前一个实参 */
    CHECK_FMT("misalign.size_t", "n=%zu tail=%u", (size_t)7u, 99u);
    /* ③ posix_demo.c 现网形态: "unknown(%u): %.*s" */
    CHECK_FMT("misalign.star_str", "unknown(%u): %.*s", 3u, 3, "ABC");

    /* 逐字节确认修复后的实际字形(不只看对拍) */
    cap_start();
    (void)et_log_raw("addr=0x%02x (kv regs %u..%u)", 17u, 1000u, 1003u);
    cap_stop();
    ET_CHECK(strcmp(g_buf, "addr=0x11 (kv regs 1000..1003)") == 0);
    ET_CHECK(strstr(g_buf, "%") == NULL);           /* 无任何字面 % 残留 */
    ET_CHECK(g_fail == 0);
}

/* ---- 既有规格语义不变 (HC-1 回归) ---- */
static void log_legacy_unchanged(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("legacy.d",  "%d|%i", -7, 42);
    CHECK_FMT("legacy.u",  "%u", 42u);
    CHECK_FMT("legacy.x",  "%x|%X", 255u, 3735928559u);
    CHECK_FMT("legacy.c",  "%c|%c", 'Z', '%');
    CHECK_FMT("legacy.s",  "%s", "ab");
    CHECK_FMT("legacy.ld", "%ld|%lx", 123456789L, 0xBEEFL);
    CHECK_FMT("legacy.pct", "%u%%", (unsigned)SENT);

    CHECK_LIT("legacy.null_str", "(null)" SENT_TAIL, "%s S=%u",
              (const char *)NULL, (unsigned)SENT);
    CHECK_LIT("legacy.p_null", "0x0" SENT_TAIL, "%p S=%u",
              (void *)0, (unsigned)SENT);
    CHECK_FMT("legacy.hd", "%hd|%hhu", 5, 200u);
    ET_CHECK(g_fail == 0);
}

/* ---- 组合: 一次调用覆盖多规格, 哨兵收尾 ---- */
static void log_mixed_matrix(void)
{
    et_log_set_level(ET_LOG_LEVEL_TRACE);
    g_fail = 0;

    CHECK_FMT("mixed.all", "u=%u h=%02x s=%-4s f=%+d p=%.2d",
              7u, 0xABu, "xy", -3, 5);
    CHECK_FMT("mixed.star_both", "[%*.*s]", 8, 3, "ABCDEF");
    CHECK_FMT("mixed.chain", "%04u%04u%04u", 1u, 22u, 333u);
    ET_CHECK(g_fail == 0);
}

const et_test_case_t *test_log_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"log.level_filter",   log_level_filter},
        {"log.prefix_shape",   log_prefix_shape},
        {"log.int_formats",    log_int_formats},
        {"log.str_char_pct",   log_str_char_percent},
        {"log.longlong",       log_longlong},
        {"log.hexdump_layout", log_hexdump_layout},
        {"log.raw_passthrough", log_raw_passthrough},
        /* ---- v2.5 规格解析加固 (P1-3, ≥16 例) ---- */
        {"log.field_width",     log_field_width},
        {"log.left_align",      log_left_align},
        {"log.sign_flags",      log_sign_flags},
        {"log.alt_prefix",      log_alt_prefix},
        {"log.star_width",      log_star_width},
        {"log.precision_int",   log_precision_int},
        {"log.precision_str",   log_precision_str},
        {"log.length_mods",     log_length_mods},
        {"log.size_mod",        log_size_mod},
        {"log.unsup_float",     log_unsupported_float},
        {"log.unsup_others",    log_unsupported_others},
        {"log.unknown_conv",    log_unknown_conv},
        {"log.no_arg_specs",    log_no_arg_specs},
        {"log.field_cap",       log_field_cap},
        {"log.misalign_regress", log_misalign_regression},
        {"log.legacy_unchanged", log_legacy_unchanged},
        {"log.mixed_matrix",    log_mixed_matrix},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

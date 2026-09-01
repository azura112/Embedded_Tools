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
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}

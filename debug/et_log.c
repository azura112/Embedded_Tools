/**
 * @file    et_log.c
 * @brief   分级日志实现 (精简格式化器)
 */
#include "et_log.h"
#include "port.h"

#if ET_MODULE_LOG

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

static volatile et_log_level_t g_level = ET_LOG_LEVEL_INFO;

void et_log_set_level(et_log_level_t lv)
{
    /* lv 为枚举类型, 合法值恒 >= TRACE, 下界无需再判 */
    if ((int)lv > ET_LOG_LEVEL_NONE) {
        lv = ET_LOG_LEVEL_NONE;
    }
    g_level = lv;
}

et_log_level_t et_log_get_level(void)
{
    return g_level;
}

static void emit_char(char c)
{
    port_putc(c);
}

static int emit_str_cnt(const char *s)
{
    int n = 0;

    if (s == NULL) {
        s = "(null)";
    }
    while (*s != '\0') {
        port_putc(*s++);
        n++;
    }
    return n;
}

static void emit_str(const char *s)
{
    (void)emit_str_cnt(s);
}

/* 无符号整数输出(2~16进制), 返回字符数(64 位承载, 兼容 ll 修饰) */
static int emit_uint(unsigned long long v, uint8_t base, bool upper)
{
    char        tmp[20];
    uint8_t     n = 0u;
    int         cnt = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (v == 0u) {
        port_putc('0');
        return 1;
    }
    while (v != 0u) {
        tmp[n++] = digits[(uint8_t)(v % (unsigned)base)];
        v /= (unsigned)base;
    }
    while (n != 0u) {
        port_putc(tmp[--n]);
        cnt++;
    }
    return cnt;
}

/* 单字节两位大写十六进制 */
static void emit_hex_byte(uint8_t b)
{
    static const char hx[] = "0123456789ABCDEF";

    port_putc(hx[b >> 4]);
    port_putc(hx[b & 0x0Fu]);
}

/*
 * 精简格式化: 直接写 port_putc。
 * 支持: %d %i %u %x %X %c %s %p %% 与 l/ll 长度修饰
 */
static int vformat(const char *fmt, va_list ap)
{
    int cnt = 0;

    if (fmt == NULL) {
        return 0;
    }
    while (*fmt != '\0') {
        if (*fmt != '%') {
            port_putc(*fmt++);
            cnt++;
            continue;
        }
        fmt++;                                  /* 跳过 '%' */

        {
            uint8_t longs = 0u;

            while ((*fmt == 'l') && (longs < 2u)) {
                fmt++;
                longs++;
            }

            switch (*fmt) {
            case 'd':
            case 'i': {
                long long v;

                if (longs == 0u) {
                    v = (long long)va_arg(ap, int);
                } else if (longs == 1u) {
                    v = (long long)va_arg(ap, long);
                } else {
                    v = va_arg(ap, long long);
                }
                if (v < 0) {
                    port_putc('-');
                    cnt++;
                    v = -v;
                }
                cnt += emit_uint((unsigned long long)v, 10u, false);
                break;
            }
            case 'u':
            case 'x':
            case 'X': {
                unsigned long long v;

                if (longs == 0u) {
                    v = (unsigned long long)va_arg(ap, unsigned int);
                } else if (longs == 1u) {
                    v = (unsigned long long)va_arg(ap, unsigned long);
                } else {
                    v = va_arg(ap, unsigned long long);
                }
                if (*fmt == 'u') {
                    cnt += emit_uint(v, 10u, false);
                } else {
                    cnt += emit_uint(v, 16u, (*fmt == 'X'));
                }
                break;
            }
            case 'c':
                port_putc((char)va_arg(ap, int));
                cnt++;
                break;
            case 's':
                cnt += emit_str_cnt(va_arg(ap, const char *));
                break;
            case 'p':
                cnt += emit_str_cnt("0x");
                cnt += emit_uint((unsigned long long)(uintptr_t)va_arg(ap, void *),
                                 16u, false);
                break;
            case '%':
                port_putc('%');
                cnt++;
                break;
            case '\0':
                port_putc('%');
                cnt++;
                return cnt;
            default:                            /* 未知规格原样回显 */
                port_putc('%');
                port_putc(*fmt);
                cnt += 2;
                break;
            }

            if (*fmt != '\0') {
                fmt++;
            }
        }
    }
    return cnt;
}

int et_log_output(et_log_level_t lv, const char *tag, const char *fmt, ...)
{
    static const char lvl_ch[5] = { 'T', 'D', 'I', 'W', 'E' };
    va_list ap;
    int cnt = 0;

#if ET_LOG_MAX_LEVEL > ET_LOG_LEVEL_TRACE
    if ((int)lv < ET_LOG_MAX_LEVEL) {           /* 双保险: 比编译期裁剪线更详细的级别直接丢弃 */
        return -1;
    }
#endif
    if ((int)lv < (int)g_level) {
        return -1;
    }

    emit_char('[');
    cnt += emit_uint((unsigned long)port_tick_get_ms(), 10u, false);
    emit_str("][");
    emit_char(lvl_ch[(int)lv]);
    emit_str("][");
    cnt += emit_str_cnt(tag);
    emit_str("] ");

    va_start(ap, fmt);
    cnt += vformat(fmt, ap);
    va_end(ap);

    emit_char('\n');
    cnt++;
    return cnt;
}

int et_log_raw(const char *fmt, ...)
{
    va_list ap;
    int cnt;

    va_start(ap, fmt);
    cnt = vformat(fmt, ap);
    va_end(ap);
    return cnt;
}

void et_log_hexdump(et_log_level_t lv, const char *tag,
                    const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t off;

#if ET_LOG_MAX_LEVEL > ET_LOG_LEVEL_TRACE
    if ((int)lv < ET_LOG_MAX_LEVEL) {
        return;
    }
#endif
    if ((int)lv < (int)g_level) {
        return;
    }
    for (off = 0u; off < len; off += 16u) {
        uint32_t i;
        uint8_t  s;

        emit_char('[');
        emit_uint((unsigned long)port_tick_get_ms(), 10u, false);
        emit_str("][H][");
        emit_str_cnt(tag);
        emit_str("] ");
        for (s = 16u; s != 0u; s -= 4u) {       /* 偏移固定 4 位十六进制 */
            static const char hx[] = "0123456789ABCDEF";

            emit_char(hx[(off >> (s - 4u)) & 0x0Fu]);
        }
        emit_str(": ");

        for (i = 0u; i < 16u; i++) {
            if ((off + i) < len) {
                emit_hex_byte(p[off + i]);
            } else {
                emit_str("  ");
            }
            emit_char(' ');
        }
        emit_char('|');
        for (i = 0u; (i < 16u) && ((off + i) < len); i++) {
            uint8_t b = p[off + i];

            emit_char(((b >= 0x20u) && (b < 0x7Fu)) ? (char)b : '.');
        }
        emit_str("|\n");
    }
}

#endif /* ET_MODULE_LOG */

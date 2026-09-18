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

/* =====================================================================
 * 规格解析 (v2.5 加固, 缺陷清偿)
 *
 * 旧实现只认 l/ll 后紧跟转换字符, 其余一律"字面回显 %x" —— 既输出错字形,
 * 又**不消费对应实参**, 使后续实参整体错位(`%02x` 之后的 %u 会取到前一个
 * 实参的值)。新实现按 C 语义**完整吃掉**规格序列并消费实参:
 *
 *   标志{- 0 + 空格 #} + 十进制域宽(含 * 取实参) + 精度(.N / .*)
 *   + 长度修饰{h hh l ll z} + 转换字符
 *
 * 三类转换字符的处置:
 *   - 支持   : d i u x X c s p %      → 按新语义格式化输出
 *   - 已知不支持(标准 C 有定义, 消费实参 + 输出可见占位):
 *              o f F e E g G a A n, 以及宽字符 lc/ls
 *   - 未知   : 输出可见占位且**不消费**(该例外见 et_log.h 头注)
 *
 * 可见占位字形 = `<?` + 转换字符 + `>`(如 `%f` → `<?f>`, `%lc` → `<?lc>`)。
 * 域宽/精度上限 = ET_LOG_FIELD_MAX(超限按上限截断, 不无限刷字符)。
 * ===================================================================== */

/* 长度修饰编码 */
#define SP_LEN_NONE     0u
#define SP_LEN_H        1u
#define SP_LEN_HH       2u
#define SP_LEN_L        3u
#define SP_LEN_LL       4u
#define SP_LEN_Z        5u

typedef struct {
    bool     left;          /* '-' 左对齐 */
    bool     zero;          /* '0' 补零(有精度时对整数无效) */
    bool     plus;          /* '+' 恒显符号 */
    bool     space;         /* ' ' 无符号时占位空格 */
    bool     alt;           /* '#' 备用形式(x/X 的 0x 前缀) */
    uint32_t width;         /* 域宽(已截断至 ET_LOG_FIELD_MAX) */
    uint32_t prec;          /* 精度(已截断至 ET_LOG_FIELD_MAX) */
    bool     has_prec;      /* 精度是否给出 */
    uint8_t  len;           /* SP_LEN_* */
} et_spec_t;

static int emit_pad(char c, uint32_t n)
{
    uint32_t i;

    for (i = 0u; i < n; i++) {
        port_putc(c);
    }
    return (int)n;
}

/* 可见占位: <?转换字符> */
static int emit_placeholder(const char *conv, uint8_t clen)
{
    uint8_t i;

    port_putc('<');
    port_putc('?');
    for (i = 0u; i < clen; i++) {
        port_putc(conv[i]);
    }
    port_putc('>');
    return 2 + (int)clen + 1;
}

/* 单字符占位(浮点族/未知转换字符/%%n 等) */
static int emit_placeholder1(char conv)
{
    port_putc('<');
    port_putc('?');
    port_putc(conv);
    port_putc('>');
    return 4;
}

/* 数字主体: 前导补零至 mindig 位(不写 tmp 之外, 上限由调用方保证) */
static int emit_digits(unsigned long long v, uint8_t base, bool upper,
                       uint32_t mindig)
{
    char        tmp[20];
    uint8_t     n = 0u;
    uint32_t    pad0;
    int         cnt = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (v == 0u) {
        tmp[n++] = '0';
    } else {
        while (v != 0u) {
            tmp[n++] = digits[(uint8_t)(v % (unsigned)base)];
            v /= (unsigned)base;
        }
    }
    /* 前导零单独输出: 精度可达 ET_LOG_FIELD_MAX, 不能落进 tmp[20] */
    pad0 = ((uint32_t)n < mindig) ? (mindig - (uint32_t)n) : 0u;
    cnt += emit_pad('0', pad0);
    while (n != 0u) {
        port_putc(tmp[--n]);
        cnt++;
    }
    return cnt;
}

/* 数值输出: 符号 + 前缀 + 精度补零 + 域宽/左对齐/零填充(C 语义) */
static int emit_number(const et_spec_t *sp, unsigned long long mag, uint8_t base,
                       bool upper, char sign, const char *pfx, uint8_t plen)
{
    uint32_t body = (uint32_t)((mag == 0u) ? 1u : 0u);
    uint32_t total;
    uint32_t pad;
    uint32_t i;
    int      cnt = 0;
    unsigned long long t = mag;

    while (t != 0u) {                           /* 主体位数 */
        t /= (unsigned)base;
        body++;
    }
    if (sp->has_prec) {
        if ((sp->prec == 0u) && (mag == 0u)) {
            body = 0u;                          /* C: 精度 0 + 值 0 → 不输出数字 */
        } else if (sp->prec > body) {
            body = sp->prec;                    /* 精度 = 最少位数(补零) */
        }
    }
    total = ((sign != '\0') ? 1u : 0u) + (uint32_t)plen + body;
    pad   = (sp->width > total) ? (sp->width - total) : 0u;

    if (!sp->left && !(sp->zero && !sp->has_prec)) {
        cnt += emit_pad(' ', pad);              /* 空格填充(符号/前缀之前) */
        pad = 0u;
    }
    if (sign != '\0') {
        port_putc(sign);
        cnt++;
    }
    for (i = 0u; i < (uint32_t)plen; i++) {
        port_putc(pfx[i]);
        cnt++;
    }
    if (!sp->left && (pad > 0u)) {
        cnt += emit_pad('0', pad);              /* '0' 标志: 符号/前缀之后补零 */
    }
    if (body > 0u) {                            /* 精度 0 + 值 0 → 无数字(C 语义) */
        cnt += emit_digits(mag, base, upper, body);
    }
    if (sp->left && (pad > 0u)) {
        cnt += emit_pad(' ', pad);              /* 左对齐: 右侧空格 */
    }
    return cnt;
}

/* 字符串域输出: 精度截断 + 域宽/左对齐 */
static int emit_str_field(const et_spec_t *sp, const char *s)
{
    uint32_t len = 0u;
    uint32_t lim = sp->has_prec ? sp->prec : 0xFFFFFFFFu;
    uint32_t pad;
    uint32_t i;
    int      cnt = 0;

    if (s == NULL) {
        s = "(null)";
    }
    while ((len < lim) && (s[len] != '\0')) {
        len++;
    }
    pad = (sp->width > len) ? (sp->width - len) : 0u;
    if (!sp->left) {
        cnt += emit_pad(' ', pad);
    }
    for (i = 0u; i < len; i++) {
        port_putc(s[i]);
        cnt++;
    }
    if (sp->left) {
        cnt += emit_pad(' ', pad);
    }
    return cnt;
}

/* 解析一个规格序列: 入口 *pp 指向 '%' 之后, 出口 *pp 指向转换字符 */
static char parse_spec(const char **pp, et_spec_t *sp, va_list *ap)
{
    const char *p = *pp;

    sp->left     = false;
    sp->zero     = false;
    sp->plus     = false;
    sp->space    = false;
    sp->alt      = false;
    sp->width    = 0u;
    sp->prec     = 0u;
    sp->has_prec = false;
    sp->len      = SP_LEN_NONE;

    for (;;) {                                  /* 标志(可任意顺序/重复) */
        if (*p == '-')      { sp->left  = true; p++; }
        else if (*p == '0') { sp->zero  = true; p++; }
        else if (*p == '+') { sp->plus  = true; p++; }
        else if (*p == ' ') { sp->space = true; p++; }
        else if (*p == '#') { sp->alt   = true; p++; }
        else break;
    }
    if (*p == '*') {                            /* 域宽取实参(负值 = 左对齐) */
        int w = va_arg(*ap, int);

        p++;
        if (w < 0) {
            sp->left = true;
            w = (w == (-2147483647 - 1)) ? 2147483647 : -w;   /* 防 INT_MIN 溢出 */
        }
        sp->width = (uint32_t)w;
    } else {
        while ((*p >= '0') && (*p <= '9')) {
            if (sp->width < ET_LOG_FIELD_MAX) { /* 到上限即停: 防溢出/防刷屏 */
                sp->width = (sp->width * 10u) + (uint32_t)(*p - '0');
                if (sp->width > ET_LOG_FIELD_MAX) {
                    sp->width = ET_LOG_FIELD_MAX;
                }
            }
            p++;
        }
    }
    if (sp->width > ET_LOG_FIELD_MAX) {
        sp->width = ET_LOG_FIELD_MAX;
    }
    if (*p == '.') {                            /* 精度 */
        p++;
        sp->has_prec = true;
        if (*p == '*') {
            int q = va_arg(*ap, int);

            p++;
            if (q < 0) {
                sp->has_prec = false;           /* C: 负精度视为未给出 */
            } else {
                sp->prec = (uint32_t)q;
            }
        } else {
            while ((*p >= '0') && (*p <= '9')) {
                if (sp->prec < ET_LOG_FIELD_MAX) {
                    sp->prec = (sp->prec * 10u) + (uint32_t)(*p - '0');
                    if (sp->prec > ET_LOG_FIELD_MAX) {
                        sp->prec = ET_LOG_FIELD_MAX;
                    }
                }
                p++;
            }
        }
        if (sp->prec > ET_LOG_FIELD_MAX) {
            sp->prec = ET_LOG_FIELD_MAX;
        }
    }
    if (*p == 'h') {                            /* 长度修饰 */
        sp->len = SP_LEN_H;
        p++;
        if (*p == 'h') { sp->len = SP_LEN_HH; p++; }
    } else if (*p == 'l') {
        sp->len = SP_LEN_L;
        p++;
        if (*p == 'l') { sp->len = SP_LEN_LL; p++; }
    } else if (*p == 'z') {
        sp->len = SP_LEN_Z;
        p++;
    }
    *pp = p;
    return (*p != '\0') ? *p : '\0';
}

/* 消费有符号实参(h/hh 按 C 语义截断; z 取 size_t 同宽有符号) */
static long long get_signed(va_list *ap, const et_spec_t *sp)
{
    switch (sp->len) {
    case SP_LEN_H:  return (long long)(short)va_arg(*ap, int);
    case SP_LEN_HH: return (long long)(signed char)va_arg(*ap, int);
    case SP_LEN_L:  return (long long)va_arg(*ap, long);
    case SP_LEN_LL: return va_arg(*ap, long long);
    case SP_LEN_Z:  return (long long)va_arg(*ap, ptrdiff_t);
    default:        return (long long)va_arg(*ap, int);
    }
}

/* 消费无符号实参(h/hh 按 C 语义截断) */
static unsigned long long get_unsigned(va_list *ap, const et_spec_t *sp)
{
    switch (sp->len) {
    case SP_LEN_H:  return (unsigned long long)(unsigned short)va_arg(*ap, unsigned int);
    case SP_LEN_HH: return (unsigned long long)(unsigned char)va_arg(*ap, unsigned int);
    case SP_LEN_L:  return (unsigned long long)va_arg(*ap, unsigned long);
    case SP_LEN_LL: return va_arg(*ap, unsigned long long);
    case SP_LEN_Z:  return (unsigned long long)va_arg(*ap, size_t);
    default:        return (unsigned long long)va_arg(*ap, unsigned int);
    }
}

/*
 * 精简格式化: 直接写 port_putc。
 * 支持: %d %i %u %x %X %c %s %p %% + 标志/域宽(含 *)/精度(.N,.*)/h hh l ll z
 * 不支持但按 C 语义消费: %o %f %e %E %g %G %a %A %n %lc %ls (输出可见占位)
 */
static int vformat(const char *fmt, va_list ap)
{
    int cnt = 0;

    if (fmt == NULL) {
        return 0;
    }
    while (*fmt != '\0') {
        et_spec_t sp;
        char      conv;

        if (*fmt != '%') {
            port_putc(*fmt++);
            cnt++;
            continue;
        }
        fmt++;                                  /* 跳过 '%' */

        conv = parse_spec(&fmt, &sp, &ap);
        if (conv == '\0') {                     /* 尾部孤 '%': 原样输出 */
            port_putc('%');
            cnt++;
            return cnt;
        }

        switch (conv) {
        case 'd':
        case 'i': {
            long long          v = get_signed(&ap, &sp);
            char               sign = '\0';
            unsigned long long mag;

            if (v < 0) {
                sign = '-';
                mag  = (unsigned long long)(-(v + 1)) + 1u;   /* 防 LLONG_MIN */
            } else {
                mag = (unsigned long long)v;
                if (sp.plus)       { sign = '+'; }
                else if (sp.space) { sign = ' '; }
            }
            cnt += emit_number(&sp, mag, 10u, false, sign, "", 0u);
            break;
        }
        case 'u':
            cnt += emit_number(&sp, get_unsigned(&ap, &sp), 10u, false, '\0', "", 0u);
            break;
        case 'x':
        case 'X': {
            bool               up = (conv == 'X');
            unsigned long long v  = get_unsigned(&ap, &sp);

            cnt += emit_number(&sp, v, 16u, up,
                               '\0',
                               (sp.alt && (v != 0u)) ? (up ? "0X" : "0x") : "",
                               (uint8_t)((sp.alt && (v != 0u)) ? 2u : 0u));
            break;
        }
        case 'c':
            if (sp.len >= SP_LEN_L) {           /* %lc 宽字符: 消费不输出 */
                (void)va_arg(ap, int);
                cnt += emit_placeholder("lc", 2u);
            } else {
                port_putc((char)va_arg(ap, int));
                cnt++;
            }
            break;
        case 's':
            if (sp.len >= SP_LEN_L) {           /* %ls 宽字符串: 消费不输出 */
                (void)va_arg(ap, void *);
                cnt += emit_placeholder("ls", 2u);
            } else {
                cnt += emit_str_field(&sp, va_arg(ap, const char *));
            }
            break;
        case 'p': {
            void *pv = va_arg(ap, void *);

            cnt += emit_number(&sp, (unsigned long long)(uintptr_t)pv, 16u, false,
                               '\0', "0x", 2u);
            break;
        }
        case '%':
            port_putc('%');
            cnt++;
            break;
        case 'o':                               /* 已知不支持: 消费 + 占位 */
            (void)get_unsigned(&ap, &sp);
            cnt += emit_placeholder("o", 1u);
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':                               /* 浮点族: 按 double 消费 */
            (void)va_arg(ap, double);
            cnt += emit_placeholder1(conv);
            break;
        case 'n':                               /* %n: 消费指针, 禁写内存 */
            (void)va_arg(ap, void *);
            cnt += emit_placeholder1('n');
            break;
        default:                                /* 未知转换字符: 占位不消费 */
            cnt += emit_placeholder1(conv);
            break;
        }

        if (*fmt != '\0') {
            fmt++;
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

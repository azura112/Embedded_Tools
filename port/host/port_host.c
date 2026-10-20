/**
 * @file    port_host.c
 * @brief   宿主机(PC)平台适配实现 —— 仅测试用
 *
 * 说明:
 *  - 时基为纯软件虚拟时钟, 由测试代码通过 port_host_tick_set/advance 控制;
 *  - 单线程宿主环境, 临界区为空实现;
 *  - 移植到真实 MCU 时请替换为目标平台的 port 实现(SysTick/开关中断/串口)。
 */
#include "port.h"
#include "port_host.h"
#include "et_config.h"

#include <stdio.h>
#include <string.h>

static volatile uint32_t g_virtual_ticks = 0u;

/* 输出捕获状态(测试用) */
static char    *g_cap_buf = NULL;
static uint32_t g_cap_cap = 0u;
static uint32_t g_cap_n = 0u;
static int      g_cap_active = 0;

void port_host_capture_start(char *buf, uint32_t cap)
{
    if ((buf == NULL) || (cap == 0u)) {
        return;
    }
    buf[0]      = '\0';
    g_cap_buf   = buf;
    g_cap_cap   = cap;
    g_cap_n     = 0u;
    g_cap_active = 1;
}

uint32_t port_host_capture_stop(void)
{
    g_cap_active = 0;
    if (g_cap_buf != NULL) {
        g_cap_buf[(g_cap_n < g_cap_cap) ? g_cap_n : (g_cap_cap - 1u)] = '\0';
    }
    return g_cap_n;
}

void port_host_tick_set(uint32_t ms)
{
    g_virtual_ticks = ms;
}

void port_host_tick_advance(uint32_t delta)
{
    g_virtual_ticks += delta;
}

uint32_t port_host_tick_now(void)
{
    return g_virtual_ticks;
}

void port_critical_enter(void)
{
    /* 单线程空实现 */
}

void port_critical_exit(void)
{
    /* 单线程空实现 */
}

port_tick_ms_t port_tick_get_ms(void)
{
    return g_virtual_ticks;
}

void port_putc(char c)
{
    if (g_cap_active != 0) {
        if (g_cap_n < g_cap_cap) {
            g_cap_buf[g_cap_n++] = c;
        }
        return;
    }
    (void)putchar(c);
}

/* ===================== flash 参数区模拟 ===================== */

static uint8_t  g_flash[PORT_HOST_FLASH_BYTES];  /* 参数区 + A/B 槽 + 状态页(同一镜像) */
static uint32_t g_fail_limit = 0xFFFFFFFFu;     /* 掉电注入阈值(写入累计) */
static uint32_t g_written    = 0u;              /* 累计写入字节数 */
static bool     g_erase_fail = false;           /* 下次擦除只擦前半 */

void port_host_flash_reset(void)
{
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_fail_limit = 0xFFFFFFFFu;
    g_written    = 0u;
    g_erase_fail = false;
}

uint8_t *port_host_flash_mem(uint32_t offset)
{
    if (offset >= sizeof(g_flash)) {
        return NULL;
    }
    return &g_flash[offset];
}

void port_host_flash_fail_after(uint32_t n)
{
    g_fail_limit = n;
}

void port_host_flash_erase_fail_once(void)
{
    g_erase_fail = true;
}

uint32_t port_host_flash_written(void)
{
    return g_written;
}

bool port_flash_read(uint32_t offset, void *buf, uint32_t len)
{
    if ((offset > sizeof(g_flash)) || (len > sizeof(g_flash) - offset)) {
        return false;
    }
    memcpy(buf, &g_flash[offset], len);
    return true;
}

uint32_t port_flash_write(uint32_t offset, const void *buf, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t i;

    if ((offset > sizeof(g_flash)) || (len > sizeof(g_flash) - offset)) {
        return 0u;
    }
    if (((offset % 4u) != 0u) || ((len % 4u) != 0u)) {
        return 0u;                              /* 4B 对齐契约 */
    }

    for (i = 0u; i < len; i++) {
        uint8_t old = g_flash[offset + i];
        uint8_t new = src[i];

        if (g_written >= g_fail_limit) {
            /* 掉电: 只截断本次, 之后(模拟重启后)恢复可用 */
            g_fail_limit = 0xFFFFFFFFu;
            break;
        }
        if (((uint8_t)(new & (uint8_t)~old)) != 0u) {
            break;                              /* 位写违约: 0 位不可写成 1 */
        }
        g_flash[offset + i] = new;
        g_written++;
    }
    return i;
}

/* 页擦除(供参数区扇区擦除与区域擦除共用): 擦除失败注入沿用同一 g_erase_fail 机制
 * —— 只擦前半, 等效擦除中断(掉电矩阵用例的"坏页"来源)。 */
static bool host_erase_page(uint32_t page_base, uint32_t page_size)
{
    uint32_t n;

    if ((page_base > sizeof(g_flash)) || (page_size > sizeof(g_flash) - page_base)) {
        return false;
    }
    n = g_erase_fail ? (page_size / 2u) : page_size;
    g_erase_fail = false;
    memset(&g_flash[page_base], 0xFF, n);
    return true;
}

bool port_flash_erase_sector(uint32_t sector_index)
{
    if (sector_index >= PORT_FLASH_SECTOR_COUNT) {
        return false;
    }
    return host_erase_page(sector_index * (uint32_t)PORT_FLASH_SECTOR_SIZE,
                           (uint32_t)PORT_FLASH_SECTOR_SIZE);
}

/* ===================== flash 区域(分区表)实现 (v2.29 P2-1/P2-2, REQ-7) =====================
 * host 绝对地址 = 本镜像字节偏移(参数区基址 0) ⇒ 掉电截断与半途擦除注入在区域位形
 * 下与参数区共用同一套开关(port_host_flash_fail_after / _erase_fail_once)。
 */

/* 编译期对账: 分区表条目数须与能力宏一致(否则循环上界与数组维度错位)。
 * C99 负数组尺寸技巧, 与 et_bootctl.h 的镜像头 32B 断言同族。 */
typedef char port_host_regions_check_[(PORT_FLASH_REGIONS == 4u) ? 1 : -1];

static const port_flash_region_info_t g_regions[PORT_FLASH_REGIONS] = {
    { PORT_HOST_REGION_PARAM,  0u,
      PORT_HOST_PARAM_BYTES,            (uint32_t)PORT_FLASH_SECTOR_SIZE },
    { PORT_HOST_REGION_SLOT_A, PORT_HOST_PARAM_BYTES,
      PORT_HOST_SLOT_BYTES,             (uint32_t)PORT_FLASH_SECTOR_SIZE },
    { PORT_HOST_REGION_SLOT_B, PORT_HOST_PARAM_BYTES + PORT_HOST_SLOT_BYTES,
      PORT_HOST_SLOT_BYTES,             (uint32_t)PORT_FLASH_SECTOR_SIZE },
    { PORT_HOST_REGION_STATE,  PORT_HOST_PARAM_BYTES + 2u * PORT_HOST_SLOT_BYTES,
      PORT_FLASH_SECTOR_SIZE,           (uint32_t)PORT_FLASH_SECTOR_SIZE }
};

static const port_flash_region_info_t *host_region_of(uint32_t addr)
{
    uint32_t i;

    for (i = 0u; i < (uint32_t)PORT_FLASH_REGIONS; i++) {
        const port_flash_region_info_t *r = &g_regions[i];

        if ((addr >= r->base) && ((addr - r->base) < r->size)) {
            return r;
        }
    }
    return NULL;                        /* 不在任何区域内 */
}

bool port_flash_region_lookup(uint32_t addr, port_flash_region_info_t *info)
{
    const port_flash_region_info_t *r;

    if (info == NULL) {
        return false;
    }
    r = host_region_of(addr);
    if (r == NULL) {
        return false;
    }
    *info = *r;
    return true;
}

bool port_flash_region_get(uint32_t index, port_flash_region_info_t *info)
{
    if ((info == NULL) || (index >= (uint32_t)PORT_FLASH_REGIONS)) {
        return false;
    }
    *info = g_regions[index];
    return true;
}

bool port_flash_region_read(uint32_t addr, void *buf, uint32_t len)
{
    const port_flash_region_info_t *r = host_region_of(addr);

    if ((r == NULL) || (len > (r->size - (addr - r->base)))) {
        return false;                   /* 越界或跨区 */
    }
    return port_flash_read(addr, buf, len);
}

uint32_t port_flash_region_write(uint32_t addr, const void *buf, uint32_t len)
{
    const port_flash_region_info_t *r = host_region_of(addr);

    if ((r == NULL) || (len > (r->size - (addr - r->base)))) {
        return 0u;
    }
    return port_flash_write(addr, buf, len);
}

bool port_flash_region_erase(uint32_t addr)
{
    const port_flash_region_info_t *r = host_region_of(addr);
    uint32_t off;

    if (r == NULL) {
        return false;
    }
    off = ((addr - r->base) / r->page_size) * r->page_size;   /* 页内任地址 → 该页首字节 */
    return host_erase_page(r->base + off, r->page_size);
}

/* ===================== 看门狗软件模拟 (et_wdt 测试用) ===================== */

static port_host_wdt_cb_t g_wdt_cb   = NULL;    /* 超时回调 */
static void              *g_wdt_user = NULL;
static uint32_t           g_wdt_timeout = 0u;   /* 0 = 未启用 */
static uint32_t           g_wdt_last_feed = 0u; /* 最近一次喂狗时刻 */
static uint32_t           g_wdt_feeds = 0u;     /* 累计喂狗计数 */
static bool               g_wdt_fired = false;  /* 超时已触发(不重复) */

bool port_wdt_enable(uint32_t timeout_ms)
{
    if (timeout_ms < PORT_FLASH_ERASE_MS_MAX * 2u) {
        return false;                           /* 契约下限: 擦除窗口 ×2 */
    }
    g_wdt_timeout   = timeout_ms;
    g_wdt_last_feed = port_tick_get_ms();
    g_wdt_fired     = false;
    return true;
}

void port_wdt_feed(void)
{
    if (g_wdt_timeout != 0u) {
        g_wdt_last_feed = port_tick_get_ms();
        g_wdt_feeds++;
    }
}

bool port_wdt_disable(void)
{
    /* host 模拟可停 (真实 IWDG 不可停, 返回 false) */
    g_wdt_timeout = 0u;
    return true;
}

void port_host_wdt_install(port_host_wdt_cb_t cb, void *user)
{
    g_wdt_cb   = cb;
    g_wdt_user = user;
}

void port_host_wdt_poll(void)
{
    if ((g_wdt_timeout != 0u) && (!g_wdt_fired) && (g_wdt_cb != NULL) &&
        ((port_tick_get_ms() - g_wdt_last_feed) >= g_wdt_timeout)) {
        g_wdt_fired = true;
        g_wdt_cb(g_wdt_user);
    }
}

uint32_t port_host_wdt_feeds(void)
{
    return g_wdt_feeds;
}

void port_host_wdt_reset(void)
{
    g_wdt_timeout   = 0u;
    g_wdt_last_feed = 0u;
    g_wdt_feeds     = 0u;
    g_wdt_fired     = false;
    g_wdt_cb        = NULL;
    g_wdt_user      = NULL;
}

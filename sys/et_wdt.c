/**
 * @file    et_wdt.c
 * @brief   看门狗封装实现 (契约下限校验 + guard 阻塞段保护)
 */
#include "et_wdt.h"
#include "port.h"

#if ET_MODULE_WDT

bool et_wdt_enable(uint32_t timeout_ms)
{
    if (timeout_ms < PORT_FLASH_ERASE_MS_MAX * 2u) {
        return false;                       /* 库层下限: 擦除窗口 ×2 */
    }
    return port_wdt_enable(timeout_ms);     /* port 层再做同规则校验+落硬件 */
}

void et_wdt_feed(void)
{
    port_wdt_feed();                        /* 🔒ISR-safe */
}

bool et_wdt_disable(void)
{
    return port_wdt_disable();              /* IWDG 语义: 可能 false */
}

bool et_wdt_guard(et_wdt_job_fn fn, void *user)
{
    bool ret;

    if (fn == NULL) {
        return false;
    }
    port_wdt_feed();                        /* 进入长操作前喂一次 */
    ret = fn(user);
    port_wdt_feed();                        /* 退出后再喂, 留足下一窗口 */
    return ret;
}

#endif /* ET_MODULE_WDT */

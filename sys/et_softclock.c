/**
 * @file    et_softclock.c
 * @brief   软时钟实现
 */
#include "et_softclock.h"

#if ET_MODULE_SOFTCLOCK

#define SC_MS_PER_SEC   1000u
#define SC_SEC_PER_DAY  86400u

/* days 自 1970-01-01 起算 → 公历日期 (Hinnant civil_from_days, 纯整数) */
static void sc_civil_from_days(uint32_t z, uint16_t *y, uint8_t *m, uint8_t *d)
{
    uint32_t era, yoe, doy, mp, doe, yy, mm, dd;

    z   += 719468u;
    era  = z / 146097u;                                     /* [0, ~0.9] */
    doe  = z - era * 146097u;                               /* [0, 146096] */
    yoe  = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    yy   = yoe + era * 400u;
    doy  = doe - (365u * yoe + yoe / 4u - yoe / 100u);      /* [0, 365] */
    mp   = (5u * doy + 2u) / 153u;                          /* [0, 11] */
    dd   = doy - (153u * mp + 2u) / 5u + 1u;                /* [1, 31] */
    mm   = mp + ((mp < 10u) ? 3u : (uint32_t)(0u - 9u));    /* [1, 12] */
    if (mm <= 2u) {
        yy++;
    }
    *y = (uint16_t)yy;
    *m = (uint8_t)mm;
    *d = (uint8_t)dd;
}

bool et_softclock_init(et_softclock_t *sc, uint32_t unix_sec)
{
    ET_ASSERT(sc != NULL);
    if (sc == NULL) {
        return false;
    }
    sc->unix_sec  = unix_sec;
    sc->last_tick = 0u;
    sc->acc_ms    = 0u;
    sc->has_last  = false;
    return true;
}

void et_softclock_set_unix(et_softclock_t *sc, uint32_t unix_sec)
{
    ET_ASSERT(sc != NULL);
    if (sc == NULL) {
        return;
    }
    sc->unix_sec = unix_sec;                /* acc_ms 保留: 毫秒推进不受影响 */
}

uint32_t et_softclock_unix(const et_softclock_t *sc)
{
    ET_ASSERT(sc != NULL);
    return (sc == NULL) ? 0u : sc->unix_sec;
}

void et_softclock_poll(et_softclock_t *sc, uint32_t now_ms)
{
    uint32_t delta;

    ET_ASSERT(sc != NULL);
    if (sc == NULL) {
        return;
    }
    if (!sc->has_last) {
        sc->last_tick = now_ms;             /* 首次 poll 只锁定基准 */
        sc->has_last  = true;
        return;
    }
    delta = now_ms - sc->last_tick;         /* 无符号减法, 回绕安全 */
    sc->last_tick = now_ms;
    sc->acc_ms += delta;
    if (sc->acc_ms >= SC_MS_PER_SEC) {
        sc->unix_sec += sc->acc_ms / SC_MS_PER_SEC;
        sc->acc_ms %= SC_MS_PER_SEC;
    }
}

bool et_softclock_get_datetime(const et_softclock_t *sc, et_datetime_t *dt)
{
    uint32_t days, sod;

    ET_ASSERT(sc != NULL);
    ET_ASSERT(dt != NULL);
    if ((sc == NULL) || (dt == NULL)) {
        return false;
    }
    days = sc->unix_sec / SC_SEC_PER_DAY;
    sod  = sc->unix_sec % SC_SEC_PER_DAY;

    sc_civil_from_days(days, &dt->year, &dt->month, &dt->day);
    dt->hour = (uint8_t)(sod / 3600u);
    dt->min  = (uint8_t)((sod % 3600u) / 60u);
    dt->sec  = (uint8_t)(sod % 60u);
    return true;
}

#endif /* ET_MODULE_SOFTCLOCK */

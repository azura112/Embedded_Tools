/**
 * @file    et_event.c
 * @brief   事件标志组实现
 */
#include "et_event.h"
#include "port.h"

#if ET_MODULE_EVENT

void et_event_init(et_event_group_t *g)
{
    ET_ASSERT(g != NULL);
    if (g != NULL) {
        g->flags = 0u;
    }
}

void et_event_set(et_event_group_t *g, uint32_t bits)
{
    if ((g == NULL) || (bits == 0u)) {
        return;
    }
    PORT_CRITICAL_ENTER();
    g->flags |= bits;
    PORT_CRITICAL_EXIT();
}

uint32_t et_event_peek(const et_event_group_t *g)
{
    if (g == NULL) {
        return 0u;
    }
    return g->flags;
}

uint32_t et_event_wait_and_clear(et_event_group_t *g, uint32_t mask)
{
    uint32_t got;

    if ((g == NULL) || (mask == 0u)) {
        return 0u;
    }
    PORT_CRITICAL_ENTER();
    got      = g->flags & mask;
    g->flags &= ~mask;
    PORT_CRITICAL_EXIT();
    return got;
}

void et_event_clear(et_event_group_t *g, uint32_t bits)
{
    if ((g == NULL) || (bits == 0u)) {
        return;
    }
    PORT_CRITICAL_ENTER();
    g->flags &= ~bits;
    PORT_CRITICAL_EXIT();
}

#endif /* ET_MODULE_EVENT */

#include "nv2a_pb_reserve.h"
#include "xbox_memory_layout.h"

static uint32_t pb_guest32(uint32_t va)
{
    return *(volatile uint32_t *)((uintptr_t)va + (uintptr_t)xbox_GetMemoryOffset());
}

static void pb_guest32_store(uint32_t va, uint32_t value)
{
    *(volatile uint32_t *)((uintptr_t)va + (uintptr_t)xbox_GetMemoryOffset()) = value;
}

static int pb_room(uint32_t cursor, uint32_t limit, uint32_t bytes)
{
    return cursor && !(cursor & 3u) && limit >= cursor && limit - cursor >= bytes;
}

uint32_t xbox_pb_reserve(uint32_t device, uint32_t bytes, Nv2aPbRolloverFn rollover, void *user)
{
    uint32_t cursor = pb_guest32(device), limit = pb_guest32(device + 4u);
    /* The native cursor may already be beyond its soft limit. Roll over
     * through the title's normal path rather than dropping the record. */
    if (cursor && !(cursor & 3u) && (limit < cursor || limit - cursor < bytes)) {
        rollover(user);
        cursor = pb_guest32(device);
        limit = pb_guest32(device + 4u);
    }
    if (!pb_room(cursor, limit, bytes)) return 0;
    pb_guest32_store(device, cursor + bytes);
    return cursor;
}

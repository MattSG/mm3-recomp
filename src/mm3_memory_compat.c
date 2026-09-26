#include "xbox_memory_layout.h"

/*
 * The stock reserve arena lives above RAM.  MM3 later treats the returned
 * address as a CRT heap block, so it must come from the same allocator as
 * ordinary heap allocations.
 */
uint32_t __wrap_xbox_ReserveAlloc(uint32_t size, uint32_t align)
{
    return xbox_HeapAlloc(size, align);
}

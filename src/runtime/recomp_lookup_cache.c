#include "recomp_lookup_cache.h"
#include "xbox_memory_layout.h"

#define RECOMP_LOOKUP_SLOTS 1024u

RecompLookupTarget recomp_lookup_memoized(uint32_t guest_va, RecompLookupFn uncached)
{
    static RECOMP_TLS struct { uint32_t va; RecompLookupTarget fn; } cache[RECOMP_LOOKUP_SLOTS];
    unsigned slot = (guest_va ^ (guest_va >> 10)) & (RECOMP_LOOKUP_SLOTS - 1u);
    if (cache[slot].va != guest_va || !guest_va) {
        cache[slot].fn = uncached(guest_va);
        cache[slot].va = guest_va;
    }
    return cache[slot].fn;
}

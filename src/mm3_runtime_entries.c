#include <stdint.h>
void sub_00083A6C(void);

typedef void (*mm3_recomp_func_t)(void);
mm3_recomp_func_t recomp_lookup_manual_base(uint32_t va);
mm3_recomp_func_t mm3_lookup_memory_tail(uint32_t va);
mm3_recomp_func_t mm3_lookup_recovered_entry(uint32_t va);

/* Keep title-specific recovered entries separate from the shared toolkit
 * and the existing manual ABI/SEH compatibility implementations. */
static mm3_recomp_func_t lookup_manual_uncached(uint32_t va)
{
    if (va == 0x00083A6Cu) return sub_00083A6C;
    mm3_recomp_func_t tail = mm3_lookup_memory_tail(va);
    if (tail) return tail;
    mm3_recomp_func_t recovered = mm3_lookup_recovered_entry(va);
    return recovered ? recovered : recomp_lookup_manual_base(va);
}

/* Every indirect call asks here first (most answer "not manual") and the
 * chain above is a long run of compares; the answers never change, so each
 * thread remembers them. */
mm3_recomp_func_t recomp_lookup_manual(uint32_t va)
{
#ifdef _MSC_VER
    static __declspec(thread) struct { uint32_t va; mm3_recomp_func_t fn; } cache[1024];
#else
    static _Thread_local struct { uint32_t va; mm3_recomp_func_t fn; } cache[1024];
#endif
    unsigned slot = (va ^ (va >> 10)) & 1023u;
    if (cache[slot].va != va || !va) {
        cache[slot].fn = lookup_manual_uncached(va);
        cache[slot].va = va;
    }
    return cache[slot].fn;
}

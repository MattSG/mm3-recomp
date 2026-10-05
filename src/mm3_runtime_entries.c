#include <stdint.h>
void sub_00083A6C(void);

typedef void (*mm3_recomp_func_t)(void);
mm3_recomp_func_t recomp_lookup_manual_base(uint32_t va);
mm3_recomp_func_t mm3_lookup_memory_tail(uint32_t va);
mm3_recomp_func_t mm3_lookup_recovered_entry(uint32_t va);

/* Keep title-specific recovered entries separate from the shared toolkit
 * and the existing manual ABI/SEH compatibility implementations. */
mm3_recomp_func_t recomp_lookup_manual(uint32_t va)
{
    if (va == 0x00083A6Cu) return sub_00083A6C;
    mm3_recomp_func_t tail = mm3_lookup_memory_tail(va);
    if (tail) return tail;
    mm3_recomp_func_t recovered = mm3_lookup_recovered_entry(va);
    return recovered ? recovered : recomp_lookup_manual_base(va);
}

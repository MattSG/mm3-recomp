#include <stdint.h>

typedef void (*mm3_recomp_func_t)(void);
mm3_recomp_func_t recomp_lookup_manual_base(uint32_t va);
mm3_recomp_func_t mm3_lookup_memory_tail(uint32_t va);
void sub_000D66A3(void);
void sub_00104DD3(void);
void sub_0013679A(void);

/* Keep title-specific recovered entries separate from the shared toolkit
 * and the existing manual ABI/SEH compatibility implementations. */
mm3_recomp_func_t recomp_lookup_manual(uint32_t va)
{
    mm3_recomp_func_t tail = mm3_lookup_memory_tail(va);
    if (tail) return tail;
    switch (va) {
    case 0x000D66A3u: return sub_000D66A3;
    case 0x00104DD3u: return sub_00104DD3;
    case 0x0013679Au: return sub_0013679A;
    default: return recomp_lookup_manual_base(va);
    }
}

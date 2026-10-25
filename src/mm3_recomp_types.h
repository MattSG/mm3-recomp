/* Midtown Madness 3's title layer over the runtime's generic recomp_types.h.
 * The generator and CMake copy that runtime header beside this file as
 * recomp_base.h. Only MM3-specific choices live here. */
#ifndef MM3_RECOMP_TYPES_H
#define MM3_RECOMP_TYPES_H

#include <stdint.h>

/* MM3's rep movs copies go through the hardware-window-safe memcpy. */
#define RECOMP_GUEST_MEMCPY 1

#if defined(__has_include) && __has_include("recomp_base.h")
#include "recomp_base.h"
#else
#include "../tools/xboxrecomp/templates/runtime/recomp_types.h"
#endif

/* The pinned generator lowers CPUID to recomp_cpuid. Xbox Pentium III,
 * measured through the original CPUID instruction in xemu; leaf 2 and
 * out-of-range clamping match xemu's Xbox cpu_x86_cpuid path. The feature
 * word reflects that machine's disabled APIC/VME, not host SIMD. */
static inline void recomp_cpuid(uint32_t leaf, uint32_t subleaf,
                               uint32_t *a, uint32_t *b,
                               uint32_t *c, uint32_t *d)
{
    static const uint32_t leaves[3][4] = {
        {2u, 0x756E6547u, 0x6C65746Eu, 0x49656E69u},
        {0x68Au, 0u, 0u, 0x0383F9FDu},
        {0x03020101u, 0u, 0u, 0x0C040841u}
    };
    const uint32_t *v = leaves[leaf > 2u ? 2u : leaf];
    (void)subleaf;
    /* This Xbox profile does not expose TCG hypervisor leaves. */
    if (leaf == 0x40000000u || leaf == 0x40000001u) {
        *a = *b = *c = *d = 0u;
        return;
    }
    *a = v[0]; *b = v[1]; *c = v[2]; *d = v[3];
}

#endif /* MM3_RECOMP_TYPES_H */

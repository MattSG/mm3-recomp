/* Midtown Madness 3's title layer over the runtime's generic recomp_types.h.
 * The generator and CMake copy that runtime header beside this file as
 * recomp_base.h. Only MM3-specific choices live here. */
#ifndef MM3_RECOMP_TYPES_H
#define MM3_RECOMP_TYPES_H

#include <stdint.h>

/* MM3's CPUID sites: the translator leaves these three instructions unimplemented,
 * and the title answers them with the Xbox CPUID values. */
#define RECOMP_UNIMPL(_text, _va) do {     if ((uint32_t)(_va) == 0x0009379Eu ||         (uint32_t)(_va) == 0x000A4FEDu ||         (uint32_t)(_va) == 0x000A504Bu)         recomp_cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);     else         recomp_unimpl((_text), (_va)); } while (0)

/* MM3's rep movs copies go through the hardware-window-safe memcpy. */
#define RECOMP_GUEST_MEMCPY 1

#if defined(__has_include) && __has_include("recomp_base.h")
#include "recomp_base.h"
#else
#include "../tools/xboxrecomp/templates/runtime/recomp_types.h"
#endif
#include "recomp_cpu.h"
extern RECOMP_TLS uint32_t g_eflags;

#endif /* MM3_RECOMP_TYPES_H */

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

#endif /* MM3_RECOMP_TYPES_H */

/* Declarations the generated sources use without declaring, force-included
 * into those files only (see CMakeLists.txt). C99 compilers reject implicit
 * declarations -- Clang does by default -- and GCC/MSVC only warn.
 *
 * sub_0009B4E7 and sub_0009B584: tail jumps after an unconditional goto in
 * recomp_runtime_entries.c. The pinned generator emits them for code it never
 * reaches, so nothing defines them and optimised builds drop the calls. */
#ifndef MM3_GEN_PROTOTYPES_H
#define MM3_GEN_PROTOTYPES_H
#include <stdint.h>

void sub_0009B4E7(void);
void sub_0009B584(void);

#endif

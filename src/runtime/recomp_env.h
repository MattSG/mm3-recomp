/* Cached environment switch for diagnostics.
 *
 * getenv takes the CRT's environment lock and scans the block, which is far too
 * slow on hot paths such as an allocator hook. A switch is read once, when
 * first asked for, and the answer is kept in the caller's slot: -1 until read,
 * then 0 or 1. The environment is fixed before the guest starts. */
#ifndef RECOMP_ENV_H
#define RECOMP_ENV_H

#include <stdlib.h>

static inline int recomp_env_flag(int *slot, const char *name)
{
    if (*slot < 0) *slot = getenv(name) != NULL;
    return *slot;
}

#endif

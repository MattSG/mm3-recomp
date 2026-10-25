/* Per-thread memo for a function lookup keyed by guest address.
 *
 * Indirect calls ask a lookup chain on every call, and the answers do not
 * change once made. recomp_lookup_memoized remembers each thread's answers in a
 * direct-mapped table and asks the uncached lookup only on a miss. */
#ifndef RECOMP_LOOKUP_CACHE_H
#define RECOMP_LOOKUP_CACHE_H

#include <stdint.h>

typedef void (*RecompLookupTarget)(void);
typedef RecompLookupTarget (*RecompLookupFn)(uint32_t guest_va);

RecompLookupTarget recomp_lookup_memoized(uint32_t guest_va, RecompLookupFn uncached);

#endif

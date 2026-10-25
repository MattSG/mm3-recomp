/* Refresh a derived cache file from its authoritative source.
 *
 * A cache copy is current only when its bytes match the source and, if the
 * policy asks, its creation time falls in the same bucket. Otherwise it is
 * deleted so the title rebuilds it. Deletion needs full reads of both files
 * and successful closes of read-only handles; reparse points, symlinks and
 * the same file under two names are refused and leave the cache alone.
 *
 * Guest paths are translated with xbox_translate_path. */
#ifndef XBOX_DERIVED_CACHE_H
#define XBOX_DERIVED_CACHE_H

#include <stdint.h>

typedef struct {
    const char *source;         /* guest path of the authoritative file */
    const char *cache;          /* guest path of the derived copy */
    const char *tag;            /* log tag, without brackets */
    const char *label;          /* file name used in messages */
    const char *rebuild;        /* what the title does after invalidation */
    uint64_t creation_ticks;    /* creation-time bucket, 100 ns ticks; 0 ignores it */
} XboxDerivedCachePolicy;

/* Returns 1 when the cache was invalidated, 0 when it was left in place,
 * and -1 when a path or handle could not be verified. */
int xbox_derived_cache_refresh(const XboxDerivedCachePolicy *policy);

#endif

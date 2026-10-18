#ifndef MM3_FRAME_LIMIT_H
#define MM3_FRAME_LIMIT_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* An invalid value still falls back to 60, but the first one is named in the
 * log so a mistyped setting is visible. An empty value is simply unset. */
static inline int mm3_frame_limit_rejected(const char *value)
{
    static int reported;
    if (!reported) {
        reported = 1;
        fprintf(stderr, "[FRAME_LIMIT] ignoring invalid frame limit \"%s\" (want 0 for unpaced or a positive integer); using 60\n",
                value);
    }
    return 60;
}

/* Rendering is independent of the console-rate simulation. Keep the tested
 * default at 60; explicit zero disables pacing for diagnostic runs. */
static inline int mm3_frame_limit(const char *value)
{
    char *end;
    long requested;
    if (!value || !*value)
        return 60;
    errno = 0;
    requested = strtol(value, &end, 10);
    if (end == value)
        return mm3_frame_limit_rejected(value);
    while (isspace((unsigned char)*end))
        ++end;
    if (*end || errno == ERANGE || requested < 0 || requested > INT_MAX)
        return mm3_frame_limit_rejected(value);
    return (int)requested;
}

#endif

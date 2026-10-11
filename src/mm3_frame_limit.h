#ifndef MM3_FRAME_LIMIT_H
#define MM3_FRAME_LIMIT_H

#include <ctype.h>
#include <stdlib.h>

/* The title's simulation and AI are validated at 60 Hz. A launch override
 * may choose a lower limit, but must never disable or exceed this cap. */
static inline int mm3_frame_limit(const char *value)
{
    char *end;
    long requested;
    if (!value || !*value)
        return 60;
    requested = strtol(value, &end, 10);
    if (end == value)
        return 60;
    while (isspace((unsigned char)*end))
        ++end;
    if (*end || requested < 1 || requested > 60)
        return 60;
    return (int)requested;
}

#endif

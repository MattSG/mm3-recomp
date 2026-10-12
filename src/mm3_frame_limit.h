#ifndef MM3_FRAME_LIMIT_H
#define MM3_FRAME_LIMIT_H

#include <ctype.h>
#include <stdlib.h>

/* Rendering is independent of the console-rate simulation. Keep the tested
 * default at 60 and accept the upstream launcher's optional 90 FPS setting. */
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
    if (*end || requested < 1 || requested > 90)
        return 60;
    return (int)requested;
}

#endif

#include "feature_switch.h"
#include <stdlib.h>
#include <string.h>

int feature_switch_on(const char *env, int default_on, int required, int *cache)
{
    if (required)
        return 1;
    if (!*cache) {
        const char *v = getenv(env);
        *cache = (v && *v ? strcmp(v, "0") != 0 : default_on) ? 2 : 1;
    }
    return *cache == 2;
}

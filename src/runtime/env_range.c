#include "env_range.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

float recomp_env_float_in_range(const char *tag, const char *name, float fallback,
                                float lo, float hi)
{
    const char *env = getenv(name);
    char *end = NULL;
    float value;
    if (!env)
        return fallback;
    value = strtof(env, &end);
    if (end == env || *end || !isfinite(value) || value < lo || value > hi) {
        fprintf(stderr, "[%s] invalid %s; expected %g..%g, using %g\n", tag, name,
                lo, hi, fallback);
        return fallback;
    }
    return value;
}

long recomp_env_long_in_range(const char *tag, const char *name, long fallback,
                              long lo, long hi)
{
    const char *env = getenv(name);
    char *end = NULL;
    long value;
    if (!env || !*env)
        return fallback;
    value = strtol(env, &end, 10);
    if (end == env || *end || value < lo || value > hi) {
        fprintf(stderr, "[%s] invalid %s; expected %ld..%ld, using %ld\n", tag, name,
                lo, hi, fallback);
        return fallback;
    }
    return value;
}

#ifndef MM3_PERFORMANCE_PROFILE_H
#define MM3_PERFORMANCE_PROFILE_H

#include <ctype.h>
#include <stdlib.h>
#include "platform/win32_compat.h" /* _putenv_s: the CRT on Windows, compat layer elsewhere */

typedef struct MM3PerformanceProfile {
    const char *name;
    const char *render_scale;
    const char *msaa;
    const char *draw_distance;
    const char *aniso;
    const char *fps_limit;
} MM3PerformanceProfile;

static inline int mm3_profile_name_equal(const char *value, const char *name)
{
    if (!value) return 0;
    while (*value && *name) {
        if (tolower((unsigned char)*value) != tolower((unsigned char)*name))
            return 0;
        ++value;
        ++name;
    }
    return *value == *name;
}

static inline const MM3PerformanceProfile *mm3_performance_profile(const char *name)
{
    static const MM3PerformanceProfile balanced = {"Balanced", "2.0", "4", "2", "16", "60"};
    static const MM3PerformanceProfile quality = {"Quality", "3.0", "8", "2", "16", "60"};
    static const MM3PerformanceProfile low_power = {"LowPower", "1.5", "1", "1", "16", "30"};
    if (mm3_profile_name_equal(name, quality.name)) return &quality;
    if (mm3_profile_name_equal(name, low_power.name)) return &low_power;
    return &balanced;
}

static inline int mm3_performance_default(const char *key, const char *value)
{
    const char *current = getenv(key);
    if (current && *current) return 0;
    return _putenv_s(key, value);
}

/* Each independently configured setting takes priority over the profile.
 * Return the failed key so startup can report an environment allocation error. */
static inline const char *mm3_apply_performance_defaults(const char *name)
{
    const MM3PerformanceProfile *profile = mm3_performance_profile(name);
    if (mm3_performance_default("RECOMP_RENDER_SCALE", profile->render_scale))
        return "RECOMP_RENDER_SCALE";
    if (mm3_performance_default("RECOMP_MSAA", profile->msaa))
        return "RECOMP_MSAA";
    if (mm3_performance_default("RECOMP_DRAW_DISTANCE", profile->draw_distance))
        return "RECOMP_DRAW_DISTANCE";
    if (mm3_performance_default("RECOMP_ANISO", profile->aniso))
        return "RECOMP_ANISO";
    if (mm3_performance_default("RECOMP_FPS_LIMIT", profile->fps_limit))
        return "RECOMP_FPS_LIMIT";
    return NULL;
}

#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mm3_performance_profile.h"
#include "platform/frame_limit.h"

static const char *keys[] = {
    "RECOMP_RENDER_SCALE", "RECOMP_MSAA", "RECOMP_DRAW_DISTANCE",
    "RECOMP_ANISO", "RECOMP_FPS_LIMIT"
};

static void setting(const char *key, const char *value)
{
    assert(_putenv_s(key, value ? value : "") == 0);
}

static void clear_settings(void)
{
    size_t i;
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        setting(keys[i], NULL);
}

static void profile(const char *name, const char *scale, const char *msaa,
                    const char *distance, const char *fps)
{
    clear_settings();
    assert(mm3_apply_performance_defaults(name) == NULL);
    assert(strcmp(getenv("RECOMP_RENDER_SCALE"), scale) == 0);
    assert(strcmp(getenv("RECOMP_MSAA"), msaa) == 0);
    assert(strcmp(getenv("RECOMP_DRAW_DISTANCE"), distance) == 0);
    assert(strcmp(getenv("RECOMP_ANISO"), "16") == 0);
    assert(strcmp(getenv("RECOMP_FPS_LIMIT"), fps) == 0);
    assert(recomp_frame_limit_parse(getenv("RECOMP_FPS_LIMIT")) == atoi(fps));
}

int main(void)
{
    const char *names[] = {"Balanced", "Quality", "LowPower"};
    const char *overrides[] = {"2.25", "8", "1.25", "4", "45"};
    size_t i, j;
    profile(NULL, "2.0", "4", "2", "60");
    profile("", "2.0", "4", "2", "60");
    profile("unknown", "2.0", "4", "2", "60");
    profile("Balanced", "2.0", "4", "2", "60");
    profile("quality", "3.0", "8", "2", "60");
    profile("LOWPOWER", "1.5", "1", "1", "30");

    clear_settings();
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        setting(keys[i], overrides[i]);
    for (j = 0; j < sizeof(names) / sizeof(names[0]); ++j) {
        assert(mm3_apply_performance_defaults(names[j]) == NULL);
        for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
            assert(strcmp(getenv(keys[i]), overrides[i]) == 0);
    }
    setting("RECOMP_RENDER_SCALE", NULL);
    assert(mm3_apply_performance_defaults("Quality") == NULL);
    assert(strcmp(getenv("RECOMP_RENDER_SCALE"), "3.0") == 0);
    for (i = 1; i < sizeof(keys) / sizeof(keys[0]); ++i)
        assert(strcmp(getenv(keys[i]), overrides[i]) == 0);

    /* A zero-valued explicit variable is an override, not an absent setting,
     * and zero is the documented uncapped rate. */
    setting("RECOMP_FPS_LIMIT", "0");
    assert(mm3_apply_performance_defaults("LowPower") == NULL);
    assert(strcmp(getenv("RECOMP_FPS_LIMIT"), "0") == 0);
    assert(recomp_frame_limit_parse(getenv("RECOMP_FPS_LIMIT")) == 0);
    puts("Performance profile contracts passed.");
    return 0;
}

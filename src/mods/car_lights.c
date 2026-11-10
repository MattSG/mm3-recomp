/* Car lamps. sub_000CF2DA draws one lamp node of a car (ECX = node, stack +4 the lamp's transform, +12 the
 * mask of lamp kinds to draw): +0x34 the kind (0 front, 1 tail, 2 brake, 3 reverse, 4 siren, 5/6 signals,
 * 8 projector, 9 siren env), +4 the owner car, +0x54 the brightness, +0x64 the ARGB colour.
 *
 * The game draws a car's tail lamps at 0.3 to 0.5 and its brake lamps at 0.6 to 0.7; that brightness is the
 * flare's strength, so by night the rear of a car shows only a small dim dot. "Lit tail lights" raises both
 * to a floor (MM3_TAIL_GLOW, default 0.9, and MM3_BRAKE_GLOW, default 1.0, 0..1) for the call and puts the
 * game's value back; a lamp the game has switched off (brightness 0) stays off. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"
#include "mods.h"

extern void sub_000CF2DA_original(void);

/* MM3_LAMP_TRACE=1: log each lamp's state when it changes (owner, kind, brightness, colour, mask, time of day). */
static void trace_lamp(uint32_t node, uint32_t stack)
{
    static struct { uint32_t node; float brightness; uint32_t color, mask; } seen[4096];
    static unsigned lines;
    uint32_t i, mask = MEM32(stack + 12), color = MEM32(node + 0x64);
    float brightness = MEMF(node + 0x54);
    if (lines > 20000) return;
    for (i = 0; i < 4096 && seen[i].node && seen[i].node != node; ++i) {}
    if (i == 4096) return;
    if (seen[i].node == node && seen[i].brightness == brightness && seen[i].color == color && seen[i].mask == mask) return;
    seen[i].node = node; seen[i].brightness = brightness; seen[i].color = color; seen[i].mask = mask;
    fprintf(stderr, "[LAMP] node %08X owner %08X kind %u brightness %.3f color %08X mask %08X tod %u\n", node,
            MEM32(node + 4), MEM32(node + 0x34), brightness, color, mask, MEM32(0x00407C30u));
    fflush(stderr);
    ++lines;
}

static float floor_from_env(const char *name, float fallback)
{
    const char *v = getenv(name);
    char *end;
    double d = v && *v ? strtod(v, &end) : -1;
    return v && *end == 0 && d >= 0 && d <= 1 ? (float)d : fallback;
}

void sub_000CF2DA(void)
{
    static int trace = -1;
    static float tail, brake, rest;
    uint32_t node = g_ecx, stack = g_esp, kind = MEM32(node + 0x34);
    float saved = MEMF(node + 0x54);
    uint32_t saved_mask = MEM32(stack + 12);
    if (trace < 0) {
        trace = getenv("MM3_LAMP_TRACE") != NULL;
        tail = floor_from_env("MM3_TAIL_GLOW", 0.9f);
        brake = floor_from_env("MM3_BRAKE_GLOW", 1.0f);
        rest = floor_from_env("MM3_REST_GLOW", 1.0f);   /* resting brake lamps, as a share of the tail level */
    }
    if (trace) trace_lamp(node, stack);
    if ((kind == 1 || kind == 2) && saved > 0.0f && mm3_mod_on(MM3_MOD_CAR_LIGHTS)) {
        float want = kind == 1 ? tail : brake;
        if (kind == 2 && !(saved_mask & 4u)) {   /* not braking: the brake lamps glow at the tail level */
            MEM32(stack + 12) = saved_mask | 4u;
            want = tail * rest;
        }
        if (saved < want || (kind == 2 && !(saved_mask & 4u))) MEMF(node + 0x54) = want;
    }
    sub_000CF2DA_original();
    MEMF(node + 0x54) = saved;
    MEM32(stack + 12) = saved_mask;
}

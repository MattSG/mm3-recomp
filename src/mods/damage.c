/* Ported from lakshya16157/midtown-madness-3-PC-Native (GPL-3.0), adapted to this project's mod layer.
 *
 * "Damage dealt" / "Damage received" (MM3_DAMAGE_PERCENT, MM3_OWN_DAMAGE_PERCENT; 100 = the Xbox).
 * Every car's damage step (sub_001CDB6C: EDX = the car's component, +0x3C its simulation, whose +4 is
 * the contact record) adds a hit's share times the car's damage factor (+0x8718 of the component,
 * 1.0 for the Audi TT) to the nearest damage zone. That factor, scaled for the step and put back,
 * scales every crash's damage; the game's own cap on each zone still applies. Damage dealt covers
 * every car but the player's, damage received the player's own (the vehicle interface of the primary
 * player, and in split screen the second player's beside it). Only the scaling is ported: the original
 * also re-counts hits per console frame, which matters only at tick rates other than the native 30 Hz. */
#include <stdio.h>
#include <stdlib.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"
#include "mods.h"

extern void sub_001CDB6C_original(void);

static float percent_scale(const char *name, const char *what)
{
    const char *v = getenv(name);
    int percent = v && *v ? atoi(v) : 100;
    float scale = percent >= 0 && percent <= 1000 ? (float)percent / 100.0f : 1.0f;
    if (scale != 1.0f)
        fprintf(stderr, "[MM3_DAMAGE] damage %s x%.2f (Damage mod)\n", what, scale);
    return scale;
}

static float damage_scale(void)
{
    static float scale = -1.0f;
    if (scale < 0.0f) scale = percent_scale("MM3_DAMAGE_PERCENT", "dealt");
    return scale;
}

static float own_damage_scale(void)
{
    static float scale = -1.0f;
    if (scale < 0.0f) scale = percent_scale("MM3_OWN_DAMAGE_PERCENT", "received");
    return scale;
}

static int guest_object(uint32_t address)
{
    return address >= 0x10000u && address < 0x08000000u && !(address & 3u);
}

static int own_car(uint32_t car)
{
    uint32_t manager = MEM32(0x003C5CDCu), slot, i;
    if (!guest_object(manager)) return 0;
    for (i = 0; i < (MEM32(0x00407C20u) == 1u ? 2u : 1u); ++i) {
        uint32_t player, vehicle;
        if (!guest_object(slot = MEM32(manager + 0x54u + i * 4u)) ||
            !guest_object(player = MEM32(slot + 0xD8u)) ||
            !guest_object(vehicle = MEM32(player + 0x19Cu)) || MEM32(vehicle) != 0x00383768u) continue;
        if (MEM32(vehicle + 8u) == car) return 1;
    }
    return 0;
}

void sub_001CDB6C(void)
{
    uint32_t component = g_edx;
    float scale, factor;
    if (!mm3_mod_on(MM3_MOD_DAMAGE) || !guest_object(component)) {
        sub_001CDB6C_original();
        return;
    }
    scale = own_damage_scale() != damage_scale() && own_car(component) ? own_damage_scale() : damage_scale();
    if (scale == 1.0f) {
        sub_001CDB6C_original();
        return;
    }
    factor = MEMF(component + 0x8718u);
    MEMF(component + 0x8718u) = factor * scale;
    sub_001CDB6C_original();
    MEMF(component + 0x8718u) = factor;
}

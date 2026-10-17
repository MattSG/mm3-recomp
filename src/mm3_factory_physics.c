/* Optional factory engines for explicitly configured extra cars. Retail cars
 * retain the original functions. The car loader binds the constructed vehicle,
 * never the currently selected menu item. No host pointer enters guest memory.
 *
 * Native 1CBF23 applies a 0.1 gearing factor, then 1CC014/1CC172/1CC1FA
 * distribute FOUR times that value across the wheels (the complementary
 * differential weights each sum to two). 1D38EF divides each wheel torque
 * by radius; 1D4062 integrates summed force with dt/mass, without a quarter
 * correction. Factory TorqueScale is Nm, so 2.5 = 1/(0.1*4) normalizes the
 * complete native wheel path. The integration contract executes these pieces.
 * The supplied net engine curve already includes forced induction/electric
 * assistance; the donor's turbo and 5000-RPM arcade launch boost are excluded.
 * This remains MM3's tyre/contact simulation, not a full hybrid drivetrain. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#if defined(_WIN32) && !defined(MM3_FACTORY_CONTRACT)
#include <windows.h>
#endif
#include "recomp_types.h"

extern void sub_0021ED64_original(void);
extern void sub_001D0E2D_original(void);
extern void sub_001CB7E8_original(void);
extern void sub_001CB881_original(void);
extern void sub_001CB9A5_original(void);
extern void sub_001CF7B4_original(void);

#define PROFILE_LIMIT 16
#define POINT_LIMIT 16
#define BINDING_LIMIT 64
typedef struct {
    char alias[32];
    float shift, radius;
    unsigned count;
    float rpm[POINT_LIMIT], torque[POINT_LIMIT];
} FactoryProfile;
typedef struct {
    uint32_t player, bridge, controller, simulation, parameters;
    const FactoryProfile *profile;
} FactoryBinding;
static FactoryProfile profiles[PROFILE_LIMIT];
static unsigned profile_count, initialized;
static FactoryBinding bindings[BINDING_LIMIT];
/* Runtime low RAM remains committed until shutdown: HeapFree reuses blocks,
 * and NtFreeVirtualMemory decommit zeroes them (kernel_bridge.c). Cache only
 * OS-verified ranges, refresh on each vehicle construction or memory rebase.
 * Object identity is still checked on every physics call. */
static struct { uintptr_t lo, hi; } mapped_ranges[4];
#ifndef MM3_FACTORY_CONTRACT
static ptrdiff_t mapped_offset;
static unsigned mapped_next;
#endif

static int mapped(uint32_t va, size_t size)
{
    if (va < 0x10000u || va >= 0x08000000u || size > 0x08000000u - va)
        return 0;
#ifdef MM3_FACTORY_CONTRACT
    return va + size <= 0x400000u;
#else
    {
        uintptr_t p = XBOX_PTR(va), lo = 0, hi = 0;
        unsigned i;
        if (mapped_offset != g_xbox_mem_offset) {
            memset(mapped_ranges, 0, sizeof mapped_ranges);
            mapped_offset = g_xbox_mem_offset;
        }
        for (i = 0; i < 4; ++i)
            if (p >= mapped_ranges[i].lo && p + size <= mapped_ranges[i].hi) return 1;
#ifdef _WIN32
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((void *)p, &info, sizeof info) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
        lo = (uintptr_t)info.BaseAddress; hi = lo + info.RegionSize;
#else
        FILE *maps = fopen("/proc/self/maps", "r");
        char line[256], access[5];
        unsigned long long map_lo, map_hi;
        if (!maps) return 0;
        while (fgets(line, sizeof line, maps))
            if (sscanf(line, "%llx-%llx %4s", &map_lo, &map_hi, access) == 3 &&
                access[0] == 'r' && access[1] == 'w' && p >= map_lo && p + size <= map_hi) {
                lo = (uintptr_t)map_lo; hi = (uintptr_t)map_hi; break;
            }
        fclose(maps);
#endif
        if (p + size > hi) return 0;
        i = mapped_next++ % 4;
        mapped_ranges[i].lo = lo; mapped_ranges[i].hi = hi;
        return 1;
    }
#endif
}

static int alias_equal(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
    }
    return *a == *b;
}

static int allowed_alias(const char *alias)
{
    static const char *const names[] = {"Panoz", "McLaren_F1", "Form1",
        "Ford_GTcar", "Senna_Road", "Senna_GTR1", "FXXK_Racer", "Regera_GT1", "AventadorX"};
    unsigned i;
    for (i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (alias_equal(alias, names[i])) return 1;
    return 0;
}

static int next_number(char **cursor, double *value)
{
    char *end;
    *value = strtod(*cursor, &end);
    if (end == *cursor || !isfinite(*value)) return 0;
    *cursor = end;
    return 1;
}

/* Bounded line format: alias shift_rpm reference_radius count rpm fraction ... */
static int parse_profile(char *line, FactoryProfile *profile)
{
    char *p = line;
    unsigned n = 0, i;
    double value, maximum = 0.0;
    memset(profile, 0, sizeof *profile);
    while (isspace((unsigned char)*p)) ++p;
    while (*p && !isspace((unsigned char)*p)) {
        if (n == sizeof profile->alias - 1) return 0;
        profile->alias[n++] = *p++;
    }
    if (!allowed_alias(profile->alias) || !next_number(&p, &value) || value < 100 || value > 22000) return 0;
    profile->shift = (float)value;
    if (!next_number(&p, &value) || value < .1 || value > 2) return 0;
    profile->radius = (float)value;
    if (!next_number(&p, &value) || value < 2 || value > POINT_LIMIT || floor(value) != value) return 0;
    profile->count = (unsigned)value;
    for (i = 0; i < profile->count; ++i) {
        if (!next_number(&p, &value) || value < 0 || value > 22000 ||
            (i && (float)value <= profile->rpm[i - 1])) return 0;
        profile->rpm[i] = (float)value;
        if (!next_number(&p, &value) || value < 0 || value > 1) return 0;
        profile->torque[i] = (float)value;
        if (value > maximum) maximum = value;
    }
    while (isspace((unsigned char)*p)) ++p;
    return !*p && maximum >= .999 && profile->rpm[0] == 0.0f &&
        profile->shift <= profile->rpm[profile->count - 1];
}

static void load_profiles(void)
{
    FILE *file;
    char line[1536];
    unsigned lines = 0;
    if (initialized) return;
    initialized = 1;
    file = fopen("game_files/Data/mm3_factory_physics.ini", "rb");
    if (!file) return;
    while (lines++ < 128 && fgets(line, sizeof line, file)) {
        FactoryProfile profile;
        unsigned i;
        if (!strchr(line, '\n') && !feof(file)) break; /* Never parse a truncated line. */
        if (!parse_profile(line, &profile)) continue;
        for (i = 0; i < profile_count; ++i)
            if (alias_equal(profile.alias, profiles[i].alias)) break;
        if (i == profile_count && profile_count == PROFILE_LIMIT) continue;
        if (i == profile_count) ++profile_count;
        profiles[i] = profile;
    }
    fclose(file);
}

static int player_alias(uint32_t player, char alias[32])
{
    uint32_t text, length, capacity;
    if (!mapped(player, 0x1F8)) return 0;
    text = player + 0x1DC;
    length = MEM32(text + 0x14); capacity = MEM32(text + 0x18);
    if (!length || length >= 32 || capacity < length) return 0;
    text = capacity < 16 ? text + 4 : MEM32(text + 4);
    if (!mapped(text, length + 1) || MEM8(text + length)) return 0;
    memcpy(alias, (void *)XBOX_PTR(text), length);
    alias[length] = 0;
    return 1;
}

static FactoryBinding *binding_for(uint32_t controller)
{
    unsigned i;
    for (i = 0; i < BINDING_LIMIT; ++i) {
        FactoryBinding *b = &bindings[i];
        if (!b->profile || b->controller != controller) continue;
        if (mapped(b->player, 0x1A8) && mapped(b->bridge, 0xC) &&
            mapped(controller, 0x94) && mapped(b->simulation, 0x118) &&
            mapped(b->parameters, 0x68) &&
            MEM32(b->player + 0x19C) == b->bridge &&
            MEM32(b->player + 0x1A4) == b->parameters &&
            MEM32(b->bridge) == 0x00383768u && MEM32(b->bridge + 8) == controller &&
            MEM32(controller + 0x3C) == b->simulation)
            return b;
        memset(b, 0, sizeof *b); /* Address reuse must not retain an old engine. */
    }
    return NULL;
}

static void apply_gearing(FactoryBinding *b)
{
    const uint32_t sim = b->simulation;
    uint32_t count = MEM32(sim + 0x50), ratios = MEM32(sim + 0x3C), shifts = MEM32(sim + 0x40);
    double radius = MEMF(sim + 0x94), final = MEMF(b->parameters + 0x60);
    double cutoff = MEMF(sim + 0xE4), shift = b->profile->shift;
    unsigned i;
    if (!count || count > 7 || !mapped(ratios, (count + 1) * 4) || !mapped(shifts, count * 4) ||
        !isfinite(radius) || radius < .1 || radius > 2 || !isfinite(final) || final <= 0 || final > 100 ||
        !isfinite(cutoff) || cutoff <= 100 || cutoff > 22000) return;
    final *= radius / b->profile->radius;
    if (shift >= cutoff) shift = cutoff - 50.0;
    for (i = 1; i <= count; ++i)
        if (!isfinite(MEMF(ratios + i * 4)) || MEMF(ratios + i * 4) <= 0) return;
    MEMF(sim + 0x90) = (float)final;
    for (i = 0; i < count; ++i)
        MEMF(shifts + i * 4) = (float)(radius * 6.283185307179586 * 3.6 * shift /
            (60.0 * final * MEMF(ratios + (i + 1) * 4)));
}

static void bind_player(uint32_t player)
{
    char alias[32];
    unsigned i, slot = BINDING_LIMIT;
    FactoryBinding value;
    memset(&value, 0, sizeof value);
    if (!player_alias(player, alias)) return;
    load_profiles();
    for (i = 0; i < profile_count; ++i)
        if (alias_equal(alias, profiles[i].alias)) { value.profile = &profiles[i]; break; }
    if (!value.profile) return;
    value.player = player;
    value.bridge = MEM32(player + 0x19C); value.parameters = MEM32(player + 0x1A4);
    if (!mapped(value.bridge, 0xC)) return;
    value.controller = MEM32(value.bridge + 8);
    if (!mapped(value.controller, 0x94)) return;
    value.simulation = MEM32(value.controller + 0x3C);
    for (i = 0; i < BINDING_LIMIT; ++i) {
        if (bindings[i].player == player || bindings[i].controller == value.controller) {
            memset(&bindings[i], 0, sizeof bindings[i]); slot = i;
        } else if (slot == BINDING_LIMIT && !bindings[i].profile) slot = i;
    }
    if (slot == BINDING_LIMIT) {
        /* Every slot holds a live car. Refuse rather than evict one: the evicted
         * car would silently drop to retail physics while still running. */
        fprintf(stderr, "[FACTORY_PHYSICS] no free binding for alias=%s player=%08X; retail physics\n",
            alias, player);
        return;
    }
    bindings[slot] = value;
    if (binding_for(value.controller)) {
        apply_gearing(&bindings[slot]);
        fprintf(stderr, "[FACTORY_PHYSICS] alias=%s controller=%08X radius=%.6f final=%.6f gears=%u\n",
            alias, value.controller, MEMF(value.simulation + 0x94), MEMF(value.simulation + 0x90),
            MEM32(value.simulation + 0x50));
    }
}

void sub_0021ED64(void)
{
    uint32_t player = MEM32(g_esp + 4);
    unsigned i;
    memset(mapped_ranges, 0, sizeof mapped_ranges);
    for (i = 0; i < BINDING_LIMIT; ++i)
        if (bindings[i].player == player) memset(&bindings[i], 0, sizeof bindings[i]);
    sub_0021ED64_original();
    if ((g_eax & 0xFFu) != 0) bind_player(player);
}

/* Controller+88 is seeded by construction but incremented every physics
 * update at 1D47AA; it is a tick counter, not an object generation. Invalidate
 * before the actual constructor can reuse an address, including a retail car
 * constructed outside the player loader. The controller is stack argument 1. */
void sub_001D0E2D(void)
{
    uint32_t controller = MEM32(g_esp + 4);
    unsigned i;
    memset(mapped_ranges, 0, sizeof mapped_ranges);
    for (i = 0; i < BINDING_LIMIT; ++i)
        if (bindings[i].controller == controller) memset(&bindings[i], 0, sizeof bindings[i]);
    sub_001D0E2D_original();
}

void sub_001CF7B4(void)
{
    uint32_t controller = g_ebx;
    FactoryBinding *binding;
    sub_001CF7B4_original();
    binding = binding_for(controller);
    if (binding) apply_gearing(binding);
}

static double curve_sample(const FactoryProfile *profile, double rpm)
{
    unsigned i;
    if (!isfinite(rpm)) return 0.0;
    rpm = fabs(rpm);
    if (rpm >= profile->rpm[profile->count - 1]) return profile->torque[profile->count - 1];
    for (i = 1; i < profile->count; ++i)
        if (rpm < profile->rpm[i]) {
            double t = (rpm - profile->rpm[i - 1]) / (profile->rpm[i] - profile->rpm[i - 1]);
            return profile->torque[i - 1] + t * (profile->torque[i] - profile->torque[i - 1]);
        }
    return 0.0;
}

static double engine_fraction(FactoryBinding *binding)
{
    uint32_t sim = binding->simulation, engine = MEM32(sim), input = MEM32(binding->controller + 0x38);
    double rpm = MEMF(sim + 0xC), cutoff = MEMF(sim + 0xE4), maximum = MEMF(sim + 0xE0);
    if (!mapped(engine, 0x20) || !mapped(input, 0x20) || !isfinite(rpm) ||
        !isfinite(maximum) || maximum < 100 || maximum > 22000 ||
        !isfinite(cutoff) || cutoff < 100 || cutoff > maximum) return 0.0;
    if (MEMF(input + 4) > maximum) MEMF(input + 4) = (float)maximum;
    /* Preserve the retail engine-interruption timer and its throttle side effect. */
    if (MEMF(engine + 0x18) > 0.0f) {
        MEMF(sim + 0x24) = .5f;
        return 0.0;
    }
    if (fabs(rpm) >= cutoff) return 0.0;
    return curve_sample(binding->profile, rpm);
}

static void return_float(double value)
{
    g_fp_top = (g_fp_top + 7u) & 7u;
    g_fp_stack[g_fp_top] = value;
    g_esp += 4;
}

void sub_001CB7E8(void)
{
    FactoryBinding *binding = binding_for(g_ecx);
    if (!binding) { sub_001CB7E8_original(); return; }
    return_float(engine_fraction(binding));
}

void sub_001CB881(void)
{
    FactoryBinding *binding = binding_for(g_edi);
    double torque, scale, factor, condition;
    uint32_t sim;
    if (!binding) { sub_001CB881_original(); return; }
    sim = binding->simulation;
    scale = MEMF(sim + 0x8C); factor = MEMF(sim + 0x114);
    condition = MEMF(binding->controller + 0x90);
    torque = engine_fraction(binding) * scale * factor * condition * 2.5;
    if (!isfinite(torque) || scale < 0 || scale > 10000 || factor < 0 || factor > 5 ||
        condition < 0 || condition > 5) torque = 0.0;
    return_float(torque);
}

/* Native CB9A5 computes both coupled-wheel and free-rev RPM. Bound the final
 * state too: removing its caller's retail 9500-RPM curve guard must not leave
 * an unlimited free-rev accumulator or nonfinite dashboard/audio input. */
void sub_001CB9A5(void)
{
    uint32_t controller = g_esi;
    FactoryBinding *binding;
    sub_001CB9A5_original();
    binding = binding_for(controller);
    if (binding) {
        uint32_t sim = binding->simulation, input = MEM32(controller + 0x38);
        double maximum = MEMF(sim + 0xE0), rpm = MEMF(sim + 0xC);
        if (!mapped(input, 8)) return;
        if (!isfinite(maximum) || maximum < 100 || maximum > 22000 || !isfinite(rpm)) rpm = 0;
        else if (rpm > maximum) rpm = maximum;
        else if (rpm < -maximum) rpm = -maximum;
        MEMF(sim + 0xC) = MEMF(input + 4) = (float)rpm;
    }
}

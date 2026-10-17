/* Production hooks exercised against bounded guest memory and ABI stubs.
 * No game, user save, or generated source is modified by this contract. */
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;

#ifndef MM3_FACTORY_WINDOWS_MAPPING
#define MM3_FACTORY_CONTRACT 1
#endif
#include "../src/mm3_factory_physics.c"

/* Extracted native gearing, wheel-force conversion and body integration. */
#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#include "production_wheel_path.inc"
#undef eax
#undef ecx
#undef edx
#undef esp
#undef ebx
#undef esi
#undef edi

static unsigned char memory[0x400000];
enum { PLAYER=0x10000, BRIDGE=0x11000, CONTROLLER=0x12000, SIM=0x13000,
    PARAMS=0x14000, ENGINE=0x15000, DRIVER_INPUT=0x16000, RATIOS=0x17000,
    SHIFTS=0x18000, STACK=0x19000 };
static unsigned original_curve, original_torque, original_builder, original_constructor, original_rpm;
static unsigned original_controller_constructor;
static unsigned construction_success = 1;
static float next_rpm;
static const uint32_t GUARD = 0x7FA55AA5u;

void sub_0021ED64_original(void)
{
    ++original_constructor;
    assert(MEM32(g_esp + 4) == PLAYER);
    g_eax = 0xAABBCC00 | construction_success;
    g_esp += 8;
}
void sub_001D0E2D_original(void)
{
    ++original_controller_constructor;
    assert(MEM32(g_esp + 4) == CONTROLLER);
    assert(!binding_for(CONTROLLER)); /* Must clear before any original writes. */
    g_eax = CONTROLLER;
    g_esp += 24;
}
void sub_001CB7E8_original(void) { ++original_curve; return_float(-42.0); g_eax = 0xFACE; }
void sub_001CB881_original(void) { ++original_torque; return_float(-84.0); g_eax = 0xCAFE; }
void sub_001CF7B4_original(void) { ++original_builder; g_eax = 0x1234; g_esp += 8; }
void sub_001CB9A5_original(void)
{
    ++original_rpm; MEMF(SIM + 0xC) = next_rpm; MEMF(DRIVER_INPUT + 4) = next_rpm;
    g_eax = 0x5678; g_esp += 16;
}

static void registers(void)
{
    unsigned i;
    g_eax = 0xAAAA; g_edx = 0xBBBB; g_ebp = 0xCCCC; g_seh_ebp = 0xDDDD;
    g_esi = 0xEEEE; g_ebx = CONTROLLER; g_ecx = CONTROLLER; g_edi = CONTROLLER;
    g_esp = STACK; g_fp_top = 3; g_fp_cc = 0x4000; g_fp_control_word = 0x37F;
    for (i = 0; i < 8; ++i) g_fp_stack[i] = 1000.0 + i;
}

static void alias(const char *name)
{
    size_t size = strlen(name);
    assert(size < 16);
    memset(memory + PLAYER + 0x1DC, 0, 28);
    memcpy(memory + PLAYER + 0x1E0, name, size + 1);
    MEM32(PLAYER + 0x1F0) = (uint32_t)size;
    MEM32(PLAYER + 0x1F4) = 15;
}

static void fixture(unsigned gears)
{
    unsigned i;
    memset(memory, 0, sizeof memory);
    memset(bindings, 0, sizeof bindings);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    MEM32(PLAYER + 0x19C) = BRIDGE; MEM32(PLAYER + 0x1A4) = PARAMS;
    MEM32(BRIDGE) = 0x383768; MEM32(BRIDGE + 8) = CONTROLLER;
    MEM32(CONTROLLER + 0x38) = DRIVER_INPUT; MEM32(CONTROLLER + 0x3C) = SIM;
    MEM32(CONTROLLER + 0x88) = 77; MEMF(CONTROLLER + 0x90) = 1;
    MEM32(SIM) = ENGINE; MEM32(SIM + 0x3C) = RATIOS; MEM32(SIM + 0x40) = SHIFTS;
    MEM32(SIM + 0x50) = gears; MEMF(SIM + 0x94) = .392290831f;
    MEMF(SIM + 0x8C) = 350; MEMF(SIM + 0x114) = 1;
    MEMF(SIM + 0x90) = MEMF(PARAMS + 0x60) = 3.5f;
    MEMF(SIM + 0xE0) = 20000; MEMF(SIM + 0xE4) = 19000;
    MEMF(SIM + 0xCC) = 14500; MEMF(SIM + 0xD0) = 14501;
    MEMF(RATIOS) = -3;
    for (i = 1; i <= gears; ++i) MEMF(RATIOS + i * 4) = 4.0f / i;
    MEM32(RATIOS - 4) = MEM32(RATIOS + 4 * (gears + 1)) = GUARD;
    MEM32(SHIFTS - 4) = MEM32(SHIFTS + 4 * gears) = GUARD;
    alias("Form1"); registers();
}

static void construct(void)
{
    registers(); MEM32(STACK + 4) = PLAYER;
    sub_0021ED64();
    assert(g_esp == STACK + 8 && g_eax == (0xAABBCC00 | construction_success));
    assert(g_esi == 0xEEEE && g_fp_top == 3);
}

static double sample(float rpm, int torque)
{
    unsigned i;
    registers(); MEMF(SIM + 0xC) = rpm;
    if (torque) sub_001CB881(); else sub_001CB7E8();
    assert(g_esp == STACK + 4 && g_fp_top == 2);
    assert(g_eax == 0xAAAA && g_edx == 0xBBBB && g_ecx == CONTROLLER);
    assert(g_ebx == CONTROLLER && g_edi == CONTROLLER && g_esi == 0xEEEE);
    assert(g_ebp == 0xCCCC && g_seh_ebp == 0xDDDD && g_fp_control_word == 0x37F);
    for (i = 0; i < 8; ++i) if (i != 2) assert(g_fp_stack[i] == 1000.0 + i);
    return g_fp_stack[2];
}

static void parser_contract(void)
{
    FactoryProfile profile;
    char good[] = "Form1 18000 .33 4 0 .2 14000 1 18000 .9 19000 .8";
    char bad_count[] = "Form1 18000 .33 17";
    char bad_nan[] = "Form1 nan .33 2 0 1 19000 .8";
    char bad_order[] = "Form1 18000 .33 3 0 .2 19000 1 18000 .8";
    char bad_precision[] = "Form1 18000 .33 4 0 .2 18000 1 18000.00000001 .8 19000 .7";
    char bad_alias[] = "Viper 18000 .33 2 0 1 19000 .8";
    char retired_alias[] = "MRly 18000 .33 2 0 1 19000 .8";
    char bad_fraction[] = "Form1 18000 .33 2 0 1.01 19000 .8";
    char bad_shift[] = "Form1 20000 .33 2 0 1 19000 .8";
    char bad_first[] = "Form1 18000 .33 2 100 1 19000 .8";
    assert(parse_profile(good, &profile));
    assert(!parse_profile(bad_count, &profile)); assert(!parse_profile(bad_nan, &profile));
    assert(!parse_profile(bad_order, &profile)); assert(!parse_profile(bad_precision, &profile));
    assert(!parse_profile(bad_alias, &profile)); assert(!parse_profile(bad_fraction, &profile));
    assert(!parse_profile(retired_alias, &profile));
    assert(!parse_profile(bad_shift, &profile)); assert(!parse_profile(bad_first, &profile));
    load_profiles();
    assert(profile_count == 1 && initialized);
    assert(profiles[0].shift == 18000 && profiles[0].count == 4);
    puts("PASS bounded profile parser / explicit installed opt-in");
}

static void torque_contract(void)
{
    fixture(7); construct();
    assert(binding_for(CONTROLLER));
    assert(fabs(sample(16000, 0) - .95) < .000001);
    assert(fabs(sample(18000, 1) - 787.5) < .001);
    assert(fabs(sample(-18000, 1) - 787.5) < .001);
    MEMF(SIM + 0x114) = .8f; MEMF(CONTROLLER + 0x90) = .75f;
    assert(fabs(sample(14000, 1) - 525) < .001);
    assert(sample(19000, 1) == 0 && sample(22000, 0) == 0);
    assert(sample(NAN, 1) == 0);
    /* Timer zero must permit torque; positive timer retains the stock throttle
     * interruption. No inherited launch or turbo multiplier is applied. */
    MEMF(ENGINE + 0x18) = .1f; MEMF(SIM + 0x24) = 1;
    assert(sample(14000, 1) == 0 && MEMF(SIM + 0x24) == .5f);
    MEMF(ENGINE + 0x18) = 0; MEMF(SIM + 0xAC) = 100;
    assert(fabs(sample(14000, 1) - 525) < .001);
    assert(sample(0, 1) > 0); /* Single-speed launch approximation has net torque. */
    MEMF(DRIVER_INPUT + 4) = 25000; sample(18000, 0); assert(MEMF(DRIVER_INPUT + 4) == 20000);
    MEMF(SIM + 0xE0) = 25000; assert(sample(18000, 1) == 0);
    puts("PASS high-RPM curve, Nm normalization, cutoff, interruption and guest ABI");
}

static void gearing_contract(unsigned count)
{
    unsigned i;
    uint32_t peak, turbo_peak, expected_final;
    float previous = 0;
    fixture(count); construct();
    peak = MEM32(SIM + 0xCC); turbo_peak = MEM32(SIM + 0xD0);
    expected_final = MEM32(SIM + 0x90);
    assert(fabs(MEMF(SIM + 0x90) - 3.5 * MEMF(SIM + 0x94) / .33) < .000001);
    for (i = 0; i < 3; ++i) {
        registers(); MEMF(SIM + 0xAC) = (float)i;
        sub_001CF7B4();
        assert(g_esp == STACK + 8 && g_fp_top == 3 && g_eax == 0x1234);
        assert(MEM32(SIM + 0x90) == expected_final); /* Idempotent, no radius compounding. */
        assert(MEM32(SIM + 0xCC) == peak && MEM32(SIM + 0xD0) == turbo_peak);
    }
    for (i = 0; i < count; ++i) {
        double expected = .33 * 6.283185307179586 * 3.6 * 18000 /
            (60 * 3.5 * MEMF(RATIOS + 4 * (i + 1)));
        assert(fabs(MEMF(SHIFTS + 4 * i) - expected) < .001);
        assert(MEMF(SHIFTS + 4 * i) > previous); previous = MEMF(SHIFTS + 4 * i);
    }
    assert(MEM32(RATIOS - 4) == GUARD && MEM32(RATIOS + 4 * (count + 1)) == GUARD);
    assert(MEM32(SHIFTS - 4) == GUARD && MEM32(SHIFTS + 4 * count) == GUARD);
    MEMF(SIM + 0xE4) = 17000; registers(); sub_001CF7B4();
    assert(MEMF(SHIFTS) < previous); /* Edited cutoff bounds the shift below fuel cut. */
    MEMF(SIM + 0xE4) = NAN; previous = MEMF(SHIFTS); registers(); sub_001CF7B4();
    assert(MEMF(SHIFTS) == previous);
    printf("PASS %u gears, geometry correction, guard words and peak-field preservation\n", count);
}

static void lifecycle_contract(void)
{
    unsigned i;
    fixture(7); construct();
    for (i = 0; i < 600; ++i) {
        registers(); g_esi = CONTROLLER; native_physics_tick();
        assert(MEM32(CONTROLLER + 0x88) == 78 + i);
        assert(binding_for(CONTROLLER));
        assert(fabs(sample(18000, 1) - 787.5) < .001);
    }
    /* At counter wrap, the object is still the same live vehicle. */
    MEM32(CONTROLLER + 0x88) = UINT32_MAX;
    registers(); g_esi = CONTROLLER; native_physics_tick();
    assert(MEM32(CONTROLLER + 0x88) == 0 && binding_for(CONTROLLER));
    /* Simulate reusing exactly the same guest addresses for a new controller:
     * clearing must not depend on pointer changes or this mutable counter. */
    registers(); MEM32(STACK + 4) = CONTROLLER; sub_001D0E2D();
    assert(original_controller_constructor == 1 && g_esp == STACK + 24);
    assert(g_eax == CONTROLLER && g_edi == CONTROLLER && g_fp_top == 3);
    registers(); sub_001CB881();
    assert(original_torque == 1 && g_fp_stack[g_fp_top] == -84 && g_eax == 0xCAFE);
    assert(!binding_for(CONTROLLER));
    construct(); assert(binding_for(CONTROLLER));
    alias("Viper"); construct(); assert(!binding_for(CONTROLLER));
    registers(); sub_001CB7E8();
    assert(original_curve == 1 && g_fp_stack[g_fp_top] == -42 && g_eax == 0xFACE);
    alias("Form1"); construction_success = 0; construct();
    assert(!binding_for(CONTROLLER)); construction_success = 1;
    construct(); MEM32(PLAYER + 0x19C) = 0; assert(!binding_for(CONTROLLER));
    puts("PASS 601 native physics ticks, counter wrap, constructor reuse and stock fallback");
}

static void binding_limit_contract(void)
{
    /* A full table of live cars must refuse the next car, not evict one. */
    FactoryBinding before[BINDING_LIMIT];
    unsigned i;
    fixture(7);
    load_profiles();
    assert(profile_count == 1);
    for (i = 0; i < BINDING_LIMIT; ++i) {
        memset(&bindings[i], 0, sizeof bindings[i]);
        bindings[i].player = 0x30000u + i;
        bindings[i].controller = 0x40000u + i;
        bindings[i].profile = &profiles[0];
    }
    memcpy(before, bindings, sizeof before);
    bind_player(PLAYER);
    assert(!memcmp(before, bindings, sizeof before));
    assert(!binding_for(CONTROLLER));
    puts("PASS full binding table refuses a new car without evicting a live one");
}

static void rpm_contract(void)
{
    unsigned i;
    const float values[] = {18000, 21000, -21000, NAN};
    const float expected[] = {18000, 20000, -20000, 0};
    fixture(1); construct();
    for (i = 0; i < 4; ++i) {
        registers(); g_esi = CONTROLLER; next_rpm = values[i]; sub_001CB9A5();
        assert(g_esp == STACK + 16 && g_fp_top == 3 && g_eax == 0x5678);
        assert(MEMF(SIM + 0xC) == expected[i] && MEMF(DRIVER_INPUT + 4) == expected[i]);
    }
    alias("Viper"); construct(); registers(); g_esi = CONTROLLER;
    next_rpm = 23000; sub_001CB9A5(); assert(MEMF(SIM + 0xC) == 23000);
    puts("PASS computed/free-rev RPM bounds and untouched stock RPM path");
}

static void wheel_path_contract(void)
{
    const uint32_t wheel_table = 0x1A000, wheel_base = 0x1B000;
    const uint32_t differential = 0x1C000, body = 0x1D000, frame = 0x20000;
    unsigned gears, gear, drive, speed, i, cases = 0;
    for (gears = 1; gears <= 7; gears += 6)
    for (gear = 0; gear <= gears; ++gear)
    for (drive = 0; drive <= 2; ++drive)
    for (speed = 25; speed <= 100; speed += 75) {
        double native_torque, sum_torque = 0, sum_force = 0, expected_torque, expected_force;
        fixture(gears); construct(); native_constants();
        MEM32(CONTROLLER + 0x40) = wheel_table; MEM32(CONTROLLER + 0x44) = 4;
        MEM32(CONTROLLER + 0x34) = differential; MEM32(SIM + 4) = body;
        MEM32(SIM + 0x2C) = gear; MEM32(SIM + 0x58) = drive;
        MEMF(SIM + 8) = (float)speed; MEMF(SIM + 0x24) = 1;
        MEMF(differential + 8) = 1; MEM8(0x3AD094) = 1;
        for (i = 0; i < 4; ++i) MEM32(wheel_table + i * 4) = wheel_base + i * 0x100;
        MEMF(body + 0x10) = 1500; MEMF(body + 0x14) = 1.0f / 1500.0f;
        MEMF(body + 0x28) = 10;
        native_torque = sample(18000, 1);
        /* The source slice begins immediately after the real engine hook
         * returns ST(0). It retains the native .1 and every drive split. */
        g_ebp = frame; g_esi = CONTROLLER; g_fp_top = 3;
        g_fp_stack[3] = native_torque;
        MEMF(frame - 28) = MEMF(frame - 24) = MEMF(frame - 8) = 1;
        native_distribute();
        assert(g_fp_top == 4);
        expected_torque = 315.0 * MEMF(RATIOS + gear * 4) * MEMF(SIM + 0x90);
        for (i = 0; i < 4; ++i) {
            float wheel_torque = MEMF(wheel_base + i * 0x100 + 8);
            sum_torque += wheel_torque;
            g_ebp = frame; g_ebx = CONTROLLER; g_ecx = frame - 268;
            g_fp_top = 3; g_fp_stack[3] = wheel_torque;
            native_wheel_force();
            sum_force += MEMF(frame + 0x54);
            /* Execute the real per-wheel body-force accumulator. */
            MEMF(frame + 0x18) = MEMF(frame + 0x1C) = 0;
            MEMF(frame + 0x20) = MEMF(frame + 0x54);
            g_fp_top = 3; g_fp_stack[3] = MEMF(frame + 0x40);
            native_accumulate_force();
        }
        assert(fabs(sum_torque - expected_torque) < .005);
        expected_force = 315.0 * MEMF(RATIOS + gear * 4) * 3.5 / .33;
        assert(fabs(sum_force - expected_force) < .02);
        assert(fabs(MEMF(frame + 0x48) - sum_force) < .002);
        /* Native timestep*inverse-mass preparation and velocity integration,
         * including the exact live field offsets, with no invented scaling. */
        MEMF(frame + 0x7C) = 1.0f / 60.0f;
        g_ecx = body; native_dt_inverse_mass();
        g_ebx = CONTROLLER; g_fp_top = 3; native_integrate_velocity();
        assert(fabs(MEMF(body + 0x28) - (10 + expected_force / 1500 / 60)) < .00001);
        ++cases;
    }
    printf("PASS %u extracted native forward/reverse, FWD/RWD/AWD wheel-force/body cases\n", cases);
}

#ifdef MM3_FACTORY_DEFAULTS
static void default_driving_contract(void)
{
    const char *aliases[] = {"Panoz", "McLaren_F1", "Form1", "Ford_GTcar",
        "Senna_Road", "Senna_GTR1", "FXXK_Racer", "Regera_GT1", "AventadorX"};
    unsigned i;
    assert(initialized && profile_count == 0);
    for (i = 0; i < sizeof aliases / sizeof aliases[0]; ++i) {
        unsigned char before[0x200];
        unsigned curve = original_curve, torque = original_torque;
        unsigned builder = original_builder, rpm = original_rpm;
        fixture(!strcmp(aliases[i], "Form1") ? 5 : 6);
        alias(aliases[i]);
        MEMF(SIM + 0x90) = MEMF(PARAMS + 0x60) = 4;
        MEMF(SIM + 0xCC) = 6500; MEMF(SIM + 0xD0) = 6750;
        MEMF(SIM + 0xE0) = 10000; MEMF(SIM + 0xE4) = 8000;
        memcpy(before, memory + SIM, sizeof before);
        construct();
        assert(!binding_for(CONTROLLER));
        registers(); sub_001CF7B4();
        assert(original_builder == builder + 1 && g_eax == 0x1234);
        assert(!memcmp(before, memory + SIM, sizeof before));
        registers(); sub_001CB7E8();
        assert(original_curve == curve + 1 && g_fp_stack[g_fp_top] == -42);
        registers(); sub_001CB881();
        assert(original_torque == torque + 1 && g_fp_stack[g_fp_top] == -84);
        registers(); next_rpm = 7000; g_esi = CONTROLLER; sub_001CB9A5();
        assert(original_rpm == rpm + 1 && MEMF(SIM + 0xC) == 7000);
        printf("PASS default %s: retail gearing, engine curve, torque and RPM paths\n", aliases[i]);
    }
}
#endif

int main(void)
{
#ifdef MM3_FACTORY_DEFAULTS
    load_profiles();
    default_driving_contract();
    return 0;
#else
    parser_contract(); torque_contract(); gearing_contract(1); gearing_contract(7);
    lifecycle_contract(); binding_limit_contract(); rpm_contract(); wheel_path_contract();
#ifdef MM3_FACTORY_WINDOWS_MAPPING
    {
        LARGE_INTEGER frequency, start, end;
        unsigned i;
        fixture(7); construct(); MEMF(SIM + 0xC) = 18000;
        QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
        for (i = 0; i < 100000; ++i) {
            g_edi = CONTROLLER; g_esp = STACK; g_fp_top = 0;
            sub_001CB881();
        }
        QueryPerformanceCounter(&end);
        printf("Production Windows mapping validation: %.3f us/torque call\n",
            (end.QuadPart - start.QuadPart) * 1e6 / frequency.QuadPart / 100000);
    }
#endif
    puts("Factory physics integration contracts passed.");
    return 0;
#endif
}

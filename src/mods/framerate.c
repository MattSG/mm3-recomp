#include <stdlib.h>
#include <stdio.h>
#include "recomp_types.h"
#include "mods.h"

/* The console-rate mod: the game's simulation runs at the console's rate,
 * whatever the frame rate. MM3 presents with D3DPRESENT_INTERVAL_TWO_OR_
 * IMMEDIATE at 60 Hz (0x1ECB..: +0x34 = 0x80000002), so on the Xbox it ran
 * at 30 fps: the world updated 30 times a second, each update taking four
 * 1/120 s physics steps. MM3_SIM_HZ picks the rate: 30 (the console's, the
 * default) or 60.
 *
 * The main loop (0x1E77F3, ebx the game object) enters its initial state
 * through 0x25422 once. Its persistent loop starts at 0x1E7849: it runs
 * the state machine (0x2545A), updates the timer through 0x1E6C87 (dt, a
 * double at game+0x30), averages dt into game+0x54, then updates the world
 * through 0x1E7590 (scripts, cars, their AI, physics in
 * fixed 1/120 s substeps, traffic) and renders. All of that took the frame's
 * time and not all of it scales with it: above 60 fps the cars accelerated
 * slower, and above ~120 fps the race AI reversed off the start line
 * (Test-MM3AiStart.ps1).
 *
 * Here a clock decides, once a frame, how many whole ticks have
 * passed, using the current timer value immediately before the world update.
 * The world runs that many times
 * with every frame-time field set to exactly one tick, while input, menus and rendering
 * still run every frame, with the real dt back for rendering. A frame with
 * less than a tick gone renders the last state; a slow frame catches up (at
 * most 4 ticks, past that the time is dropped, as a hitch). Every frame rate
 * then plays the simulation the console does. */
extern void sub_00025422_original(void);
extern void sub_0002545A_original(void);
extern void sub_001E7590_original(void);

#define MAX_TICKS 4

/* One tick: 1/30 s (the console) or 1/60 s (MM3_SIM_HZ=60). */
static float tick(void)
{
    static float t;
    if (!t) {
        const char *v = getenv("MM3_SIM_HZ");
        t = v && atoi(v) == 60 ? 1.0f / 60.0f : 1.0f / 30.0f;
    }
    return t;
}
#define TICK tick()

/* Optional rate oracle for a real gameplay run. The elapsed value comes from
 * the freshly sampled guest performance-counter timer; world time counts
 * completed original world calls. The independent cumulative game+0x38 timer
 * can reset while loading a race: compare guest/world deltas within a warm
 * gameplay epoch, accounting for dropped hitch time. Normal launches do not
 * emit this log. */
static void trace_ticks(uint32_t game, double elapsed, double dropped, int ticks)
{
    static int initialized, enabled;
    static unsigned long long frames, total_ticks;
    static double total_elapsed, total_dropped, guest_origin, next_report = 1.0;
    if (!initialized) {
        const char *value = getenv("MM3_SIM_TRACE");
        enabled = value && value[0] && value[0] != '0';
        guest_origin = MEMD(game + 0x38) - elapsed;
        initialized = 1;
    }
    if (!enabled) return;
    ++frames;
    total_ticks += ticks;
    total_elapsed += elapsed;
    total_dropped += dropped;
    if (total_elapsed >= next_report) {
        double world = (double)total_ticks * TICK;
        fprintf(stderr, "[SIM_TRACE] hz=%d frames=%llu ticks=%llu elapsed=%.6f guest=%.6f world=%.6f dropped=%.6f lag=%.6f\n",
                (int)(1.0f / TICK + 0.5f), frames, total_ticks,
                total_elapsed, MEMD(game + 0x38) - guest_origin,
                world, total_dropped, total_elapsed - world - total_dropped);
        /* Redirected stderr is buffered; preserve rate evidence even when a
         * smoke harness terminates the process before normal shutdown. */
        fflush(stderr);
        next_report = total_elapsed + 1.0;
    }
}

/* Call `fn` `n` times as the main loop called it once: the guest stack's
 * return address and `nargs` arguments back in place each time, and the
 * registers the main loop keeps. ret_pop is what the callee pops. */
static void repeat(void (*fn)(void), int n, int nargs, uint32_t ret_pop)
{
    uint32_t esp0 = g_esp, saved[4], eax = g_eax, ecx = g_ecx, edx = g_edx;
    uint32_t ebx = g_ebx, esi = g_esi, edi = g_edi, ebp = g_ebp;
    int i, a;
    for (a = 0; a <= nargs; a++)
        saved[a] = MEM32(esp0 + 4u * a);
    for (i = 0; i < n; i++) {
        g_esp = esp0;
        for (a = 0; a <= nargs; a++)
            MEM32(esp0 + 4u * a) = saved[a];
        g_eax = eax; g_ecx = ecx; g_edx = edx;
        g_ebx = ebx; g_esi = esi; g_edi = edi; g_ebp = ebp;
        fn();
    }
    if (n == 0) { g_eax = eax; g_ebx = ebx; g_esi = esi; g_edi = edi; g_ebp = ebp; }
    g_esp = esp0 + ret_pop;
}

/* State changes are not a simulation clock: the main-loop call to this
 * helper is startup-only, and the state machine also calls it for transitions.
 * Preserve both the initial entry and subsequent nested transitions exactly. */
void sub_00025422(void)
{
    sub_00025422_original();
}

/* The state machine drives menu/input and state transitions each render
 * frame, before the current timer is sampled. Keep its original cadence. */
void sub_0002545A(void)
{
    sub_0002545A_original();
}

/* The world update (edi the game, one argument, ret 4), with the console's
 * step in all three frame-time fields and their real values back afterwards
 * for rendering. */
void sub_001E7590(void)
{
    static double clock;
    uint32_t game = g_edi;
    float dt = MEMF(game + 0x54);
    int ticks;
    if (!mm3_mod_on(MM3_MOD_FRAMERATE) || MEM32(g_esp) != 0x001E7895u) {
        sub_001E7590_original();
        return;
    }
    /* Every copy of the frame time the update can read: the timer's double
     * (game+0x30), its clamped float (+0x4C) and the averaged step (+0x54). */
    double raw = MEMD(game + 0x30);
    float clamped = MEMF(game + 0x4C);
    double elapsed = raw;
    double dropped = 0.0;
    if (!isfinite(elapsed) || elapsed < 0.0) elapsed = 0.0;
    clock += elapsed;
    /* Bound before converting to int, including arbitrarily long hitches.
     * Zero/backward/invalid deltas never manufacture simulation time. */
    if (clock >= (double)(MAX_TICKS + 1) * TICK) {
        ticks = MAX_TICKS;
        dropped = clock - (double)ticks * TICK;
        clock = 0.0;
    } else {
        ticks = (int)(clock / TICK);
        clock -= (double)ticks * TICK;
    }
    MEMD(game + 0x30) = TICK;
    MEMF(game + 0x4C) = TICK;
    MEMF(game + 0x54) = TICK;
    repeat(sub_001E7590_original, ticks, 1, 8);
    MEMD(game + 0x30) = raw;
    MEMF(game + 0x4C) = clamped;
    MEMF(game + 0x54) = dt;
    trace_ticks(game, elapsed, dropped, ticks);
}

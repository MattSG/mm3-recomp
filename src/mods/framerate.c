#include <stdlib.h>
#include <windows.h>
#include "recomp_types.h"
#include "nv2a_d3d11.h"
#include "mods.h"

/* The console-rate mod: the game's simulation runs at the console's rate,
 * whatever the frame rate. MM3 presents with D3DPRESENT_INTERVAL_TWO_OR_
 * IMMEDIATE at 60 Hz (0x1ECB..: +0x34 = 0x80000002), so on the Xbox it ran
 * at 30 fps: the world updated 30 times a second, each update taking four
 * 1/120 s physics steps. MM3_SIM_HZ picks the rate: 30 (the console's),
 * 60 (the default) or 90, each at least one physics step per update. At
 * 120 the player car covers ~5% less ground (302 vs 311-336 units in 12 s,
 * Test-MM3AiStart.ps1), likely from one-step ticks rounding.
 *
 * The main loop (0x1E77F3, ebx the game object) sets up once, then each
 * frame (0x1E7849..0x1E792F) runs the state machine (0x2545A), times the
 * frame and averages it (0x1E6C87: dt, a double at game+0x30, clamped into
 * +0x4C, averaged into +0x54), updates the world through 0x1E7590 (scripts,
 * the AI module, cars, physics in fixed 1/120 s substeps, traffic) and
 * renders. The world took the frame's time and not all of it scales with
 * it: at 144 fps the AI module stepped in 1/144 s against the cars' physics
 * and the race AI reversed off the start line (Test-MM3AiStart.ps1).
 *
 * Here a clock fed by the game's own frame time decides, once a frame, how
 * many whole ticks have passed, and the world update runs that many times
 * with every copy of the frame time it reads set to exactly one tick; the
 * state machine and input still run every frame. A frame with less than a
 * tick gone shows the last image again (sub_001EC8E6); a slow frame
 * catches up (at most 4 ticks, past that the time is dropped, as a hitch).
 * Every frame rate then plays the same simulation. */
extern void sub_001E7590_original(void);

#define MAX_TICKS 4

/* One tick: 1/MM3_SIM_HZ s. */
static float tick(void)
{
    static float t;
    if (!t) {
        const char *v = getenv("MM3_SIM_HZ");
        int hz = v && *v ? atoi(v) : 60;
        t = 1.0f / (float)(hz == 30 || hz == 90 ? hz : 60);
    }
    return t;
}
#define TICK tick()

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

/* Whole ticks in this frame's dt, the remainder carried to the next. */
static int ticks_for(float dt)
{
    static float clock;
    int n;
    if (!(dt > 0.0f) || dt > 1.0f) dt = TICK;   /* loading, a reset timer */
    clock += dt;
    n = (int)(clock / TICK);
    if (n > MAX_TICKS) {
        n = MAX_TICKS;
        clock = (float)MAX_TICKS * TICK;
    }
    clock -= (float)n * TICK;
    return n;
}

/* The race AI's update (0x22DBBF: ecx the AI module, dt and a flag, ret 8).
 * Given each tick, the AI drives differently at 60 or 90 ticks than at 30
 * -- its cars covered ~9% more ground in 25 s (Test-MM3AiStart.ps1
 * -Course) -- so it keeps the console's rate: every second or third tick,
 * with that many ticks' time. Cars and their physics still step every tick. */
extern void sub_0022DBBF_original(void);
void sub_0022DBBF(void)
{
    static int skipped;
    int every = (int)(1.0f / 30.0f / TICK + 0.5f);
    if (every <= 1 || !mm3_mod_on(MM3_MOD_FRAMERATE)) {
        sub_0022DBBF_original();
        return;
    }
    if (++skipped < every) {
        g_esp += 12;
        return;
    }
    skipped = 0;
    MEMF(g_esp + 4) *= (float)every;
    sub_0022DBBF_original();
}

/* Ticks the last world update ran; -1 before the first. */
static int s_ticks = -1;

/* The world update (edi the game, one argument, ret 4), with the console's
 * step in every frame-time field and the real ones back afterwards for
 * rendering. */
void sub_001E7590(void)
{
    uint32_t game = g_edi;
    if (!mm3_mod_on(MM3_MOD_FRAMERATE) || MEM32(g_esp) != 0x001E7895u) {
        sub_001E7590_original();
        return;
    }
    double raw = MEMD(game + 0x30);       /* the timer's double */
    float clamped = MEMF(game + 0x4C);    /* its clamped float */
    float dt = MEMF(game + 0x54);         /* the averaged step */
    int n = s_ticks = ticks_for((float)raw);
    MEMD(game + 0x30) = TICK;
    MEMF(game + 0x4C) = TICK;
    MEMF(game + 0x54) = TICK;
    repeat(sub_001E7590_original, n, 1, 8);
    MEMD(game + 0x30) = raw;
    MEMF(game + 0x4C) = clamped;
    MEMF(game + 0x54) = dt;
}

/* The render (two arguments, ret 8). A frame the world did not tick has
 * nothing new to draw, and drawn again it comes out wrong (menu text cut
 * short, tail lights blinking: state the world update leaves for one
 * render). So it is not drawn: the last frame is shown again, paced by the
 * frame limit like any frame, and every frame rate stays one. */
extern void sub_001EC8E6_original(void);
void sub_001EC8E6(void)
{
    if (s_ticks == 0 && MEM32(g_esp) == 0x001E78AAu && mm3_mod_on(MM3_MOD_FRAMERATE)) {
        g_esp += 12;
        nv2a_d3d_repeat_frame();
        return;
    }
    sub_001EC8E6_original();
}

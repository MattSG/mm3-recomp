#include <stdlib.h>
#include "recomp_types.h"
#include "mods.h"

/* The console-rate mod: the game's simulation runs at the console's rate,
 * whatever the frame rate. MM3 presents with D3DPRESENT_INTERVAL_TWO_OR_
 * IMMEDIATE at 60 Hz (0x1ECB..: +0x34 = 0x80000002), so on the Xbox it ran
 * at 30 fps: the world updated 30 times a second, each update taking four
 * 1/120 s physics steps. MM3_SIM_HZ picks the rate: 30 (the console's, the
 * default) or 60.
 *
 * The main loop (0x1E77F3, ebx the game object) runs once a frame: it times
 * the frame (dt, a double at game+0x30), runs the game's processes
 * (0x25422) and its state machine (0x2545A), averages dt into game+0x54,
 * updates the world through 0x1E7590 (scripts, cars, their AI, physics in
 * fixed 1/120 s substeps, traffic) and renders. All of that took the frame's
 * time and not all of it scales with it: above 60 fps the cars accelerated
 * slower, and above ~120 fps the race AI reversed off the start line
 * (Test-MM3AiStart.ps1).
 *
 * Here a clock decides, once a frame, how many whole ticks have
 * passed, and those three stages run that many times -- the world update
 * with game+0x54 set to exactly one tick -- while input, menus and rendering
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

static int s_ticks = 1;   /* this frame's ticks, decided at the first stage */

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

/* The game's processes: the frame's first simulation stage, so the tick
 * count is decided here. */
void sub_00025422(void)
{
    static float clock;
    if (MEM32(g_esp) != 0x001E781Du) {   /* the state machine calls it too */
        sub_00025422_original();
        return;
    }
    if (!mm3_mod_on(MM3_MOD_FRAMERATE)) {
        s_ticks = 1;
        sub_00025422_original();
        return;
    }
    {
        float dt = (float)MEMD(g_ebx + 0x30);   /* the main loop's game object */
        if (!(dt > 0.0f) || dt > 1.0f) dt = TICK;
        clock += dt;
        s_ticks = (int)(clock / TICK);
        if (s_ticks > MAX_TICKS) {
            s_ticks = MAX_TICKS;
            clock = (float)MAX_TICKS * TICK;
        }
        clock -= (float)s_ticks * TICK;
    }
    repeat(sub_00025422_original, s_ticks, 1, 8);   /* its argument: the ebx left by the previous call */
}

/* The state machine (one argument, ret 4). */
void sub_0002545A(void)
{
    if (!mm3_mod_on(MM3_MOD_FRAMERATE) || MEM32(g_esp) != 0x001E7852u) {
        sub_0002545A_original();
        return;
    }
    repeat(sub_0002545A_original, s_ticks, 1, 8);
}

/* The world update (edi the game, one argument, ret 4), with the console's
 * step in game+0x54 and the real dt back afterwards for rendering. */
void sub_001E7590(void)
{
    uint32_t game = g_edi;
    float dt = MEMF(game + 0x54);
    if (!mm3_mod_on(MM3_MOD_FRAMERATE)) {
        sub_001E7590_original();
        return;
    }
    /* Every copy of the frame time the update can read: the timer's double
     * (game+0x30), its clamped float (+0x4C) and the averaged step (+0x54). */
    double raw = MEMD(game + 0x30);
    float clamped = MEMF(game + 0x4C);
    MEMD(game + 0x30) = TICK;
    MEMF(game + 0x4C) = TICK;
    MEMF(game + 0x54) = TICK;
    repeat(sub_001E7590_original, s_ticks, 1, 8);
    MEMD(game + 0x30) = raw;
    MEMF(game + 0x4C) = clamped;
    MEMF(game + 0x54) = dt;
}


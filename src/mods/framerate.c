#include <stdlib.h>
#include <string.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"
#include "nv2a_d3d11.h"
#include "mods.h"
#include "fixed_step.h"
#include "guest_call.h"

/* The console-rate mod: the game's simulation runs at the console's rate,
 * whatever the frame rate. MM3 presents with D3DPRESENT_INTERVAL_TWO_OR_
 * IMMEDIATE at 60 Hz (0x1ECB..: +0x34 = 0x80000002), so on the Xbox it ran
 * at 30 fps: the world updated 30 times a second, each update taking four
 * 1/120 s physics steps. MM3_SIM_HZ picks the rate: 30 (the console's), 60
 * (the default) or 90. At 120 the player car covers ~5% less ground (302 vs
 * 311-336 units in 12 s, tools/scripts/test_ai_start.py), likely from one-step ticks
 * rounding.
 *
 * The main loop (0x1E77F3, ebx the game object) sets up once, then each
 * frame (0x1E7849..0x1E792F) runs the state machine (0x2545A), times the
 * frame and averages it (0x1E6C87: dt, a double at game+0x30, clamped into
 * +0x4C, averaged into +0x54), updates the world through 0x1E7590 (scripts,
 * the AI module, cars, physics in fixed 1/120 s substeps, traffic) and
 * renders. The world took the frame's time and not all of it scales with
 * it: at 144 fps the AI module stepped in 1/144 s against the cars' physics
 * and the race AI reversed off the start line (tools/scripts/test_ai_start.py).
 *
 * Here a clock fed by the game's own frame time decides, once a frame, how
 * many whole ticks have passed, and the world update runs that many times
 * with every copy of the frame time it reads set to exactly one tick; the
 * state machine and input still run every frame, and so does the render,
 * between ticks too (sub_001EC8E6). A slow frame catches up (at most 4
 * ticks, past that the time is dropped, as a hitch). Every frame rate then
 * plays the same simulation. */
extern void sub_001E7590_original(void);
extern void sub_00025422_original(void);
extern void sub_0002545A_original(void);

/* The state machine's entry and step, routed here by definition-only renames
 * (CMakeLists MM3_SIMULATION_FUNCTIONS): they keep their every-frame cadence. */
void sub_00025422(void) { sub_00025422_original(); }
void sub_0002545A(void) { sub_0002545A_original(); }

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


/* How far the clock is into the next tick, 0..1. */
static float s_alpha;

/* Whole ticks in this frame's dt, the remainder carried to the next. */
static int ticks_for(float dt)
{
    static FixedStepClock clock;
    return fixed_step_take(&clock, dt, TICK, MAX_TICKS, &s_alpha);
}

/* The race AI's update (0x22DBBF: ecx the AI module, dt and a flag, ret 8).
 * Given each tick, the AI drives differently at 60 ticks than at 30 -- its
 * cars covered ~9% more ground in 25 s (tools/scripts/test_ai_start.py --course) -- so
 * it keeps the console's rate: every second tick (third at 90), with that
 * many ticks' time. Cars and their physics still step every tick. */
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

/* Frames between ticks: drawn moving (MM3_SMOOTH=1) or the last one shown
 * again (the default). */
static int smooth(void)
{
    static int m = -1;
    if (m < 0) m = getenv("MM3_SMOOTH") && atoi(getenv("MM3_SMOOTH")) != 0;
    return m;
}

/* Ticks the last world update ran; -1 before the first and once a render
 * has used them. */
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
    xbox_guest_call_repeated(sub_001E7590_original, n, 1, 8);
    MEMD(game + 0x30) = raw;
    MEMF(game + 0x4C) = clamped;
    MEMF(game + 0x54) = dt;
}

/* The render (two arguments, ret 8). A frame with no tick has nothing new
 * to draw, and drawn again it comes out wrong (the render uses up queue
 * counters the world update leaves it: menu text cut short, tail lights
 * blinking). So the title draws only when the world ticked, and the frames
 * between are the GPU's: its last frame shown again or, with MM3_SMOOTH,
 * drawn with every vertex-program constant part way between that frame and
 * the one before (nv2a_d3d_interp_frame) -- a fixed-timestep game's
 * interpolation, done on the matrices the title gave the GPU. Shown at how
 * far the clock is into the next tick, it trails the world by one tick. */
extern void sub_001EC8E6_original(void);
#define KEEP_LO 0x003C5680u   /* the render's queue counters only */
#define KEEP_HI 0x003C57B0u
static uint32_t s_keep_pre[(KEEP_HI - KEEP_LO) / 4];
static uint32_t s_keep_post[(KEEP_HI - KEEP_LO) / 4];
static uint32_t s_keep_idx[(KEEP_HI - KEEP_LO) / 4];
static int s_keep_n, s_keep_ready;


/* A guest pointer: not put back. The render allocates and frees, and the
 * heap's lists are pointers in these globals; restoring them corrupted the
 * heap a few seconds into a race. Counters like 0x3C568C are small. */
static int ptrish(uint32_t v)
{
    return (v >= 0x00010000u && v < 0x10000000u) || (v >= 0x80000000u && v < 0x88000000u);
}

static void render_ticked(void)
{
    uint32_t n = (KEEP_HI - KEEP_LO) / 4, i;
    volatile uint32_t *g = (volatile uint32_t *)XBOX_PTR(KEEP_LO);
    for (i = 0; i < n; i++)
        s_keep_pre[i] = g[i];
    sub_001EC8E6_original();
    s_keep_n = 0;
    for (i = 0; i < n; i++)
        if (g[i] != s_keep_pre[i] &&
            !ptrish(g[i]) && !ptrish(s_keep_pre[i])) {
            s_keep_post[s_keep_n] = g[i];
            s_keep_idx[s_keep_n++] = i;
        }
    s_keep_ready = 1;
}

static void render_again(void)
{
    volatile uint32_t *g = (volatile uint32_t *)XBOX_PTR(KEEP_LO);
    /* Only words nothing else has changed since: the state machine and
     * input run between renders and keep theirs. */
    for (int k = 0; k < s_keep_n; k++)
        if (g[s_keep_idx[k]] == s_keep_post[k])
            g[s_keep_idx[k]] = s_keep_pre[s_keep_idx[k]];
    sub_001EC8E6_original();
    for (int k = 0; k < s_keep_n; k++)   /* what this render left, for the next */
        s_keep_post[k] = g[s_keep_idx[k]];
}

void sub_001EC8E6(void)
{
    if (MEM32(g_esp) == 0x001E78AAu && mm3_mod_on(MM3_MOD_FRAMERATE)) {
        /* The ticks of the world update since the last render, used once. A
         * frame with none (the post-race and briefing screens, which skip it)
         * is drawn: a stale 0 kept those showing one image -- the briefing's
         * fade frozen almost black until A -- however long they ran. */
        int ticks = s_ticks;
        s_ticks = -1;
        if (smooth()) {
            int drew = ticks != 0;
            if (drew) sub_001EC8E6_original(); else g_esp += 12;
            nv2a_d3d_interp_frame(s_alpha, drew);
            return;
        }
        /* MM3_RENDER_EVERY_FRAME=1 (launcher: Draw every frame) renders
         * between ticks too, with the queue counters a ticked render used
         * put back first. */
        static int every = -1;
        if (every < 0) every = getenv("MM3_RENDER_EVERY_FRAME") && atoi(getenv("MM3_RENDER_EVERY_FRAME")) == 1;
        if (every) {
            if (ticks == 0 && s_keep_ready) render_again(); else render_ticked();
            return;
        }
        if (ticks == 0) {               /* not drawn: the last image again */
            g_esp += 12;
            nv2a_d3d_repeat_frame();
            return;
        }
    }
    sub_001EC8E6_original();
}

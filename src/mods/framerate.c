#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp_types.h"
#include "nv2a_d3d11.h"
#include "mods.h"

/* The console-rate mod: the game's simulation runs at the console's rate,
 * whatever the frame rate. MM3 presents with D3DPRESENT_INTERVAL_TWO_OR_
 * IMMEDIATE at 60 Hz (0x1ECB..: +0x34 = 0x80000002), so on the Xbox it ran
 * at 30 fps: the world updated 30 times a second, each update taking four
 * 1/120 s physics steps. MM3_SIM_HZ picks the rate: 30 (the console's), 60
 * (the default) or 90. At 120 the player car covers ~5% less ground (302 vs
 * 311-336 units in 12 s, Test-MM3AiStart.ps1), likely from one-step ticks
 * rounding.
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

/* How far the clock is into the next tick, 0..1. */
static float s_alpha;

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
    s_alpha = clock / TICK;
    return n;
}

/* The race AI's update (0x22DBBF: ecx the AI module, dt and a flag, ret 8).
 * Given each tick, the AI drives differently at 60 ticks than at 30 -- its
 * cars covered ~9% more ground in 25 s (Test-MM3AiStart.ps1 -Course) -- so
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

/* Frames between ticks drawn moving (MM3_SMOOTH=1, experimental, not in the
 * launcher) or the last image shown again (the default). The GPU-replay
 * approach that would replace it is on branch wip/gpu-interp. Smooth blends whatever floats an update
 * changed; now and then one is really an integer or feeds a lookup, and the
 * render crashes on it (seen at 0x2413A0 and in 0x114B2A, ~1 in 5 races). */
static int smooth(void)
{
    static int m = -1;
    if (m < 0) m = getenv("MM3_SMOOTH") && atoi(getenv("MM3_SMOOTH")) != 0;
    return m;
}
static void blend_pre(void);
static void blend_post(void);

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
    if (n && smooth()) blend_pre();
    repeat(sub_001E7590_original, n, 1, 8);
    if (n && smooth()) blend_post();
    MEMD(game + 0x30) = raw;
    MEMF(game + 0x4C) = clamped;
    MEMF(game + 0x54) = dt;
}

/* The render (two arguments, ret 8). With MM3_SMOOTH it runs between ticks
 * too, and drawn again as it is a frame comes out wrong: it uses up queue
 * counters the world update leaves it (0x3C568C cut menu text short; those
 * at 0x3C5788-0x3C57A4 made tail lights blink). Those a ticked render
 * changes are put back as it found them before each render between ticks.
 * Restoring more of the globals (1600 words change per race render) put
 * list counts out of step with their lists and crashed. */
extern void sub_001EC8E6_original(void);
#define KEEP_LO 0x003C5680u   /* the render's queue counters only */
#define KEEP_HI 0x003C57B0u
static uint32_t s_keep_pre[(KEEP_HI - KEEP_LO) / 4], *s_keep_post;
static uint32_t *s_keep_idx;
static int s_keep_n;


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
    if (!s_keep_idx) {
        s_keep_idx = (uint32_t *)malloc(n * sizeof *s_keep_idx);
        s_keep_post = (uint32_t *)malloc(n * sizeof *s_keep_post);
    }
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
}

/* Frames between ticks move. Whatever floats a world
 * update changes (cars, camera, everything) are drawn part way from their
 * value before it to after it, as far as the clock has gone into the next
 * tick, and put back after the render. The pages updates write are found by
 * comparing a 1/64 slice of RAM around each update; those pages are then
 * compared around every update. Only float-like values (not pointers or
 * small integers) that moved a little (not a reset or an angle wrapping)
 * are blended. */
#define RAM_HI     0x08000000u   /* main.c maps 128 MB; the heap reaches past 64 */
#define PAGE_N     (RAM_HI >> 12)
#define SLICES     64
#define BLEND_MAX  262144
typedef struct { uint32_t a; float prev, cur, shown; } Blend;
static uint32_t *s_shadow, *s_post;   /* before this update; after the last */
static uint8_t s_page_state[PAGE_N];   /* 0 unknown, 1 readable, 2 not */
static uint8_t s_cand[PAGE_N];
static uint32_t s_cand_list[PAGE_N], s_cand_n, s_slice;
static Blend *s_blend;
static int s_blend_n;

static int page_readable(uint32_t p)
{
    if (!s_page_state[p]) {
        MEMORY_BASIC_INFORMATION mi;
        s_page_state[p] = VirtualQuery((void *)XBOX_PTR(p << 12), &mi, sizeof mi) &&
                          mi.State == MEM_COMMIT && !(mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ? 1 : 2;
    }
    return s_page_state[p] == 1;
}

static int floaty(uint32_t v)
{
    uint32_t e = v >> 23 & 0xFF;
    return v == 0 || (e >= 0x60 && e < 0x8F);   /* under 65536: ints read as floats are larger */
}

/* In a race: the primary player has a car (DEBUG_TELEPORT.md's path).
 * Menus and loads are left alone; other threads work on them meanwhile. */
static int in_race(void)
{
    static int since;     /* ticks the car has existed: loading ends first */
    uint32_t g = MEM32(0x003C5CDCu), p = g ? MEM32(g + 0x54) : 0;
    p = p ? MEM32(p + 0xD8) : 0;
    if (!(p && MEM32(p + 0x19C))) { since = 0; return 0; }
    return ++since > 240;  /* counted twice a tick: 2-4 s */
}

/* Before a world update: copy what it may write. */
static void blend_pre(void)
{
    uint32_t p, lo, hi;
    if (!in_race()) { s_blend_n = 0; return; }
    if (!s_shadow) {
        s_shadow = (uint32_t *)VirtualAlloc(NULL, RAM_HI, MEM_COMMIT, PAGE_READWRITE);
        s_post = (uint32_t *)VirtualAlloc(NULL, RAM_HI, MEM_COMMIT, PAGE_READWRITE);
        s_blend = (Blend *)malloc(BLEND_MAX * sizeof *s_blend);
        if (!s_shadow || !s_blend) return;
    }
    for (uint32_t i = 0; i < s_cand_n; i++)
        memcpy(s_shadow + (s_cand_list[i] << 10), (void *)XBOX_PTR(s_cand_list[i] << 12), 4096);
    lo = 0x10 + s_slice * ((PAGE_N - 0x10) / SLICES); hi = lo + (PAGE_N - 0x10) / SLICES;
    for (p = lo; p < hi; p++) {
        /* Asked once per page: VirtualQuery on the guest's mapped views is
         * slow enough that a slice of them cost ~20 ms. */
        if (!s_cand[p] && page_readable(p))
            memcpy(s_shadow + (p << 10), (void *)XBOX_PTR(p << 12), 4096);
    }
}

/* After it: what it changed, to blend. */
static void blend_post(void)
{
    uint32_t p, lo, hi;
    if (!s_shadow || !s_post || !s_blend || !in_race()) { s_blend_n = 0; return; }
    s_blend_n = 0;
    lo = 0x10 + s_slice * ((PAGE_N - 0x10) / SLICES); hi = lo + (PAGE_N - 0x10) / SLICES;
    for (p = lo; p < hi; p++)          /* new pages the update writes */
        if (!s_cand[p] && s_page_state[p] == 1 &&
            memcmp(s_shadow + (p << 10), (void *)XBOX_PTR(p << 12), 4096)) {
            s_cand[p] = 1;
            s_cand_list[s_cand_n++] = p;
            memcpy(s_post + (p << 10), s_shadow + (p << 10), 4096);   /* no past yet */
        }
    s_slice = (s_slice + 1) % SLICES;
    for (uint32_t i = 0; i < s_cand_n; i++) {
        uint32_t pg = s_cand_list[i];
        /* Words this update wrote, from where the last one left them (not
         * what a render changed them to since). */
        const uint32_t *pre = s_shadow + (pg << 10), *now = (const uint32_t *)XBOX_PTR(pg << 12);
        uint32_t *last = s_post + (pg << 10);
        for (int w = 0; w < 1024 && s_blend_n < BLEND_MAX; w++) {
            uint32_t a = last[w], b = now[w];
            float fa, fb;
            last[w] = b;
            if (pre[w] == b || a == b || !floaty(a) || !floaty(b)) continue;
            /* An integer stepping by a little (a counter, an index) changes
             * a few units of its bits; a float that moves visibly, hundreds. */
            if ((int32_t)(b - a) <= 256 && (int32_t)(a - b) <= 256) continue;
            memcpy(&fa, &a, 4); memcpy(&fb, &b, 4);
            if (!(fb - fa < 4.0f && fa - fb < 4.0f)) continue;
            s_blend[s_blend_n].a = (pg << 12) + 4u * w;
            s_blend[s_blend_n].prev = fa;
            s_blend[s_blend_n++].cur = fb;
        }
    }
    /* Only runs of 3 or more neighbouring words: positions, rotations and
     * matrices. A lone integer that reads as a float (a count the game later
     * divides by) is left alone; blended, one came out 0. */
    int kept = 0;
    for (int i = 0; i < s_blend_n; ) {
        int j = i + 1;
        while (j < s_blend_n && s_blend[j].a == s_blend[j - 1].a + 4) j++;
        if (j - i >= 3)
            while (i < j) s_blend[kept++] = s_blend[i++];
        i = j;
    }
    s_blend_n = kept;
}

static void blend_apply(float t)
{
    for (int i = 0; i < s_blend_n; i++) {
        Blend *b = &s_blend[i];
        if (MEMF(b->a) != b->cur) { b->shown = b->cur; continue; }   /* changed since */
        b->shown = b->prev + (b->cur - b->prev) * t;
        MEMF(b->a) = b->shown;
    }
}

/* Every blended word back to the tick's value, whatever the render did
 * with it: a value it worked out from blended ones must not reach the next
 * update (one became an object pointer and crashed it). */
static void blend_undo(void)
{
    for (int i = 0; i < s_blend_n; i++)
        MEMF(s_blend[i].a) = s_blend[i].cur;
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
        if (smooth()) {
            blend_apply(s_alpha);
            if (s_ticks == 0 && s_keep_idx) render_again(); else render_ticked();
            blend_undo();
            return;
        }
        /* MM3_RENDER_EVERY_FRAME=1 (launcher: Draw every frame) renders
         * between ticks anyway, glitches and all. */
        static int every = -1;
        if (every < 0) every = getenv("MM3_RENDER_EVERY_FRAME") && atoi(getenv("MM3_RENDER_EVERY_FRAME")) == 1;
        if (s_ticks == 0 && !every) {   /* not drawn: the last image again */
            g_esp += 12;
            nv2a_d3d_repeat_frame();
            return;
        }
    }
    sub_001EC8E6_original();
}

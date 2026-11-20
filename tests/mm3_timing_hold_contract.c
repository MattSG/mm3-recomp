/* The console-rate clock (mods/timing.c) restarts its tick count at every
 * lifecycle boundary (pause, scene, state). The F2 hold's totals are counted
 * in those ticks, so they must restart too: kept, the first tick after a hold
 * and a pause computed ticks - held_total below zero (unsigned) and published
 * a game time some 6e17 seconds ahead. Runs the real begin_tick and clock. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/mods/timing.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_edi, g_esp;
static unsigned char memory[0x430000];   /* reaches the pause byte at 0x429DB2 */
static int frozen;

int mm3_mod_on(int mod) { (void)mod; return 0; }
int mm3_old_film_active(void) { return 0; }
int mm3_pause_frozen(void) { return frozen; }
void nv2a_d3d_repeat_frame(void) {}
void nv2a_d3d_manual_present(int enabled) { (void)enabled; }
void nv2a_d3d_interp_frame(float t, int need_new) { (void)t; (void)need_new; }
void sub_00025422_original(void) {}
void sub_0002545A_original(void) {}
void sub_001E6C87_original(void) {}
void sub_001E7590_original(void) {}
void sub_001EC8E6_original(void) {}
void sub_0022DBBF_original(void) {}

enum { GAME = 0x11000 };

static double tick(void)
{
    begin_tick(GAME, 1);
    return MEMD(GAME + 0x38);
}

int main(void)
{
    double before, after;
    int i;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    assert(_putenv("RECOMP_FPS_LIMIT=30") == 0);
    MEMD(GAME + 0x38) = 100.0;
    for (i = 0; i < 3; i++) tick();
    frozen = 1;                                   /* F2: the race held for five ticks */
    for (i = 0; i < 5; i++) tick();
    frozen = 0;
    for (i = 0; i < 2; i++) before = tick();
    assert(before > 100.0 && before < 101.0);

    MEM8(0x00429DB2u) = 1;                        /* the game's pause opens: a lifecycle reset */
    after = tick();
    assert(after > before && after - before < 0.1);   /* one tick on, not ~6e17 s */
    after = tick();
    assert(after - before < 0.2);
    printf("PASS: game time continues by one tick across a hold and a clock reset (%.4f -> %.4f)\n",
           before, after);
    return 0;
}

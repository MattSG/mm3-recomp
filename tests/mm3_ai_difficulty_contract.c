/* Opponents' difficulty must scale each car's script factors once, however
 * long the race stops calling the AI (the game's pause, a load). The old
 * per-car entry expired after two seconds and re-read its own scaled value as
 * the scripts', so every pause compounded the scaling. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/mods/ai_difficulty.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_ecx;
static unsigned char memory[0x400000];
static unsigned original_calls;

int mm3_mod_on(int mod) { (void)mod; return 1; }
void sub_0014194C_original(void) { ++original_calls; }

enum { STATE = 0x11000, AI = 0x12000, PERSONALITY = 0x13000, VEHICLE = 0x14000, CAR = 0x15000 };   /* above the mod's 0x10000 floor */

static void step(void)
{
    g_ecx = STATE;
    sub_0014194C();
}

int main(void)
{
    g_xbox_mem_offset = (ptrdiff_t)memory;
    assert(_putenv("MM3_AI_LEVEL=9") == 0);
    MEM32(STATE + 4) = AI;
    MEM32(AI) = RACE_AI_VTABLE;
    MEM32(AI + 0x94) = PERSONALITY;
    MEM32(AI + 0x28) = VEHICLE;
    MEM32(VEHICLE) = VEHICLE_VTABLE;
    MEM32(VEHICLE + 8) = CAR;
    MEMF(CAR + 0x90) = 1.0f;      /* engine factor the scripts set */
    MEMF(CAR + 0x94) = 1.0f;      /* brake factor */
    MEMF(PERSONALITY + 0xD4) = 20.0f;

    step();
    assert(fabsf(MEMF(CAR + 0x90) - 1.4f) < 1e-5f && fabsf(MEMF(CAR + 0x94) - 1.6f) < 1e-5f);
    assert(MEMF(PERSONALITY + 0xD4) == 20.0f);     /* personality restored after the call */
    step();
    assert(fabsf(MEMF(CAR + 0x90) - 1.4f) < 1e-5f);

    Sleep(2200);                                    /* a pause: no AI calls for over two seconds */
    step();
    assert(fabsf(MEMF(CAR + 0x90) - 1.4f) < 1e-5f && fabsf(MEMF(CAR + 0x94) - 1.6f) < 1e-5f);

    MEMF(CAR + 0x90) = 1.25f;                       /* a script changes the factor: new base */
    step();
    assert(fabsf(MEMF(CAR + 0x90) - 1.25f * 1.4f) < 1e-5f);
    assert(original_calls == 4);
    puts("PASS: opponents' difficulty scales script factors once, across pauses and script changes");
    return 0;
}

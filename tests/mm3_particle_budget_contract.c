/* The particle hook must retain the constructor's ABI and explicit title
 * budget, while bounding the inherited budget at extended draw distances.
 * Invoke each environment mode in a separate process (settings are cached). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/mods/draw_distance.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_esp, g_ebp;
static unsigned char memory[0x10000], expected[0x10000];
static unsigned original_calls;
static unsigned constructor_budget = 12;
static const uint32_t manager = 0x1000, particles = 0x3000;

void sub_000F8DCE_original(void) {}
void sub_000E2D5B_original(void) {}
void sub_000D9E2E_original(void) {}
void sub_000FE71E_original(void) {}
void sub_0022F298_original(void) {}
void sub_001661D0_original(void) {}
void sub_000F4AC3_original(void) {}
void sub_001630A7_original(void) {}
int mm3_mod_on(int mod) { (void)mod; return 1; }
void sub_002256C7_original(void)
{
    assert(g_ecx == manager);
    ++original_calls;
    MEM32(manager + 4) = particles;
    MEM32(particles + 0x524) = constructor_budget;
    g_ecx = 0x87654321; /* The hook must retain the incoming object. */
    g_eax = 0x12345678;
    g_esp += 4;
}

static void check(unsigned wanted)
{
    memset(memory, 0xA5, sizeof memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    memcpy(expected, memory, sizeof memory);
    memcpy(expected + manager + 4, &particles, sizeof particles);
    memcpy(expected + particles + 0x524, &wanted, sizeof wanted);
    g_ecx = manager;
    g_eax = 0;
    g_esp = 0xE000;
    sub_002256C7();
    assert(g_eax == 0x12345678 && g_ecx == 0x87654321 && g_esp == 0xE004);
    assert(memcmp(memory, expected, sizeof memory) == 0);
}

int main(int argc, char **argv)
{
    unsigned wanted = 255;
    assert(argc == 2);
    _putenv_s("RECOMP_DRAW_DISTANCE", "2");
    _putenv_s("MM3_PARTICLE_MAX_OVERDRAW", "");
    if (!strcmp(argv[1], "baseline")) {
        _putenv_s("RECOMP_DRAW_DISTANCE", "1");
        _putenv_s("MM3_PARTICLE_MAX_OVERDRAW", "255");
        wanted = 12;
    } else if (!strcmp(argv[1], "explicit")) {
        _putenv_s("MM3_PARTICLE_MAX_OVERDRAW", "255");
        wanted = 255;
    } else if (!strcmp(argv[1], "low")) {
        _putenv_s("MM3_PARTICLE_MAX_OVERDRAW", "1");
        wanted = 1;
    } else if (!strcmp(argv[1], "title")) {
        constructor_budget = 48;
        wanted = 48;
    } else if (strcmp(argv[1], "default")) {
        _putenv_s("MM3_PARTICLE_MAX_OVERDRAW", argv[1]);
    }
    check(wanted);
    check(wanted);
    assert(original_calls == 2);
    puts("particle budget contract passed");
    return 0;
}

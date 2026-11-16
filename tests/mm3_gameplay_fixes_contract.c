#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char memory[0x500000];
uint32_t g_esp, g_edx, g_ecx, g_eax;
static int rental_enabled = 1, egg_enabled = 1, loads, resets;
#include "../src/mods/gameplay_fixes.c"

int xbox_GuestMapped(uint32_t address, uint32_t length)
{
    return address >= 0x10000 && address < sizeof memory && length <= sizeof memory - address;
}
int mm3_mod_on(int mod)
{
    return mod == MM3_MOD_RENTAL_FIX ? rental_enabled :
           mod == MM3_MOD_EASTER_EGG_FIX ? egg_enabled : 0;
}
void sub_00088A6E_original(void) { ++loads; g_esp += 12; g_eax = 123; }
void sub_00235D1C_original(void)
{
    ++resets; MEM32(g_ecx + 8) = 0; g_esp += 4; g_eax = 1;
}
void sub_00237392_original(void) { sub_00235D1C_original(); }

static void load(uint32_t length, const char *name)
{
    g_esp = 0x10000; g_edx = 0x20000;
    MEM32(g_esp + 4) = length; MEM32(g_esp + 8) = 0x12000;
    strcpy((char *)memory + 0x12000, name);
    sub_00088A6E();
    assert(g_esp == 0x1000C && g_eax == 123);
}

int main(int argc, char **argv)
{
    static const char original[] = "waitForCompetitorToStop()\r\r\n\t\t\ttimerCreate(\"competitor_stop_fallback\", comp_stop_fallback_time, competitorStopFallback)";
    unsigned char snapshot[sizeof original];
    const char *path = "D:\\Scripts\\Washington\\Career\\01\\00Logic.lua";
    size_t length;
    int i;
    FILE *file;
    memcpy(snapshot, original, sizeof original);
    for (i = 0; i < 2; ++i) {
        rental_enabled = i;
        memcpy(memory + 0x20000, original, sizeof original);
        load(sizeof original - 1, path);
        assert(memcmp(memory + 0x20000, original, sizeof original) != 0 ? i : !i);
        if (i) {
            memcpy(snapshot, memory + 0x20000, sizeof original);
            load(sizeof original - 1, path);
            assert(!memcmp(snapshot, memory + 0x20000, sizeof original)); /* idempotent */
        }
    }
    memcpy(memory + 0x20000, original, sizeof original);
    load(sizeof original - 1, "Scripts/Paris/Career/01/00Logic.lua");
    assert(!memcmp(memory + 0x20000, original, sizeof original));
    load(20, path); /* truncated statement */
    assert(!memcmp(memory + 0x20000, original, sizeof original));
    for (i = 0; i < 4; ++i) {
        egg_enabled = i & 1; g_ecx = 0x13000; g_esp = 0x10000;
        MEM32(g_ecx + 8) = 6; MEMF(0x431638) = 100;
        if (i < 2) sub_00235D1C(); else sub_00237392();
        assert(g_eax == 1 && g_esp == 0x10004 && MEM32(g_ecx + 8) == 0);
        assert(MEMF(0x431638) == (egg_enabled ? 0 : 100));
    }
    g_ecx = 0x13000; MEM32(g_ecx + 8) = 1; MEMF(0x431638) = 100;
    sub_00235D1C(); assert(MEMF(0x431638) == 100);
    MEM32(g_ecx + 8) = 6; MEMF(0x431638) = 50;
    sub_00235D1C(); assert(MEMF(0x431638) == 50);
    assert(loads == 5 && resets == 6);
    if (argc == 3) {
        file = fopen(argv[1], "rb"); assert(file);
        length = fread(memory + 0x20000, 1, 0x100000, file);
        assert(!ferror(file) && feof(file)); fclose(file);
        load((uint32_t)length, path);
        file = fopen(argv[2], "wb"); assert(file);
        assert(fwrite(memory + 0x20000, 1, length, file) == length); fclose(file);
    }
    puts("Gameplay fixes: toggles, scope, reset state, ABI and idempotence passed");
    return 0;
}

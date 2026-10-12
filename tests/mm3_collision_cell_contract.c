/* The load guard prevents corrupt face indices from reaching stamp arrays.
 * Model only the loader's guest return boundary; exercise the real sanitizer. */
#include <assert.h>
#include <stdio.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_esi, g_esp;
static unsigned char memory[0x40000];
static const uint32_t cell = 0x10000, header = 0x11000, list = 0x12000, grid = 0x13000;
static int loader_succeeds = 1;
static unsigned calls;
void sub_0015A929_original(void) {
    ++calls;
    MEM32(cell) = header;
    g_eax = loader_succeeds ? 0x123401u : 0x123400u;
    g_esp += 4;
}
#include "../src/mods/collision_fix.c"

static void load(void) {
    g_esi = cell; g_esp = 0x20000;
    sub_0015A929();
    assert(g_esi == cell && g_esp == 0x20004);
    assert(g_eax == (loader_succeeds ? 0x123401u : 0x123400u));
}
int main(void) {
    unsigned i;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    MEM32(header + 4) = 3; MEM32(header + 8) = list; MEM32(header + 0xC) = 5;
    MEM32(header + 0x10) = grid;
    MEM32(header + 0x44) = 4; MEM32(header + 0x48) = MEM32(header + 0x4C) = 1;
    MEM16(list) = 0; MEM16(list+2) = 0xFFFF; MEM16(list+4) = 2;
    MEM16(list+6) = 128; MEM16(list+8) = 1;
    MEM16(grid) = 0; MEM16(grid+2) = 2;
    MEM16(grid+4) = 2; MEM16(grid+6) = 5;
    MEM16(grid+8) = 4; MEM16(grid+10) = 0xFFFF;
    MEM16(grid+12) = 6; MEM16(grid+14) = 2;
    load();
    for (i = 0; i < 5; ++i) assert(MEM16(list + i*2) < 3);
    assert(MEM16(list) == 0 && MEM16(list+4) == 2 && MEM16(list+8) == 1);
    assert(MEM16(list+2) == 0 && MEM16(list+6) == 2);
    assert(MEM16(grid+10) == 5 && MEM16(grid+12) == 2);
    MEM16(list+2) = 0xFFFF;
    load(); assert(MEM16(list+2) == 0xFFFF); /* Cache hit is already decoded. */
    MEM32(cell) = 0; loader_succeeds = 0;
    load(); assert(MEM16(list+2) == 0xFFFF); /* Native failure path retained. */
    MEM32(cell) = 0; loader_succeeds = 1;
    load(); assert(MEM16(list+2) == 0);
    assert(calls == 4);
    puts("PASS collision cell guard: valid faces retained, corrupt indices repaired within grid ranges, bounds clamped, native load/cache/failure ABI");
    return 0;
}

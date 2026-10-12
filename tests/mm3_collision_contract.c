#include "../src/mods/collision_fix.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_esi;
static uint32_t decoded;
void sub_0015A929_original(void) { MEM32(g_esi) = decoded; g_eax = 1; }

int main(void)
{
    /* Uncommitted guard region makes a read across the RAM end fault. */
    void *ram = VirtualAlloc(NULL, 0x08010000u, MEM_RESERVE, PAGE_NOACCESS);
    assert(ram);
    assert(VirtualAlloc(ram, 0x08000000u, MEM_COMMIT, PAGE_READWRITE) == ram);
    g_xbox_mem_offset = (ptrdiff_t)ram;
    const uint32_t hdr = 0x10000, list = 0x11000, grid = 0x12000;
    MEM32(hdr + 4) = 3; MEM32(hdr + 8) = list; MEM32(hdr + 0xC) = 4;
    MEM32(hdr + 0x10) = grid;
    MEM32(hdr + 0x44) = 0x80000001u; MEM32(hdr + 0x48) = 2; MEM32(hdr + 0x4C) = 1;
    MEM16(grid) = 0; MEM16(grid + 2) = 4;
    MEM16(list) = 0xFFFF;
    sanitize(hdr);
    assert(MEM16(list) == 0xFFFF); /* dimension product must not wrap to two cells */

    MEM32(hdr + 0x44) = 1; MEM32(hdr + 0x48) = 1;
    MEM16(list + 2) = 2; MEM16(list + 4) = 0xFFFF; MEM16(list + 6) = 1;
    MEM16(grid + 2) = 9;
    sanitize(hdr);
    assert(MEM16(grid + 2) == 4);
    assert(MEM16(list) == 2 && MEM16(list + 2) == 2 && MEM16(list + 4) == 2 && MEM16(list + 6) == 1);
    MEM16(grid) = 9; sanitize(hdr); assert(MEM16(grid) == 4);

    /* Buffers that start in RAM but extend beyond it must be rejected. */
    MEM16(grid) = 0;
    MEM32(hdr + 8) = 0x07FFFFFFu;
    sanitize(hdr);
    MEM32(hdr + 8) = list; MEM32(hdr + 0x10) = 0x07FFFFFFu;
    sanitize(hdr);
    sanitize(0x07FFFFFFu);

    /* The wrapper still only repairs a newly decoded successful cell. */
    MEM32(hdr + 0x10) = grid; MEM16(grid) = 0; MEM16(grid + 2) = 4;
    g_esi = 0x13000; decoded = hdr;
    MEM16(list) = 0xFFFF; sub_0015A929(); assert(MEM16(list) < 3);
    MEM16(list) = 0xFFFF; sub_0015A929(); assert(MEM16(list) == 0xFFFF);
    assert(VirtualFree(ram, 0, MEM_RELEASE));
    puts("PASS: collision dimensions, full ranges, repair and cache-hit behavior");
    return 0;
}

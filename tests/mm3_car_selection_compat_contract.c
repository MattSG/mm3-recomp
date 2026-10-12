/* Offline contract for retired selections. No archives or saves are opened. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/mm3_car_selection_compat.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
RECOMP_TLS uint32_t g_ebx, g_esi, g_edi, g_ebp, g_seh_ebp, g_fs_base;

static unsigned char memory[0x1000], expected[0x1000];
static const uint32_t catalog = 0x100, entries = 0x200, stack = 0xC00;
static unsigned original_calls;
static uint32_t expected_request, expected_result;
static const uint32_t installed_ids[] = {
    8, 11, 12, 15, /* Original donors remain ordinary catalog entries. */
    9000, 9004, 9005, 9006, 9009, 9010, 9011, 9012, 9013
};

void sub_001DDFC5_original(void)
{
    uint32_t cursor;
    assert(g_ecx == catalog);
    assert(g_esp == stack);
    assert(MEM32(stack) == 0x00121828u);
    assert(MEM32(stack + 4) == expected_request);
    ++original_calls;
    g_eax = 0;
    for (cursor = MEM32(catalog + 8); cursor < MEM32(catalog + 12);
         cursor += 4) {
        uint32_t entry = MEM32(cursor);
        if (MEM32(entry) == MEM32(stack + 4)) {
            g_eax = entry;
            break;
        }
    }
    /* Preserve both the original result and its caller-saved clobbers. */
    g_ecx = 0xA1B2C3D4u;
    g_edx = 0xB2C3D4E5u;
    g_esp += 8;
}

static void check_lookup(uint32_t request, uint32_t lookup)
{
    size_t i;
    g_esp = stack;
    g_ecx = catalog;
    g_eax = 0xDEADBEEFu;
    g_edx = 0x11111111u;
    g_ebx = 0x22334455u;
    g_esi = 0x33445566u;
    g_edi = 0x44556677u;
    g_ebp = 0x55667788u;
    g_seh_ebp = 0x66778899u;
    g_fs_base = 0x778899AAu;
    MEM32(stack) = 0x00121828u;
    MEM32(stack + 4) = request;
    memcpy(expected, memory, sizeof memory);
    expected_request = lookup;
    expected_result = 0;
    for (i = 0; i < sizeof installed_ids / sizeof installed_ids[0]; ++i) {
        if (installed_ids[i] == lookup)
            expected_result = MEM32(entries + (uint32_t)i * 4);
    }
    original_calls = 0;

    sub_001DDFC5();

    assert(original_calls == 1);
    assert(g_eax == expected_result);
    assert(g_ecx == 0xA1B2C3D4u && g_edx == 0xB2C3D4E5u);
    assert(g_esp == stack + 8); /* Exactly the original RET 4. */
    assert(g_ebx == 0x22334455u && g_esi == 0x33445566u);
    assert(g_edi == 0x44556677u && g_ebp == 0x55667788u);
    assert(g_seh_ebp == 0x66778899u && g_fs_base == 0x778899AAu);
    /* Includes catalog membership, the original ID argument and all other
     * stack bytes: the wrapper must not change saved selections in memory. */
    assert(memcmp(memory, expected, sizeof memory) == 0);
}

int main(void)
{
    uint32_t id;
    size_t i;
    memset(memory, 0xA5, sizeof memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    MEM32(catalog + 8) = entries;
    MEM32(catalog + 12) = entries + sizeof installed_ids;
    for (i = 0; i < sizeof installed_ids / sizeof installed_ids[0]; ++i) {
        uint32_t entry = 0x300u + (uint32_t)i * 0x40u;
        MEM32(entries + (uint32_t)i * 4u) = entry;
        MEM32(entry) = installed_ids[i];
    }

    check_lookup(9001u, 12u);
    check_lookup(9002u, 8u);
    check_lookup(9003u, 11u);
    check_lookup(9007u, 15u);
    check_lookup(9008u, 15u);
    /* Exhaustive passthrough covers every original ID, all retained custom
     * IDs and unknown IDs on both sides of the retired selections. */
    for (id = 0; id <= 0xFFFFu; ++id) {
        if (id != 9001u && id != 9002u && id != 9003u && id != 9007u && id != 9008u)
            check_lookup(id, id);
    }
    check_lookup(0x10000u, 0x10000u);
    check_lookup(0x80000000u, 0x80000000u);
    check_lookup(0xFFFFFFFFu, 0xFFFFFFFFu);
    /* Missing donors retain the original NULL result; no fabricated entry. */
    MEM32(catalog + 12) = entries;
    for (i = 0; i < sizeof installed_ids / sizeof installed_ids[0]; ++i)
        MEM32(entries + (uint32_t)i * 4u) = 0;
    check_lookup(9001u, 12u);
    check_lookup(9002u, 8u);
    check_lookup(9003u, 11u);
    check_lookup(9007u, 15u);
    check_lookup(9008u, 15u);
    puts("PASS: five retired IDs resolve to donors; 65,531 other low IDs and "
         "32-bit boundaries pass through; missing donors, RET 4, registers, "
         "catalog and stack bytes preserved");
    return 0;
}

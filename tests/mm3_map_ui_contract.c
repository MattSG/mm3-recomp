/* Execute the title's loader stores and teardown ABI, then query exactly as
 * the asynchronous renderer does. Fake graphics allocation is the boundary. */
#include <assert.h>
#include <stdio.h>
#include <windows.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
static unsigned char memory[0x440000];
static int (*query)(uint32_t);
static const uint32_t stack = 0x80000, context = 0x12000;
static uint32_t load_group, fail_name, invalid_link, released_address;
static unsigned allocations, base_unloads, deletes;
static volatile LONG query_stop;

float nv2a_d3d_display_aspect(void) { return 1.6f; }
void nv2a_d3d_set_edge_hud(int on) { (void)on; }
void nv2a_d3d_set_hud_start(float a, float b, float c, float d) {
    (void)a; (void)b; (void)c; (void)d;
}
void nv2a_d3d_set_centered_ui_query(int (*callback)(void)) { (void)callback; }
void nv2a_d3d_set_map_texture_query(int (*callback)(uint32_t)) { query = callback; }
void sub_0025968E_original(void) { g_esp += 24; }
void sub_001EEC53_original(void) { g_esp += 60; }
#include "production_widescreen.c"

static uint32_t data(unsigned group, unsigned part) {
    return 0x2000000 + group * 0x10000 + part * 0x1000;
}
static uint32_t resource(unsigned group, unsigned part) {
    return 0x20000 + group * 0x1000 + part * 0x100;
}
static void allocate(unsigned part) {
    uint32_t r = resource(load_group, part), d = r + 0x40, t = r + 0x80;
    ++allocations;
    MEM32(r + 0x24) = invalid_link == 1 ? 0 : d;
    MEM32(d + 0x14) = invalid_link == 2 ? 0 : t;
    MEMF(d + 0x20) = MEMF(d + 0x24) = 128;
    MEM32(t + 4) = invalid_link == 3 ? 0 : data(load_group, part);
    g_eax = r;
}
static void fake_icall(uint32_t target) {
    static const uint32_t names[] = {
        0, 0x3A0DAC, 0x3A0D8C, 0x3A0D98, 0x3A0D7C,
        0x3A0DC0, 0x3A0DD4, 0x3A0DE8, 0x3A0DFC, 0x3A0E10
    };
    unsigned i;
    if (target == 0x100) { g_eax = 7; g_esp += 8; return; } /* city property */
    if (target == 0x93DD3) { g_esp += 4; return; } /* sprintf */
    if (target == 0x104) { allocate(0); g_esp += 12; return; } /* map+mask */
    assert(target == 0x108);
    for (i = 1; i < 10; ++i)
        if (MEM32(g_esp + 4) == names[i]) {
            allocate(i);
            if (names[i] == fail_name) g_eax = 0;
            g_esp += 8; return;
        }
    assert(!"unexpected native map resource name");
}
void sub_001F0399(void) { g_eax = 7; g_esp += 4; }
void sub_00116509(void);
void sub_001164F7(void);
void sub_001C33B3(void) {
    ++base_unloads;
    assert(!released_address || query(released_address) == 0);
    g_eax = 1; g_esp += 12;
}
void sub_00170ED1(void) {
    ++deletes;
    assert(!released_address || query(released_address) == 0);
    g_esp += 4; /* cdecl */
}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target, saved, site) fake_icall(target)
#undef RECOMP_ICALL_SAFE
#define RECOMP_ICALL_SAFE(target, saved) fake_icall(target)
#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#include "native_map.inc"

static void setup_call(uint32_t owner) {
    g_esp = stack; g_ecx = owner;
    g_ebx = 0x12345678; g_esi = 0x23456789; g_edi = 0x3456789A;
    g_ebp = g_seh_ebp = 0x98765; g_fp_top = 0;
    MEM32(stack) = 0xDEADBEEF; MEM32(stack + 4) = context;
    MEM32(stack + 8) = 0xABABABAB; MEM32(stack + 12) = 0xCDCDCDCD;
}
static void load(uint32_t owner, unsigned group) {
    setup_call(owner); load_group = group;
    MEM32(owner) = 0x37D790; MEM32(owner + 0x64) = 0x11000;
    sub_0011C742();
    assert(g_esp == stack + 8 && g_fp_top == 0);
    assert(g_ebx == 0x12345678 && g_esi == 0x23456789 && g_edi == 0x3456789A);
    assert(MEM32(stack + 8) == 0xABABABAB && MEM32(stack + 12) == 0xCDCDCDCD);
}
static void check_group(unsigned group, int active) {
    unsigned i;
    for (i = 0; i < 10; ++i) {
        int expected = active ? (i ? 2 : 1) : 0;
        assert(query(data(group, i)) == expected);
        assert(query(data(group, i) | 0x80000000u) == expected);
        assert(query(data(group, i) + 4) == 0); /* Not an address range. */
    }
}
static DWORD WINAPI query_thread(void *arg) {
    unsigned i;
    (void)arg;
    while (!InterlockedCompareExchange(&query_stop, 0, 0))
        for (i = 0; i < 10; ++i) {
            int role = query(data(2, i) | 0x80000000u);
            assert(role == 0 || role == (i ? 2 : 1));
        }
    return 0;
}

int main(void) {
    unsigned i;
    HANDLE reader;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    MEM32(0x11000) = 0x11100; MEM32(0x11100 + 0x5C) = 0x100;
    MEM32(context) = context + 0x100;
    MEM32(context + 0x100 + 0xBC) = 0x104;
    MEM32(context + 0x100 + 0x8C) = 0x108;
    mm3_widescreen_init();
    assert(query && !query(0) && !query(0x80000000) && !query(0x80514000));
    load(0x10000, 0); assert(allocations == 10 && LO8(g_eax)); check_group(0, 1);
    /* No guest pointers are read by the render-thread callback. */
    memset(memory + 0x20000, 0xDD, 0x1000);
    check_group(0, 1);
    assert(g_eax == 1 && g_esp == stack + 8);
    load(0x10200, 1); check_group(0, 1); check_group(1, 1);
    setup_call(0x10000); released_address = data(0, 0);
    sub_0011CAAC(); assert(base_unloads == 1 && g_esp == stack + 12);
    check_group(0, 0); check_group(1, 1); released_address = 0;
    /* The same owner address now belongs to a new allocation generation. */
    load(0x10000, 2); check_group(0, 0); check_group(2, 1);
    /* Failed reload must not retain the old allocation classification. */
    fail_name = 0x3A0DAC;
    load(0x10000, 3); assert(!LO8(g_eax)); check_group(2, 0); check_group(3, 0);
    fail_name = 0;
    for (invalid_link = 1; invalid_link <= 3; ++invalid_link) {
        load(0x10000, 3); check_group(3, 0);
    }
    invalid_link = 0;
    load(0x10000, 2);
    setup_call(0x10000); MEM32(stack + 4) = 1; released_address = data(2, 0);
    sub_0007CEC3();
    assert(deletes == 1 && g_esp == stack + 8 && g_eax == 0x10000);
    check_group(2, 0); check_group(1, 1); released_address = 0;
    /* Shared textures remain classified until their last owner unloads. */
    load(0x10000, 1);
    setup_call(0x10200); sub_0011CAAC(); check_group(1, 1);
    setup_call(0x10000); sub_0011CAAC(); check_group(1, 0);
    /* An in-progress publication cannot block or expose a partial entry. */
    load(0x10000, 2);
    RECOMP_ATOMIC_ADD32(&s_map_owners[0].sequence, 1);
    check_group(2, 0);
    RECOMP_ATOMIC_ADD32(&s_map_owners[0].sequence, 1);
    check_group(2, 1);
    reader = CreateThread(NULL, 0, query_thread, NULL, 0, NULL); assert(reader);
    for (i = 0; i < 1000; ++i) {
        setup_call(0x10000); sub_0011CAAC(); load(0x10000, 2);
    }
    InterlockedExchange(&query_stop, 1);
    assert(WaitForSingleObject(reader, 5000) == WAIT_OBJECT_0); CloseHandle(reader);
    setup_call(0x10000); sub_0011CAAC(); check_group(2, 0);
    /* Bounded capacity cannot evict another live owner's resources. */
    for (i = 0; i < 17; ++i) load(0x60000 + i * 0x100, i);
    for (i = 0; i < 16; ++i) check_group(i, 1);
    check_group(16, 0);
    puts("PASS map UI: native 10-resource loader, physical/mirror query, unload/delete/reuse, shared owners, failed loads, ABI and concurrent publication");
    return 0;
}

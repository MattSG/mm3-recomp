#include "../src/mm3_debug_teleport.c"
#include <assert.h>

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_seh_ebp;
RECOMP_TLS uint32_t g_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
static unsigned updates, resets, moves;
static const uint32_t player = 0x110000, vehicle = 0x120000, car = 0x130000, body = 0x150000;

void sub_002203E5_original(void) { ++updates; assert(g_ecx == player); }
void sub_001CDDA3(void) {
    assert(g_eax == car); ++resets;
    g_esp += 4; g_eax = 0xDEAD; g_fp_stack[0] = -123;
}
void sub_001D65BC(void) {
    assert(g_ecx == vehicle); ++moves;
    memcpy((void *)XBOX_PTR(body + 0xA4), (void *)XBOX_PTR(MEM32(g_esp + 4)), 48);
    g_esp += 8; g_edx = 123; g_ebx = 456; g_df = 1; g_fp_top = 7;
    g_ebp = 0xBAD;
}

int main(int argc, char **argv) {
    void *ram = VirtualAlloc(NULL, 8 * 1024 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(ram); g_xbox_mem_offset = (ptrdiff_t)ram;
    MEM32(0x3C5CDC) = 0x160000; MEM32(0x160054) = 0x170000; MEM32(0x1700D8) = player;
    MEM32(player + 0x19C) = vehicle; MEM32(vehicle) = 0x383768; MEM32(vehicle + 8) = car;
    MEM32(car + 0x3C) = 0x140000; MEM32(0x140004) = body;
    for (unsigned i = 0; i < 12; ++i) MEMF(body + 0xA4 + i*4) = (float)i;
    g_ecx = player; g_esp = 0x700000; g_eax = 99; g_edx = 17; g_ebx = 42;
    g_fp_stack[0] = 3.25; g_fp_top = 2;
    g_ebp = 0x701000;
    finished = CreateEventA(NULL, TRUE, FALSE, NULL);
    /* Disabled hook leaves guest state and car untouched. */
    sub_002203E5(); assert(updates == 1 && resets == 0 && moves == 0);
    enabled = 1;
    sub_002203E5(); assert(available && position[0] == 9 && position[1] == 10 && position[2] == 11);
    if (argc == 2 && strcmp(argv[1], "--serve") == 0) {
        HANDLE server = CreateThread(NULL, 0, pipe_thread, NULL, 0, NULL);
        assert(server); CloseHandle(server);
        for (unsigned i = 0; i < 500; ++i) { sub_002203E5(); Sleep(20); }
        return 0;
    }
    destination[0] = -512.25f; destination[1] = 6.5f; destination[2] = 1001.125f; pending = ++serial;
    sub_002203E5();
    assert(completed == 1 && pending == 0 && resets == 1 && moves == 1);
    assert(g_eax == 99 && g_ecx == player && g_esp == 0x700000 && g_edx == 17 && g_ebx == 42);
    assert(g_fp_stack[0] == 3.25 && g_fp_top == 2 && g_df == 0);
    assert(g_ebp == 0x701000);
    for (unsigned i = 0; i < 9; ++i) assert(MEMF(body + 0xA4 + i*4) == (float)i);
    for (unsigned i = 0; i < 3; ++i) assert(position[i] == destination[i]);
    assert(memcmp((void *)XBOX_PTR(player + 0x48C), (void *)XBOX_PTR(body + 0xA4), 48) == 0);
    assert(MEM8(player + 0x218) == 1);
    /* No update arriving must cancel the request rather than execute it later. */
    char reply[256]; request_teleport(destination, reply, sizeof(reply));
    assert(strstr(reply, "game update timeout") && pending == 0);
    sub_002203E5(); assert(moves == 1);
    /* Guard against a different vehicle layout. */
    MEM32(vehicle) = 0; sub_002203E5(); assert(!available && moves == 1);
    request_teleport(destination, reply, sizeof(reply)); assert(strstr(reply, "no active car"));
    CloseHandle(finished); VirtualFree(ram, 0, MEM_RELEASE);
    puts("PASS: queue, cancellation, inactive path, XYZ/orientation, cache and register contracts (native calls stubbed)");
    return 0;
}

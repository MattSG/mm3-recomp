#include "../src/mm3_debug_teleport.c"
#include <assert.h>

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_seh_ebp;
RECOMP_TLS uint32_t g_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7;
static unsigned updates, resets, moves;
static const uint32_t player = 0x110000, vehicle = 0x120000, car = 0x130000, body = 0x150000;
static volatile LONG stop_update;
static int ground_missing;

static DWORD WINAPI fixture_update(void *unused) {
    (void)unused;
    g_ecx = player; g_esp = 0x700000;
    while (!InterlockedCompareExchange(&stop_update, 0, 0)) {
        sub_002203E5(); Sleep(10);
    }
    return 0;
}

static void check_panel(void) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = panel_proc; wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "MM3OfflinePanelContract";
    assert(RegisterClassA(&wc));
    /* Hidden test window: exercise actual edit controls and button handler
     * without displaying or controlling any game window. */
    HWND panel = CreateWindowA(wc.lpszClassName, "Offline test", WS_OVERLAPPED,
        0, 0, 355, 210, NULL, NULL, wc.hInstance, NULL);
    assert(panel);
    SetWindowTextA(edits[0], "-200.5"); SetWindowTextA(edits[1], "44.125");
    HANDLE updater = CreateThread(NULL, 0, fixture_update, NULL, 0, NULL);
    assert(updater);
    SendMessageA(panel, WM_COMMAND, MAKEWPARAM(1, BN_CLICKED), 0);
    char reply[256]; GetWindowTextA(feedback, reply, sizeof(reply));
    assert(strstr(reply, "Teleported to"));
    assert(position[0] == -200.5f && position[1] == 6.5f && position[2] == 44.125f);
    SendMessageA(panel, WM_TIMER, 1, 0);
    GetWindowTextA(coordinates, reply, sizeof(reply)); assert(strstr(reply, "-200.500"));
    SetWindowTextA(edits[1], "nan");
    SendMessageA(panel, WM_COMMAND, MAKEWPARAM(1, BN_CLICKED), 0);
    GetWindowTextA(feedback, reply, sizeof(reply)); assert(strstr(reply, "finite"));
    assert(position[1] == 6.5f);
    InterlockedExchange(&stop_update, 1);
    assert(WaitForSingleObject(updater, 3000) == WAIT_OBJECT_0); CloseHandle(updater);
    KillTimer(panel, 1); DestroyWindow(panel);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
}

void sub_002203E5_original(void) { ++updates; assert(g_ecx == player); }
#ifndef MM3_TELEPORT_NATIVE_TEST
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
#endif

#ifndef MM3_TELEPORT_NATIVE_TEST
void sub_0021ADF3(void)
{
    uint32_t point = MEM32(g_esp + 8), matrix = g_esp - 0x80;
    if (ground_missing) { g_eax = 0; g_esp += 20; return; }
    memcpy((void *)XBOX_PTR(matrix), (const void *)XBOX_PTR(body + 0xA4), 48);
    MEMF(matrix + 0x24) = MEMF(point);
    MEMF(matrix + 0x28) = 6.5f;
    MEMF(matrix + 0x2C) = MEMF(point + 8);
    g_eax = car; sub_001CDDA3();
    g_ecx = vehicle;
    PUSH32(g_esp, matrix); PUSH32(g_esp, 0); sub_001D65BC();
    memcpy((void *)XBOX_PTR(player + 0x48C), (const void *)XBOX_PTR(body + 0xA4), 48);
    MEM8(player + 0x218) = 1;
    g_eax = 1;
}
#endif

#ifdef MM3_TELEPORT_NATIVE_TEST
extern void sub_001D578C(void);
extern void sub_001CDDA3(void);
extern void sub_001D65BC(void);
void sub_001C032D(void) { g_eax = 0x232000; g_esp += 4; }
void sub_001C0107(void)
{
    assert(g_eax == 0x3B5B78 && g_ecx == 0x232000);
    g_eax = 0x230000; g_esp += 4;
}
void mm3_test_icall(uint32_t target)
{
    g_xmm0 = XMM_SCALAR(-987.0f);
    if (target == 0x001D578C) { sub_001D578C(); return; }
    if (target == 0x001D5F90) { g_eax = MEM32(g_ecx + 8); sub_001CDDA3(); return; }
    if (target == 0x001D65BC) { sub_001D65BC(); return; }
    assert(target == 0x600100);
    uint32_t output = MEM32(g_esp + 4), start = MEM32(g_esp + 8), end = MEM32(g_esp + 12);
    assert(MEMF(start + 4) == 100 && MEMF(end + 4) == -100);
    assert(MEMF(start) == MEMF(end) && MEMF(start + 8) == MEMF(end + 8));
    memset((void *)XBOX_PTR(output), 0, 36);
    MEM32(output) = ground_missing ? 0 : 1;
    MEMF(output + 4) = MEMF(start);
    MEMF(output + 8) = 6;
    MEMF(output + 12) = MEMF(start + 8);
    MEMF(output + 0x14) = 1;
    MEMF(output + 0x1C) = ground_missing ? -1.0f : 0.5f;
    g_eax = output; g_esp += 24;
}
#endif

int main(int argc, char **argv) {
    void *ram = VirtualAlloc(NULL, 8 * 1024 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(ram); g_xbox_mem_offset = (ptrdiff_t)ram;
#ifdef MM3_TELEPORT_NATIVE_TEST
    /* Seed the original PAL constants, not guessed replacements. */
    FILE *constants = fopen("conformance_tmp/native_teleport_memory.bin", "rb");
    assert(constants);
    assert(fread(ram, 1, 8 * 1024 * 1024, constants) == 8 * 1024 * 1024);
    fclose(constants);
#endif
    MEM32(0x3C5CDC) = 0x160000; MEM32(0x160054) = 0x170000; MEM32(0x1700D8) = player;
    MEM32(player + 0x19C) = vehicle; MEM32(vehicle) = 0x383768; MEM32(vehicle + 8) = car;
    MEM32(car + 0x3C) = 0x140000; MEM32(0x140004) = body;
    MEM32(0x140000) = 0x180000;
    MEM32(car + 0x40) = 0x190000; MEM32(car + 0x44) = 4;
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t wheel = 0x200000 + i * 0x1000;
        uint32_t wheel_parameters = 0x210000 + i * 0x1000;
        MEM32(0x190000 + i * 4) = wheel;
        MEM32(wheel) = wheel_parameters;
        MEMF(wheel + 4) = 50; MEMF(wheel + 0x28) = 75; MEMF(wheel + 0x2C) = 90;
        MEMF(wheel_parameters) = 4; MEMF(wheel_parameters + 4) = 5;
    }
    MEM32(player + 0x1AC) = 0x240000;
    MEMF(0x240000 + 0x15044) = 100; MEMF(0x240000 + 0x15038) = -100;
    MEM32(0x230000) = 0x231000; MEM32(0x231008) = 0x600100;
    MEMF(0x200000 + 0x20) = -0.5f; MEMF(0x200000 + 0xFC) = 0;
    const float transform[12] = {0, 0, -1, 0, 1, 0, 1, 0, 0, 9, 10, 11};
    memcpy((void *)XBOX_PTR(body + 0xA4), transform, sizeof(transform));
    for (unsigned i = 0; i < 3; ++i) {
        MEMF(body + 0x154 + i * 4) = 25 + (float)i;
        MEMF(body + 0x160 + i * 4) = 10 + (float)i;
    }
    g_ecx = player; g_esp = 0x700000; g_eax = 99; g_edx = 17; g_ebx = 42;
    g_xmm0.u[0] = 0x12345678; g_fp_stack[0] = 3.25; g_fp_top = 2;
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
    destination[0] = -512.25f; destination[1] = 10000.0f; /* Full-world ray ignores supplied Y. */ destination[2] = 1001.125f; pending = ++serial;
    sub_002203E5();
    assert(completed == 1 && pending == 0);
#ifndef MM3_TELEPORT_NATIVE_TEST
    assert(resets == 1 && moves == 1);
#else
    for (unsigned i = 0; i < 3; ++i) {
        assert(MEMF(body + 0x154 + i * 4) == 0);
        assert(MEMF(body + 0x160 + i * 4) == 0);
    }
    double quaternion_length = 0;
    for (unsigned i = 0; i < 4; ++i) {
        float q = MEMF(body + 0x134 + i * 4);
        assert(isfinite(q)); quaternion_length += q * q;
    }
    assert(fabs(quaternion_length - 1) < 1e-5);
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t wheel = MEM32(0x190000 + i * 4), parameters = MEM32(wheel);
        assert(MEMF(wheel + 4) == 0 && MEMF(wheel + 0x28) == 0 && MEMF(wheel + 0x2C) == 0);
        assert(MEMF(parameters) == 0 && MEMF(parameters + 4) == 1);
    }
#endif
    assert(g_eax == 99 && g_ecx == player && g_esp == 0x700000 && g_edx == 17 && g_ebx == 42);
    assert(g_fp_stack[0] == 3.25 && g_fp_top == 2 && g_df == 0);
    assert(g_ebp == 0x701000 && g_xmm0.u[0] == 0x12345678);
    for (unsigned i = 0; i < 9; ++i) assert(MEMF(body + 0xA4 + i*4) == transform[i]);
    assert(position[0] == destination[0] && position[2] == destination[2]);
    assert(position[1] == 6.5f);
    assert(memcmp((void *)XBOX_PTR(player + 0x48C), (void *)XBOX_PTR(body + 0xA4), 48) == 0);
    assert(MEM8(player + 0x218) == 1);
    /* A ground miss cannot clear motion or move/cache a new transform. */
    float before[12];
    memcpy(before, (const void *)XBOX_PTR(body + 0xA4), sizeof(before));
    ground_missing = 1;
    pending = ++serial;
    destination[0] += 100;
    sub_002203E5();
    assert(strcmp(result, "no usable ground at destination") == 0);
    assert(memcmp(before, (const void *)XBOX_PTR(body + 0xA4), sizeof(before)) == 0);
    ground_missing = 0;
    check_panel();
    /* No update arriving must cancel the request rather than execute it later. */
    char reply[256]; request_teleport(destination, reply, sizeof(reply));
    assert(strstr(reply, "game update timeout") && pending == 0);
    sub_002203E5();
    assert(memcmp((void *)XBOX_PTR(player + 0x48C), (void *)XBOX_PTR(body + 0xA4), 48) == 0);
    /* Guard against a different vehicle layout. */
    MEM32(vehicle) = 0; sub_002203E5(); assert(!available);
    request_teleport(destination, reply, sizeof(reply)); assert(strstr(reply, "no active car"));
    CloseHandle(finished); VirtualFree(ram, 0, MEM_RELEASE);
#ifdef MM3_TELEPORT_NATIVE_TEST
    puts("PASS: original PAL ground reset; automatic Y/clearance, heading, quaternion, motion/cache/registers, ground miss, X/Z panel");
#else
    puts("PASS: queue, cancellation, inactive path, XYZ/orientation, cache and register contracts (native calls stubbed)");
#endif
    return 0;
}

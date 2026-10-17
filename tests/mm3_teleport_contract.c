#include "../src/mods/teleport.c"
#include <assert.h>

/* Default build: teleport.c's own logic, with the native ground reset replaced by
 * a stub that supplies only its outputs (see sub_0021ADF3 below). Assertions about
 * ground height, clearance and the car's transform are only made against the
 * native PAL routine, in the MM3_TELEPORT_NATIVE_TEST build. */
#ifdef MM3_TELEPORT_NATIVE_TEST
#define EXPECTED_GROUND_Y 6.5f   /* PAL 21ADF3 clearance result at the fixture destination */
#else
#define EXPECTED_GROUND_Y 7.75f  /* sentinel the stub returns, so Y cannot come from the request */
#endif

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_seh_ebp;
RECOMP_TLS uint32_t g_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7;
static unsigned updates, feature_ticks;
static const uint32_t player = 0x110000, vehicle = 0x120000, car = 0x130000, body = 0x150000;
static uint32_t expected_update_player = 0x110000;
static int ground_missing;
#ifndef MM3_TELEPORT_NATIVE_TEST
/* What the stub saw in the argument block teleport.c built, and what it returns. */
static struct { uint32_t player, ray; float point[3], heading; } ground_call;
static unsigned ground_calls;
static uint32_t ground_al = 1;
#endif

static void arm_request(void)
{
    pending = ++serial; pending_until = GetTickCount() + 2000;
    requested_player = player; requested_car = car; requested_world = MEM32(player + 0x1AC);
}

static void check_teleport_queue(void)
{
    Mm3TeleportState state;
    assert(mm3_teleport_queue(-200.5f, 44.125f, 1.5f));
    mm3_teleport_state(&state);
    assert(state.pending && state.available);
    assert(destination_heading == 1.5f);
    sub_002203E5();
    mm3_teleport_state(&state);
    assert(!state.pending && strcmp(state.result, "applied") == 0);
    assert(state.position[0] == -200.5f && state.position[1] == EXPECTED_GROUND_Y && state.position[2] == 44.125f);
#ifndef MM3_TELEPORT_NATIVE_TEST
    /* The queued heading reaches the ground reset unchanged; the queued Y is 0. */
    assert(ground_call.heading == 1.5f && ground_call.point[0] == -200.5f && ground_call.point[2] == 44.125f);
    assert(ground_call.point[1] == 0.0f);
#endif
    assert(!mm3_teleport_queue(NAN, 0, NAN));
    assert(state.position[1] == EXPECTED_GROUND_Y);
}

void sub_002203E5_original(void) { ++updates; assert(g_ecx == expected_update_player); }
void mm3_mods_player_update(void) { ++feature_ticks; }

static void check_keyboard_context(void)
{
    /* Exercise the actual player-update hook with debug teleport disabled.
     * The input backend separately tests mapping when this probe changes. */
    assert(!enabled && mm3_keyboard_driving());
    MEM8(0x429DB2) = 1;
    assert(!mm3_keyboard_driving());
    MEM8(0x429DB2) = 0;
    assert(mm3_keyboard_driving());
    MEM32(0x431610) = 1;
    assert(!mm3_keyboard_driving());
    MEM32(0x431610) = 0;
    assert(mm3_keyboard_driving());

    /* A scene without primary-player updates cannot retain driving mode.
     * Unsigned subtraction also handles GetTickCount's wraparound. */
    DWORD stale = GetTickCount() - 500u;
    InterlockedExchange(&keyboard_sampled, (LONG)stale);
    assert(!mm3_keyboard_driving());
    expected_update_player = player + 0x1000;
    g_ecx = expected_update_player;
    sub_002203E5();
    assert((DWORD)InterlockedCompareExchange(&keyboard_sampled, 0, 0) == stale);
    assert(!mm3_keyboard_driving());
    expected_update_player = player;
    g_ecx = player;
    sub_002203E5();
    assert(mm3_keyboard_driving());

    /* Invalid car data disables the driving aliases on the next update. */
    float old_x = MEMF(body + 0xC8);
    MEMF(body + 0xC8) = NAN;
    sub_002203E5();
    assert(!mm3_keyboard_driving());
    MEMF(body + 0xC8) = old_x;
    sub_002203E5();
    assert(mm3_keyboard_driving());
    MEM32(vehicle) = 0;
    sub_002203E5();
    assert(!mm3_keyboard_driving());
    MEM32(vehicle) = 0x383768;
    sub_002203E5();
    assert(mm3_keyboard_driving());
    assert(feature_ticks == updates);
    assert(g_eax == 99 && g_ecx == player && g_esp == 0x700000);
    assert(g_edx == 17 && g_ebx == 42 && g_ebp == 0x701000);
    assert(g_fp_stack[0] == 3.25 && g_fp_top == 2);
    assert(g_xmm0.u[0] == 0x12345678);
}
#ifndef MM3_TELEPORT_NATIVE_TEST
/* Stands in for the native ground reset, 21ADF3. It records the argument block
 * teleport.c pushes (player, point, full-world ray flag, heading), then returns
 * what the native would: a clearance-adjusted Y (EXPECTED_GROUND_Y), AL as the
 * success byte, and a clobbered register set that teleport.c must restore. */
void sub_0021ADF3(void)
{
    uint32_t point = MEM32(g_esp + 8);
    ++ground_calls;
    ground_call.player = MEM32(g_esp + 4);
    ground_call.ray = MEM32(g_esp + 12);
    ground_call.heading = MEMF(g_esp + 16);
    for (unsigned i = 0; i < 3; ++i) ground_call.point[i] = MEMF(point + i * 4);
    g_eax = 0xDEAD; g_ecx = 0xBAD; g_edx = 123; g_ebx = 456; g_ebp = 0xBAD;
    g_fp_stack[0] = -123; g_fp_top = 7; g_df = 1; g_xmm0 = XMM_SCALAR(-987.0f);
    if (ground_missing) { g_eax = 0; g_esp += 20; return; }
    MEMF(body + 0xC8) = ground_call.point[0];
    MEMF(body + 0xCC) = EXPECTED_GROUND_Y;
    MEMF(body + 0xD0) = ground_call.point[2];
    g_eax = ground_al; g_esp += 20;
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
#ifdef MM3_TELEPORT_NATIVE_TEST
    /* Seed the original PAL constants, not guessed replacements. The fixture is
     * a copy of game data (tests/generate_native_teleport_test.py), so it is never
     * checked in: without it this build reports SKIP and exits 77 instead of passing. */
    FILE *constants = fopen("conformance_tmp/native_teleport_memory.bin", "rb");
    if (!constants) {
        puts("SKIP: native PAL teleport: conformance_tmp/native_teleport_memory.bin is missing; "
             "generate it with tests/generate_native_teleport_test.py --xbe game_files/default.xbe");
        return 77;
    }
#endif
    void *ram = VirtualAlloc(NULL, 8 * 1024 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(ram); g_xbox_mem_offset = (ptrdiff_t)ram;
#ifdef MM3_TELEPORT_NATIVE_TEST
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
    /* body+BC and body+C4 are the car's forward X and Z (1 and 0 here), so the heading
     * teleport.c uses when none is queued is atan2(1, 0) = pi/2. */
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
    assert(!mm3_keyboard_driving()); /* Menus before the first player update. */
    /* Disabled hook leaves guest state and car untouched. */
    sub_002203E5(); assert(updates == 1);
    check_keyboard_context();
    enabled = 1;
    sub_002203E5(); assert(available && position[0] == 9 && position[1] == 10 && position[2] == 11);
    assert(!mm3_debug_repair_player()); // Live transform is insufficient during initialization.
    MEM32(car + 0x38) = 0x280000;
    MEM32(0x140000 + 0x3C) = 0x281000; MEM32(0x140000 + 0x4C) = 0x282000;
    assert(mm3_debug_repair_player() == player);
    uint32_t wheel = MEM32(0x190000); MEM32(0x190000) = 0;
    assert(!mm3_debug_repair_player()); MEM32(0x190000) = wheel;
    Mm3TeleportState snapshot;
    mm3_teleport_state(&snapshot);
    assert(snapshot.available && !snapshot.pending && snapshot.position[0] == 9);
    assert(!mm3_teleport_queue(NAN, 0, NAN));
    assert(!mm3_teleport_queue(0, 0, INFINITY));
    assert(mm3_teleport_queue(9, 11, NAN));
    assert(!mm3_teleport_queue(1, 2, NAN));
    pending_until = GetTickCount() - 1;
    sub_002203E5();
    assert(!pending && strcmp(result, "game update timeout") == 0);
#ifndef MM3_TELEPORT_NATIVE_TEST
    assert(ground_calls == 0);
#endif
    /* Expired requests never apply after gameplay resumes. */
    assert(position[0] == 9);
    serial = completed = 0;
    if (argc == 2 && strcmp(argv[1], "--serve") == 0) {
        HANDLE server = CreateThread(NULL, 0, pipe_thread, NULL, 0, NULL);
        assert(server); CloseHandle(server);
        for (unsigned i = 0; i < 500; ++i) { sub_002203E5(); Sleep(20); }
        return 0;
    }
    destination[0] = -512.25f; destination[1] = 10000.0f; /* Full-world ray ignores supplied Y. */ destination[2] = 1001.125f;
    arm_request();
    sub_002203E5();
    assert(completed == 1 && pending == 0);
#ifndef MM3_TELEPORT_NATIVE_TEST
    /* teleport.c hands the native the requested X/Z and Y, the full-world ray flag and
     * player, and the car's heading since no heading was queued (NAN). */
    assert(ground_calls == 1 && ground_call.player == player && ground_call.ray == 0);
    assert(ground_call.point[0] == -512.25f && ground_call.point[1] == 10000.0f && ground_call.point[2] == 1001.125f);
    assert(fabsf(ground_call.heading - 1.5707963f) < 1e-6f);
#endif
    assert(strcmp(result, "applied") == 0);
    assert(g_eax == 99 && g_ecx == player && g_esp == 0x700000 && g_edx == 17 && g_ebx == 42);
    assert(g_fp_stack[0] == 3.25 && g_fp_top == 2 && g_df == 0);
    assert(g_ebp == 0x701000 && g_xmm0.u[0] == 0x12345678);
    /* The reported position is the car's translation after the reset, not the request. */
    assert(position[0] == destination[0] && position[2] == destination[2]);
    assert(position[1] == EXPECTED_GROUND_Y);
#ifdef MM3_TELEPORT_NATIVE_TEST
    assert(MEM8(player + 0x218) == 1);
    for (unsigned i = 0; i < 9; ++i) assert(MEMF(body + 0xA4 + i * 4) == transform[i]);
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
    assert(memcmp((void *)XBOX_PTR(player + 0x48C), (void *)XBOX_PTR(body + 0xA4), 48) == 0);
#endif
    /* A ground miss keeps the last reported position and leaves the car's cache alone. */
    float before[3], before_transform[12];
    memcpy(before, position, sizeof(before));
    memcpy(before_transform, (const void *)XBOX_PTR(body + 0xA4), sizeof(before_transform));
    ground_missing = 1;
    arm_request();
    destination[0] += 100;
    sub_002203E5();
    assert(strcmp(result, "no usable ground at destination") == 0);
    assert(memcmp(before, position, sizeof(before)) == 0);
    ground_missing = 0;
#ifdef MM3_TELEPORT_NATIVE_TEST
    assert(memcmp(before_transform, (const void *)XBOX_PTR(body + 0xA4), sizeof(before_transform)) == 0);
#endif
#ifndef MM3_TELEPORT_NATIVE_TEST
    /* teleport.c reads AL as the native's success byte; higher bits are not success. */
    ground_al = 0x100;
    arm_request();
    sub_002203E5();
    assert(strcmp(result, "no usable ground at destination") == 0);
    ground_al = 1;
#endif
    check_teleport_queue();
    /* No update arriving must cancel the request rather than execute it later. */
    char reply[256]; request_teleport(destination, NAN, reply, sizeof(reply));
    assert(strstr(reply, "game update timeout") && pending == 0);
    sub_002203E5();
    /* Guard against a different vehicle layout. */
    MEM32(vehicle) = 0; sub_002203E5(); assert(!available);
    request_teleport(destination, NAN, reply, sizeof(reply)); assert(strstr(reply, "no active car"));
    CloseHandle(finished); VirtualFree(ram, 0, MEM_RELEASE);
#ifdef MM3_TELEPORT_NATIVE_TEST
    puts("PASS: keyboard heartbeat/pause/scene; original PAL ground reset, automatic Y/clearance, heading, quaternion, motion/cache/registers, ground miss, ImGui teleport queue");
#else
    puts("PASS: teleport.c logic (keyboard heartbeat, queue, cancellation, argument block, heading, reported position, register restore) against a stubbed ground reset");
#endif
    return 0;
}

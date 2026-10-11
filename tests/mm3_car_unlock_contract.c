/* Offline ABI contract for the installed-car availability hook.
 * Build with the real mm3_recomp_types.h copied as recomp_types.h on /I.
 * Each toggle mode runs in its own process, including the unset default. */
#include <assert.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/mm3_car_unlocks.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
RECOMP_TLS uint32_t g_ebx, g_esi, g_edi, g_seh_ebp, g_fs_base;

static unsigned char memory[0x10000], expected[0x10000];
static const uint32_t vehicle = 0x1000, other_vehicle = 0x2000;
static const uint32_t manager = 0x3000, stack = 0xD000;
static unsigned original_calls;
static unsigned char original_locked;
static uint32_t original_result;
static int original_changes_esi;

void sub_001DEA30_original(void)
{
    /* The real function receives ESI and one stack argument, then RET 4.
     * AL is a transition notification, not its vehicle-availability result. */
    assert(g_esi == vehicle);
    assert(MEM32(g_esp) == 0x001DEFC6u);
    assert(MEM32(g_esp + 4) == manager);
    ++original_calls;
    MEM8(vehicle + 0x29) = original_locked;
    MEM32(vehicle + 0x40) += 7; /* Profile evaluation's unrelated side effect. */
    g_eax = original_result;
    g_ecx = 0xA1B2C3D4u;
    g_edx = 0xB1C2D3E4u;
    g_esp += 8; /* Return slot plus the RET 4 argument. */
    if (original_changes_esi)
        g_esi = other_vehicle; /* Stress capture-before-original semantics. */
}

static void check_vehicle(int enabled, unsigned char before_locked,
                          unsigned char stock_locked, uint32_t result,
                          int changes_esi)
{
    memset(memory, 0xA5, sizeof memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    /* A loaded catalog with two entries: no entries may be created or removed. */
    MEM32(manager + 8) = manager + 0x100;
    MEM32(manager + 12) = manager + 0x108;
    MEM32(manager + 0x100) = vehicle;
    MEM32(manager + 0x104) = other_vehicle;
    MEM32(vehicle) = 34;
    MEM8(vehicle + 0x28) = 1; /* Hidden bonus car. */
    MEM8(vehicle + 0x29) = before_locked;
    MEM32(vehicle + 0x30) = 4; /* Race class must remain meaningful. */
    MEM32(vehicle + 0x40) = 100;
    MEM32(other_vehicle) = 35;
    MEM8(other_vehicle + 0x28) = 1;
    MEM8(other_vehicle + 0x29) = 1;
    MEM32(other_vehicle + 0x30) = 2;
    MEM32(stack) = 0x001DEFC6u;
    MEM32(stack + 4) = manager;
    g_esp = stack;
    g_esi = vehicle;
    g_eax = 0xDEADBEEFu;
    g_ecx = 0x11111111u;
    g_edx = 0x22222222u;
    g_ebx = 0x33333333u;
    g_edi = 0x44444444u;
    g_seh_ebp = 0x55555555u;
    g_fs_base = 0x66666666u;
    original_calls = 0;
    original_locked = stock_locked;
    original_result = result;
    original_changes_esi = changes_esi;

    memcpy(expected, memory, sizeof expected);
    expected[vehicle + 0x29] = enabled ? 0 : stock_locked;
    {
        uint32_t side_effect = 107;
        memcpy(expected + vehicle + 0x40, &side_effect, sizeof side_effect);
    }
    sub_001DEA30();

    assert(original_calls == 1);
    assert(g_esp == stack + 8);
    assert(g_eax == result); /* Both AL=0 and the natural AL=1 survive. */
    assert(g_ecx == 0xA1B2C3D4u && g_edx == 0xB1C2D3E4u);
    assert(g_esi == (changes_esi ? other_vehicle : vehicle));
    assert(g_ebx == 0x33333333u && g_edi == 0x44444444u);
    assert(g_seh_ebp == 0x55555555u && g_fs_base == 0x66666666u);
    /* Covers hidden/class flags, the other locked car, catalog membership,
     * untouched stack slots and every byte outside the permitted changes. */
    assert(memcmp(memory, expected, sizeof memory) == 0);
}

static int run_mode(const char *mode)
{
    const char *setting = getenv("MM3_UNLOCK_ALL_CARS");
    int enabled = strcmp(mode, "0") != 0;
    if (!strcmp(mode, "default"))
        assert(setting == NULL);
    else
        assert(setting != NULL && !strcmp(setting, mode));

    check_vehicle(enabled, 1, 1, 0xABCD0000u, 0); /* Still career-locked. */
    check_vehicle(enabled, 1, 0, 0xABCD0001u, 0); /* Naturally unlocked now. */
    check_vehicle(enabled, 0, 0, 0xABCD0000u, 0); /* Already unlocked. */
    check_vehicle(enabled, 1, 1, 0xABCD0000u, 1); /* Incoming pointer capture. */
    printf("PASS: MM3_UNLOCK_ALL_CARS=%s; original ABI, notifications, "
           "catalog and unrelated state preserved\n", mode);
    return 0;
}

int main(int argc, char **argv)
{
    static const char *modes[] = {"default", "1", "0"};
    size_t i;
    _set_error_mode(_OUT_TO_STDERR);
    if (argc == 3 && !strcmp(argv[1], "--case"))
        return run_mode(argv[2]);
    assert(argc == 1);
    for (i = 0; i < sizeof modes / sizeof modes[0]; ++i) {
        const char *value = !strcmp(modes[i], "default") ? "" : modes[i];
        assert(_putenv_s("MM3_UNLOCK_ALL_CARS", value) == 0);
        assert(_spawnl(_P_WAIT, argv[0], argv[0], "--case", modes[i],
                       (char *)NULL) == 0);
    }
    puts("PASS: all installed-car availability contract modes");
    return 0;
}

#include "../src/mods/dev_tools.c"
#include "../tools/xboxrecomp/src/kernel/guest_state.c"
#include <assert.h>

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_seh_ebp, g_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7;
RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3, g_mm4, g_mm5, g_mm6, g_mm7;
RECOMP_TLS uint32_t g_fs_base;
static unsigned calls;
static unsigned repairs;
static Mm3TeleportState active_vehicle = {{0}, 0, 0x110000, 0x130000, 0x240000, 1};
uint32_t mm3_debug_repair_player(void) { return active_vehicle.player; }
void mm3_teleport_state(Mm3TeleportState *out) { *out = active_vehicle; }
void sub_0021CAE8(void) {
    assert(g_esi == active_vehicle.player); ++repairs;
    g_esi = 0xBAD; g_eax = 1; g_esp += 4; g_fp_stack[0] = -1;
}
void sub_0020F7EB(void)
{
    const char *script = (const char *)XBOX_PTR(MEM32(g_esp + 4));
    assert(strcmp(script, "feature_probe_1.lua") == 0);
    ++calls;
    g_eax = 1; g_esp += 8; g_ebp = 0xBAD; g_fp_stack[0] = -1;
}
int xbox_FramebufferKeyDown(int vk) { (void)vk; return 0; }
int main(void)
{
    _putenv_s("MM3_DEBUG_FEATURE_REQUEST", "");
    _putenv_s("MM3_DEV_HUD", "0"); _putenv_s("MM3_DEBUG_MENU", "0");
    _putenv_s("MM3_EXTENDED_CAMERAS", "0"); _putenv_s("MM3_E3_PRESET", "0");
    _putenv_s("MM3_DEV_HOTKEYS", "");
    void *ram = VirtualAlloc(NULL, 8 * 1024 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(ram); g_xbox_mem_offset = (ptrdiff_t)ram;
    MEM32(0x3C5D1C) = 0x200000;
    g_esp = 0x700000; g_eax = 99; g_ebp = 0x701000; g_fp_stack[0] = 3.25;
    assert(!mm3_dev_tools_request(14));
    mm3_dev_tools_tick(); Sleep(210);
    assert(mm3_dev_tools_request(1));
    DWORD timestamp = ui_requested_at;
    assert(!mm3_dev_tools_request(2));
    assert(ui_requested_at == timestamp); // Rejection cannot renew the pending request.
    mm3_dev_tools_tick();
    assert(calls == 1 && g_eax == 99 && g_esp == 0x700000 && g_ebp == 0x701000 && g_fp_stack[0] == 3.25);
    char status[128]; mm3_dev_tools_status(status, sizeof status); assert(strstr(status, "loaded"));
    assert(mm3_dev_tools_request(1));
    ui_requested_at = GetTickCount() - 1000; Sleep(210); mm3_dev_tools_tick();
    assert(calls == 1 && !ui_command);
    mm3_dev_tools_status(status, sizeof status); assert(strstr(status, "cancelled"));
    assert(mm3_dev_tools_request(MM3_DEV_REPAIR)); Sleep(210); mm3_dev_tools_tick();
    assert(repairs == 1 && g_eax == 99 && g_esi == 0 && g_esp == 0x700000 && g_fp_stack[0] == 3.25);
    assert(mm3_dev_tools_request(MM3_DEV_REPAIR)); active_vehicle.car++; Sleep(210); mm3_dev_tools_tick();
    assert(repairs == 1); // A queued repair cannot affect a replacement car.
    active_vehicle.available = 0; assert(!mm3_dev_tools_request(MM3_DEV_REPAIR));
    gameplay_at = (LONG)(GetTickCount() - 1000); assert(!mm3_dev_tools_request(1));
    VirtualFree(ram, 0, MEM_RELEASE);
    puts("PASS: debug command queue, bounds, busy rejection, expiry and guest register preservation");
    return 0;
}

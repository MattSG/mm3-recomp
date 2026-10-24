#include "platform/xbox_winnt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"
#include "dev_tools.h"
#include "teleport.h"
#include "guest_state.h"

extern void sub_0020F7EB(void);
extern void sub_0021CAE8(void); /* vehicleRepair's native backend; ESI = player */
extern int xbox_FramebufferKeyDown(int vk);

/* MM3_DEV_HOTKEYS=HUD:VK,MENU:VK,CAMERAS:VK (hex) toggles each tool in
 * gameplay through the prepared scripts: probes 1/2/3 on, 10/11/12 off. */
static struct { const char *name; int vk, on_probe, off_probe, on, held; } hotkeys[] = {
    {"HUD", 0, 1, 10}, {"MENU", 0, 2, 11}, {"CAMERAS", 0, 3, 12}};

static unsigned ui_command; /* probe + 1; zero means empty; status_lock */
static DWORD ui_requested_at;
static uint32_t ui_player, ui_car, ui_world;
static volatile LONG gameplay_at;
static SRWLOCK status_lock = SRWLOCK_INIT;
static char status_text[128] = "No command requested";

void mm3_dev_tools_status(char *text, unsigned size)
{
    AcquireSRWLockShared(&status_lock);
    if (size) snprintf(text, size, "%s", status_text);
    ReleaseSRWLockShared(&status_lock);
}

int mm3_dev_tools_request(unsigned probe)
{
    int accepted;
    Mm3TeleportState vehicle = {0};
    if (probe > MM3_DEV_REPAIR || !gameplay_at || GetTickCount() - (DWORD)gameplay_at > 300) return 0;
    if (probe == MM3_DEV_REPAIR) {
        mm3_teleport_state(&vehicle);
        if (!vehicle.available) return 0;
    }
    AcquireSRWLockExclusive(&status_lock);
    accepted = !ui_command;
    if (accepted) {
        ui_requested_at = GetTickCount();
        ui_command = probe + 1;
        ui_player = vehicle.player; ui_car = vehicle.car;
        ui_world = vehicle.world;
    }
    ReleaseSRWLockExclusive(&status_lock);
    return accepted;
}

static int flag(const char *name)
{
    const char *value = getenv(name);
    return value && *value && strcmp(value, "0");
}

/* The developer tools mod (mods.c). Run the title's own Lua loader on the
 * game thread, after a player update, never from a host input/pipe thread. A numbered
 * request selects a prepared script; no arbitrary guest address is accepted. */
void mm3_dev_tools_tick(void)
{
    static const char *request;
    static int initialized, running;
    static unsigned startup;
    static DWORD last;
    FILE *file;
    unsigned probe;
    char extra, name[40];
    InterlockedExchange(&gameplay_at, (LONG)GetTickCount());
    if (!initialized) {
        request = getenv("MM3_DEBUG_FEATURE_REQUEST");
        startup = (flag("MM3_DEV_HUD") << 1) | (flag("MM3_DEBUG_MENU") << 2)
                | (flag("MM3_EXTENDED_CAMERAS") << 3) | (flag("MM3_E3_PRESET") << 7);
        const char *keys = getenv("MM3_DEV_HOTKEYS");
        for (unsigned i = 0; i < sizeof hotkeys / sizeof *hotkeys; ++i) {
            const char *at = keys ? strstr(keys, hotkeys[i].name) : NULL;
            if (at && at[strlen(hotkeys[i].name)] == ':')
                hotkeys[i].vk = (int)strtoul(at + strlen(hotkeys[i].name) + 1, NULL, 16) & 0xFF;
            hotkeys[i].on = (startup >> hotkeys[i].on_probe) & 1;
        }
        initialized = 1;
    }
    for (unsigned i = 0; i < sizeof hotkeys / sizeof *hotkeys; ++i) {
        int down = hotkeys[i].vk && xbox_FramebufferKeyDown(hotkeys[i].vk);
        if (down && !hotkeys[i].held) {
            startup |= 1u << (hotkeys[i].on ? hotkeys[i].off_probe : hotkeys[i].on_probe);
            hotkeys[i].on = !hotkeys[i].on;
        }
        hotkeys[i].held = down;
    }
    if (running || GetTickCount() - last < 200) return;
    last = GetTickCount();
    if (!MEM32(0x003C5D1Cu) || g_esp < 0x10400u || g_esp >= 0x08000000u) return;
    AcquireSRWLockExclusive(&status_lock);
    unsigned command = ui_command;
    DWORD requested_at = ui_requested_at;
    uint32_t requested_player = ui_player, requested_car = ui_car;
    uint32_t requested_world = ui_world;
    ui_command = 0;
    ReleaseSRWLockExclusive(&status_lock);
    if (command) {
        if (GetTickCount() - requested_at > 500) {
            AcquireSRWLockExclusive(&status_lock);
            strcpy_s(status_text, sizeof status_text, "Command cancelled: gameplay update expired");
            ReleaseSRWLockExclusive(&status_lock);
            return;
        }
        probe = (unsigned)command - 1;
    } else if (startup) {
        for (probe = 0; !(startup & (1u << probe)); ++probe) {}
        startup &= ~(1u << probe);
    } else {
        if (!request) return;
        file = fopen(request, "rb");
        if (!file) return;
        int valid = fscanf(file, "%u %c", &probe, &extra) == 1 && probe <= 12;
        fclose(file);
        if (remove(request)) return;
        if (!valid) { fprintf(stderr, "[FEATURE_PROBE] rejected request\n"); return; }
    }
    xbox_guest_state guest;
    xbox_guest_state_save(&guest);
    running = 1;
    if (probe == MM3_DEV_REPAIR) {
        uint32_t player = mm3_debug_repair_player();
        Mm3TeleportState vehicle;
        mm3_teleport_state(&vehicle);
        if (!vehicle.available || player != requested_player || vehicle.car != requested_car || vehicle.world != requested_world)
            player = 0;
        if (player) {
            g_esi = player;
            PUSH32(g_esp, 0);
            sub_0021CAE8();
        }
        AcquireSRWLockExclusive(&status_lock);
        strcpy_s(status_text, sizeof status_text, player ? "Native vehicle repair executed" : "Repair cancelled: vehicle not ready or changed");
        ReleaseSRWLockExclusive(&status_lock);
        goto restore;
    }
    snprintf(name, sizeof name, "feature_probe_%u.lua", probe);
    uint32_t text = g_esp - 0x100u;
    memcpy((void *)XBOX_PTR(text), name, strlen(name) + 1);
    g_esp = text - 0x100u;
    g_esi = MEM32(0x003C5D1Cu);
    PUSH32(g_esp, text);
    PUSH32(g_esp, 0);
    fprintf(stderr, "[FEATURE_PROBE] begin %u\n", probe); fflush(stderr);
    sub_0020F7EB();
    AcquireSRWLockExclusive(&status_lock);
    snprintf(status_text, sizeof status_text, "Command %u: script %s (effect requires verification)",
             probe, (g_eax & 0xFF) ? "loaded" : "failed");
    ReleaseSRWLockExclusive(&status_lock);
    fprintf(stderr, "[FEATURE_PROBE] end %u loaded=%u facade_floors=%u\n",
            probe, g_eax & 0xFF, MEM32(0x003A8C3Cu)); fflush(stderr);
restore:
    xbox_guest_state_restore(&guest, guest.eax);
    running = 0;
}

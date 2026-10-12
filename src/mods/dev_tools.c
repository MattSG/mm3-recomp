#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"

extern void sub_0020F7EB(void);
extern int xbox_FramebufferKeyDown(int vk);

/* MM3_DEV_HOTKEYS=HUD:VK,MENU:VK,CAMERAS:VK (hex) toggles each tool in
 * gameplay through the prepared scripts: probes 1/2/3 on, 10/11/12 off. */
static struct { const char *name; int vk, on_probe, off_probe, on, held; } hotkeys[] = {
    {"HUD", 0, 1, 10}, {"MENU", 0, 2, 11}, {"CAMERAS", 0, 3, 12}};

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
    if ((!request && !startup) || running || GetTickCount() - last < 200) return;
    last = GetTickCount();
    if (!MEM32(0x003C5D1Cu) || g_esp < 0x10400u || g_esp >= 0x08000000u) return;
    if (startup) {
        for (probe = 0; !(startup & (1u << probe)); ++probe) {}
        startup &= ~(1u << probe);
    } else {
        file = fopen(request, "rb");
        if (!file) return;
        int valid = fscanf(file, "%u %c", &probe, &extra) == 1 && probe <= 12;
        fclose(file);
        if (remove(request)) return;
        if (!valid) { fprintf(stderr, "[FEATURE_PROBE] rejected request\n"); return; }
    }
    uint32_t regs[] = {g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp, g_seh_ebp};
    double fp[8];
    int top = g_fp_top, cmp = g_fp_cmp, df = g_df;
    uint16_t cc = g_fp_cc, control = g_fp_control_word;
    RecompXmm xmm[] = {g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7};
    memcpy(fp, g_fp_stack, sizeof fp);
    running = 1;
    snprintf(name, sizeof name, "feature_probe_%u.lua", probe);
    uint32_t text = g_esp - 0x100u;
    memcpy((void *)XBOX_PTR(text), name, strlen(name) + 1);
    g_esp = text - 0x100u;
    g_esi = MEM32(0x003C5D1Cu);
    PUSH32(g_esp, text);
    PUSH32(g_esp, 0);
    fprintf(stderr, "[FEATURE_PROBE] begin %u\n", probe); fflush(stderr);
    sub_0020F7EB();
    fprintf(stderr, "[FEATURE_PROBE] end %u loaded=%u facade_floors=%u\n",
            probe, g_eax & 0xFF, MEM32(0x003A8C3Cu)); fflush(stderr);
    g_eax=regs[0]; g_ecx=regs[1]; g_edx=regs[2]; g_esp=regs[3]; g_ebx=regs[4];
    g_esi=regs[5]; g_edi=regs[6]; g_ebp=regs[7]; g_seh_ebp=regs[8];
    memcpy(g_fp_stack, fp, sizeof fp);
    g_fp_top=top; g_fp_cmp=cmp; g_df=df; g_fp_cc=cc; g_fp_control_word=control;
    g_xmm0=xmm[0]; g_xmm1=xmm[1]; g_xmm2=xmm[2]; g_xmm3=xmm[3];
    g_xmm4=xmm[4]; g_xmm5=xmm[5]; g_xmm6=xmm[6]; g_xmm7=xmm[7];
    running = 0;
}

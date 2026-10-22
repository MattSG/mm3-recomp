#include <assert.h>
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
static unsigned char memory[0x400000];
static int enabled=1;
static uint32_t flare_capacity;
static const uint32_t stack=0x50000;
#include "../src/mods/draw_distance.c"
int mm3_mod_on(int mod) { (void)mod; return enabled; }
#define UNUSED(name) void name(void) {}
UNUSED(sub_000E2D5B_original) UNUSED(sub_000D9E2E_original)
UNUSED(sub_000FE71E_original) UNUSED(sub_001630A7_original)
UNUSED(sub_001661D0_original) UNUSED(sub_0022F298_original)
UNUSED(sub_000F4AC3_original) UNUSED(sub_002256C7_original)
UNUSED(sub_000F8750_original) UNUSED(sub_000F8DCE_original)
UNUSED(sub_00344770)
void sub_00248518_original(void) {
    assert(MEM32(g_esp+4)==0x1234 && MEM32(g_esp+8)==3);
    flare_capacity=g_eax; g_eax=0xC0FFEE; g_esp+=12;
}
static void flare_budget(uint32_t caller,uint32_t capacity,uint32_t expected) {
    g_esp=stack; MEM32(stack)=caller; MEM32(stack+4)=0x1234; MEM32(stack+8)=3;
    g_ebx=0x11; g_esi=0x22; g_edi=0x33;
    g_eax=capacity; g_ecx=0x55; g_edx=0x66; g_ebp=0x77; g_seh_ebp=0x88;
    sub_00248518();
    assert(flare_capacity==expected && g_eax==0xC0FFEE && g_esp==stack+12);
    assert(g_ebx==0x11 && g_esi==0x22 && g_edi==0x33);
    assert(g_ecx==0x55 && g_edx==0x66 && g_ebp==0x77 && g_seh_ebp==0x88);
}
int main(int argc,char **argv) {
    uint32_t expected=argc>1?(uint32_t)strtoul(argv[1],NULL,10):1024u;
    g_xbox_mem_offset=(ptrdiff_t)memory;
    enabled=0; flare_budget(0x0022C89E,256,256);
    enabled=1; flare_budget(0x0022C89E,256,expected);
    flare_budget(0x0022C89F,256,256);
    flare_budget(0x0022C89E,512,512);
    enabled=0; flare_budget(0x0022C89E,256,256); enabled=1;

    return 0;
}

#include <assert.h>
#include <string.h>
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
static unsigned char memory[0x400000];
static int enabled=1, reserves, originals;
static uint32_t flare_capacity;
static const uint32_t device=0x10000, camera=0x20000, object=0x30000, buffer=0x40000, stack=0x50000;
#include "../src/mods/draw_distance.c"
int mm3_mod_on(int mod) { (void)mod; return enabled; }
#define UNUSED(name) void name(void) {}
UNUSED(sub_000E2D5B_original) UNUSED(sub_000D9E2E_original)
UNUSED(sub_000FE71E_original) UNUSED(sub_001630A7_original)
UNUSED(sub_001661D0_original) UNUSED(sub_0022F298_original)
UNUSED(sub_000F4AC3_original) UNUSED(sub_002256C7_original)
UNUSED(sub_000F8750_original)
void sub_000F8DCE_original(void) { originals++; g_esp+=16; }
void sub_00344770(void) {
    assert(MEM32(g_esp+4)==0x8000 && MEM32(g_esp+8)==0x10000);
    reserves++; MEM32(device)=buffer; MEM32(device+4)=buffer+0x8000;
    g_eax=buffer; g_ecx=0xBAD; g_edx=0xBAD; g_ebp=0xBAD; g_seh_ebp=0xBAD; g_esp+=12;
}
void sub_00248518_original(void) {
    assert(MEM32(g_esp+4)==0x1234 && MEM32(g_esp+8)==3);
    flare_capacity=g_eax; g_eax=0xC0FFEE; g_esp+=12;
}
static void flare_budget(uint32_t caller,uint32_t capacity,uint32_t expected) {
    g_esp=stack; MEM32(stack)=caller; MEM32(stack+4)=0x1234; MEM32(stack+8)=3;
    g_eax=capacity; g_ecx=0x55; g_edx=0x66; g_ebp=0x77; g_seh_ebp=0x88;
    sub_00248518();
    assert(flare_capacity==expected && g_eax==0xC0FFEE && g_esp==stack+12);
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

    MEM32(0x351F48)=device; MEM32(0x352E00)=0x10000;
    MEM32(object+12)=camera; MEMF(camera+0x1ED8)=2;
    MEMF(camera+0x1FB0)=1000; MEMF(camera+0x1EE4)=1; MEMF(camera+0x1EF8)=1;
    for (int room=52;room>=-4;room-=4) {
        MEM32(device)=buffer; MEM32(device+4)=buffer+room;
        g_esp=stack; g_ecx=object; g_eax=0x12345600; g_edx=0x789; g_ebp=0xABC; g_seh_ebp=0xDEF;
        MEM32(stack+4)=0xFF123456; int before=reserves;
        sub_000F8DCE();
        assert(reserves-before==(room<48));
        assert(g_esp==stack+16 && g_ecx==object && g_edx==0x789 && g_eax==0x12345601);
        assert(g_ebp==0xABC && g_seh_ebp==0xDEF);
        assert(MEM32(buffer)==MM3_FOG_HEADER && MEM32(buffer+4)==MM3_FOG_TAG);
        assert(MEM32(device)==buffer+48 && MEM32(buffer+24)==0xFF123456);
    }
    assert(originals==0); enabled=0; g_esp=stack; sub_000F8DCE();
    assert(originals==1 && g_esp==stack+16);
    return 0;
}

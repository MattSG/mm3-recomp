/* Execute native camera Arrow and minimap paths with their real guest ABI. */
#include <assert.h>
#include <stdio.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
static unsigned char memory[0x20000], camera_snapshot[0x214];
static unsigned draws, textures, arrow_draws, render_state_calls, visibility_calls;
static const uint32_t frame = 0xD000, stack = 0xC000, map = 0x1000, hud = 0x2000;
static const uint32_t camera = 0x5000, outputs = 0x6000;

static void sub_001C032D(void) { ++render_state_calls; g_eax=0x7000; g_esp+=4; }
static void sub_001C0107(void) { assert(g_eax==0x3B02C8 && g_ecx==0x7000); ++render_state_calls; g_eax=0x7100; g_esp+=4; }
static void sub_00019B8D(void) { assert(g_ebx==camera+0xF8); assert(MEM32(g_esp+8)==1 && MEM32(g_esp+12)==0); g_eax=0x7200; g_esp+=16; }
static void sub_00164019(void) { assert(MEM32(g_esp+4)==0x7300 && MEM32(g_esp+8)==0x7200); ++arrow_draws; g_esp+=12; }
static void sub_000F2803(void) { assert(g_eax==0x7100); ++render_state_calls; g_esp+=4; }
static void sub_0011EFC1(void) { assert(g_edi<4); visibility_calls|=1u<<g_edi; g_eax=outputs+g_edi*0x40; g_esp+=4; }
static void sub_001C1585(void)
{
    uint32_t texture = MEM32(g_esp + 4);
    assert(g_edx == 0x3000 && g_eax == 0x4000);
    assert(MEM32(g_esp + 8) == frame - 260);
    assert(texture >= 1 && texture <= 3);
    textures |= 1u << (texture - 1);
    ++draws;
    g_esp += 12;
}

#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#define ebp g_ebp
static uint32_t _fa, _fb;
static int32_t _fas, _fbs;
#include "native_arrows.inc"

static void check_camera(void (*draw)(void), unsigned flags, unsigned mask, int hidden)
{
    unsigned i;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    memset(memory, 0, sizeof memory);
    g_esp=stack; g_esi=camera; g_ebx=0xA001; g_edi=0xA002; g_ebp=0xA003;
    MEM32(camera+0xE8)=(flags&1)?0x7300:0;
    MEM8(camera+0xF4)=(uint8_t)(flags&2);
    MEM32(camera+0x204)=flags&4;
    MEM32(camera+0x128)=mask;
    for(i=0;i<4;++i) MEM32(outputs+i*0x40+0x20)=0xFFFFFFFF;
    memcpy(camera_snapshot,memory+camera,sizeof camera_snapshot);
    arrow_draws=render_state_calls=visibility_calls=0;
    draw();
    assert(arrow_draws==(!hidden && (flags&3)==3 && !(flags&4)));
    assert(render_state_calls==arrow_draws*3);
    assert(visibility_calls==mask);
    for(i=0;i<4;++i) assert(MEM32(outputs+i*0x40+0x20)==((mask&(1u<<i))?i:0xFFFFFFFF));
    assert(!memcmp(camera_snapshot,memory+camera,sizeof camera_snapshot));
    assert(g_esp==stack+4 && g_ebx==0xA001 && g_edi==0xA002 && g_esi==camera && g_ebp==0xA003);
}

static void check_minimap(unsigned flags)
{
    g_xbox_mem_offset = (ptrdiff_t)memory;
    memset(memory, 0, sizeof memory);
    g_esp=stack; g_ebp=frame; g_ebx=map; g_esi=0xBAD000; g_edi=0xBAD001;
    MEM32(frame-4)=hud;
    MEM8(hud+0x41)=(uint8_t)(flags&1); MEM8(hud+0x42)=(uint8_t)(flags&2); MEM8(hud+0x43)=(uint8_t)(flags&4);
    MEM32(map+0x28)=1; MEM32(map+0x2C)=2; MEM32(map+0x30)=3;
    MEM32(frame+8)=0x3000; MEM32(frame+0x18)=0x4000;
    MEM32(stack)=0xAAA001; MEM32(stack+4)=0xAAA002; MEM32(stack+8)=0xAAA003; MEM32(frame)=0xAAA004;
    draws=textures=0;
    original_minimap();
    assert(textures==flags && draws==!!(flags&1)+!!(flags&2)+!!(flags&4));
    assert(g_edi==0xAAA001 && g_esi==0xAAA002 && g_ebx==0xAAA003);
    assert(g_ebp==0xAAA004 && g_esp==frame+28);
}

int main(void)
{
    unsigned flags,mask;
    for(flags=0;flags<8;++flags) {
        for(mask=0;mask<16;++mask) {
            check_camera(original_camera,flags,mask,0);
            check_camera(hidden_camera,flags,mask,1);
        }
        check_minimap(flags);
    }
    puts("PASS top camera Arrow hidden across 128 states; player visibility, native ABI and all minimap turn icons retained");
    return 0;
}

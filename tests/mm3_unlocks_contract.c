#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned char memory[0x500000];
uint32_t g_eax, g_ecx, g_edx, g_esp, g_edi, g_esi;
static int master, paints_on, races, auto_on, count_calls, effect_calls;
#include "../src/mods/unlocks.c"
#include "../src/mods/easter_eggs.c"

int xbox_GuestMapped(uint32_t a, uint32_t n)
{ return a >= 0x10000 && a < sizeof memory && n <= sizeof memory - a; }
int mm3_mod_on(int mod)
{ return mod == MM3_MOD_UNLOCK_ALL ? master : mod == MM3_MOD_UNLOCK_PAINTS ? paints_on : mod == MM3_MOD_AUTO_EASTER_EGG ? auto_on : races; }
void xbox_guest_state_save(xbox_guest_state *s)
{ s->eax=g_eax; s->ecx=g_ecx; s->edx=g_edx; s->esp=g_esp; s->edi=g_edi; s->esi=g_esi; }
void xbox_guest_state_restore(const xbox_guest_state *s, uint32_t result)
{ g_eax=result; g_ecx=s->ecx; g_edx=s->edx; g_esp=s->esp; g_edi=s->edi; g_esi=s->esi; }
void sub_00235D84(void)
{
    ++effect_calls; MEM32(g_esi+8)=g_eax;
    MEM32(0x431638)=g_eax==6?100:0;
    g_eax=0; g_ecx=0; g_edx=0; g_esi=0; g_esp+=4;
}
void sub_00235F5B_original(void)
{ assert(g_ecx==0x30000 && g_eax==123 && g_esi==55); g_esp+=16; }
void sub_00235EB6_original(void) { g_eax=1; g_esp+=4; }
void sub_001E50A9_original(void) { g_eax=0xABCD0000; g_esp+=8; }
void sub_001E5E9D_original(void) { g_eax=3; g_ecx=0x11223344; g_edx=0x55667788; g_esp+=12; }
void sub_001E12E3(void)
{ assert(MEM32(g_esp+4)==2 || MEM32(g_esp+4)==3); g_eax=0x12000; g_esp+=8; }
void sub_001E0F69(void)
{ assert(g_eax<=1 && MEM32(g_esp+4)==0x12000); ++count_calls; g_eax=30; g_ecx=0; g_edx=0; g_esp+=8; }
void sub_0012530C_original(void)
{ MEM32(g_eax+0x1B0)=1; MEM32(g_eax+0x1B4)=2; g_eax=9; g_esp+=12; }
void sub_001218F7_original(void)
{
    uint32_t p=MEM32(0x3C5CB4), city=MEM32(0x3B8580);
    if (master || races) {
        assert(MEM32(p+0x140+city*4)==MEM32(g_edi+0x1B0));
        assert(MEM32(p+0x148+city*4)==MEM32(g_edi+0x1B4));
    }
    MEM32(p+0x140+city*4)=MEM32(g_edi+0x1B0);
    MEM32(p+0x148+city*4)=MEM32(g_edi+0x1B4);
    g_eax=7; g_esp+=4;
}
void sub_001238B7_original(void)
{
    MEM32(g_esi+0x150)=3; MEM32(g_esi+0x154)=0; MEM32(g_esi+0x158)=0;
    MEM32(g_esi+0x15C)=1; MEM32(g_esi+0x160)=1;
    g_eax=1; g_esp+=4;
}

int main(void)
{
    uint32_t saved[4]={1,2,3,4}, mode;
    for (mode=0; mode<3; ++mode) {
        int on=mode!=0;
        master=mode==2; paints_on=races=mode==1;
        g_esp=0x20000; MEM32(g_esp)=0x1DE940;
        sub_001E50A9(); assert(g_eax==(on?0xABCD0001u:0xABCD0000u) && g_esp==0x20008);
        g_esp=0x20000; MEM32(g_esp)=0x212ED3;
        sub_001E50A9(); assert(g_eax==0xABCD0000); /* Actual collection queries unchanged. */
        g_esp=0x20000; MEM32(g_esp+4)=1; MEM32(g_esp+8)=3;
        sub_001E5E9D(); assert(g_eax==(on?30u:3u) && g_esp==0x2000C);
        assert(g_ecx==0x11223344 && g_edx==0x55667788);
        assert(MEM32(0x20004)==1 && MEM32(0x20008)==3);
        g_eax=0x30000; g_esp=0x20000;
        sub_0012530C(); assert(g_eax==9 && g_esp==0x2000C);
        assert(MEM32(0x301B0)==(on?8u:1u));
        MEM32(0x3C5CB4)=0x40000; MEM32(0x3B8580)=1; memcpy(memory+0x40140,saved,sizeof saved);
        g_edi=0x30000; g_esp=0x20000; sub_001218F7();
        assert(g_eax==7 && g_esp==0x20004);
        if (on) assert(!memcmp(memory+0x40140,saved,sizeof saved));
        g_esi=0x30000; g_esp=0x20000; MEM32(0x3B8578)=1;
        MEM32(0x30154)=6; MEM32(0x30158)=2;
        sub_001238B7(); assert(g_eax==1 && g_esp==0x20004);
        assert(MEM32(0x30150)==(on?30u:3u));
        assert(MEM32(0x30154)==(on?6u:0u));
        assert(MEM32(0x30158)==(on?2u:0u));
        assert(MEM32(0x30160)==(on?3u:1u));
    }
    assert(count_calls==6);
    master=paints_on=races=0;
    MEM32(0x407C1C)=1; MEM32(0x407C20)=0;
    memory[0x3859F8]=255; memory[0x3859F9]=255; memory[0x3859FA]=200;
    memory[0x3859FB]=memory[0x3859FC]=memory[0x3859FD]=255;
    memory[0x3859FE]=255; memory[0x3859FF]=255; memory[0x385A00]=200;
    for (mode=0; mode<2; ++mode) {
        const char *choice=getenv("MM3_EASTER_EGG_EFFECT");
        uint32_t wanted=choice && !strcmp(choice,"8")?8:6;
        auto_on=(int)mode; MEM32(0x30008)=0;
        g_ecx=0x30000; g_eax=123; g_esi=55; g_esp=0x28000; MEM32(g_esp+4)=1;
        sub_00235F5B(); assert(g_esp==0x28010);
        assert(MEM32(0x30008)==(mode?wanted:0));
        if (mode) assert(MEM32(0x3000C)==0xFFFFFFC8u);
        assert(mm3_old_film_active()==(mode && wanted==6));
    }
    assert(effect_calls==1);
    MEM32(0x407C20)=1; MEM32(0x30008)=0;
    g_ecx=0x30000; g_eax=123; g_esi=55; g_esp=0x28000;
    sub_00235F5B(); assert(effect_calls==1 && MEM32(0x30008)==0);
    puts("Unlocks: independent/master toggles, native counts, collection state, career profile and ABI passed");
    puts("Easter eggs: off/film/sketch, native tint, CPU state and multiplayer guard passed");
    return 0;
}

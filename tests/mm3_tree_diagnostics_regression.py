"""Exercise production contact diagnostics with original-call state sentinels.

Run inside `python scripts/devenv.py --shell`. This checks that opt-in observation leaves
every guest register, x87 value, memory byte and call sequence unchanged,
including nested events, unreadable metadata and renewable line budgets.
"""
import subprocess
import tempfile
from pathlib import Path

import cc

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long long clock_ms=100000;
static char logs[500000];
static size_t log_size;
static unsigned flushes;
static unsigned long long clock_now(void){return clock_ms;}
static void trace_log(const char *format,...){
 va_list args;int written;va_start(args,format);
 written=vsnprintf(logs+log_size,sizeof logs-log_size,format,args);va_end(args);
 assert(written>=0 && (size_t)written<sizeof logs-log_size);log_size+=written;
}
#define MM3_TREE_CLOCK clock_now
#define MM3_TREE_LOG trace_log
#define MM3_TREE_FLUSH() (++flushes)
#include "guest_state.c"
#include "vec3_rotate.c"
#include "guest_read.c"
#include "mm3_tree_diagnostics.c"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp,g_fs_base;
RECOMP_TLS int g_fp_top,g_df,g_fp_cmp;
RECOMP_TLS uint16_t g_fp_control_word,g_fp_cc;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS RecompMmx g_mm0,g_mm1,g_mm2,g_mm3,g_mm4,g_mm5,g_mm6,g_mm7;
/* Test double for the fork's xbox_GuestRead: the same host-checked read over this fixture's RAM. */
int xbox_GuestRead(uint32_t va, void *out, size_t size)
{
    SIZE_T copied = 0;
    uintptr_t host;
    if (va < 0x10000u || size > UINT32_MAX || va > UINT32_MAX - size) return 0;
    host = (uintptr_t)va + (uintptr_t)g_xbox_mem_offset;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)host, out, size, &copied) && copied == size;
}
void sub_0016CABD(void){assert(0);}
static unsigned char *ram,*expected;
static unsigned calls,events;
static int direct_call,unreadable;
static const uint32_t stack=0x200000,object=0x120000,owner=0x130000,local=0x160000;
static const uint32_t event=0x170000,velocity=0x180000,output=0x190000;
typedef struct {uint32_t r[10];int m[3];uint16_t c[2];double fp[8];} State;
static State state(void){
 State s={{g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp,g_fs_base},
 {g_fp_top,g_df,g_fp_cmp},{g_fp_control_word,g_fp_cc},{0}};
 memcpy(s.fp,g_fp_stack,sizeof s.fp);return s;
}
static void result_state(void){
 g_eax=0xABCDEF01;g_ecx=0xBCDEF012;g_edx=0xCDEF0123;g_esp=stack+40;
 g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;
 g_ebp=0x456789AB;g_seh_ebp=0x56789ABC;g_fs_base=0x6789ABCD;
 g_fp_top=3;g_df=1;g_fp_cmp=-1;g_fp_control_word=0x37F;g_fp_cc=0x4100;
 for(unsigned i=0;i<8;i++)g_fp_stack[i]=i*1.25-3.0;
}
void sub_000E1F49_original(void){
 ++events;MEM32(output)=0x1A1234;g_eax=0xCC000001;g_edx=0xBADF00D;g_esp+=40;
 g_fp_top=1;g_fp_stack[1]=27.0;g_fp_cc=0x4000;g_seh_ebp=0x66666666;
}
void sub_001D137B_original(void){assert(mm3_tree_use_solid_contact(object, 0.0f));result_state();}
static void event_setup(void){
 g_esp=stack-128;MEM32(g_esp)=0x23BBF4;g_ecx=event;
 MEM32(g_esp+24)=velocity;MEM32(g_esp+36)=output;
}
void sub_0016AEDB_original(void){
 ++calls;
 if(!unreadable){
  event_setup();if(direct_call)sub_000E1F49_original();else sub_000E1F49();
  MEM32(owner+0x112C)++;MEM32(owner+0x13E38)++;
 }
 result_state();
}
static void interned(uint32_t address,const char *name){
 size_t size=strlen(name);
 for(size_t i=0;i<=size;i++){
  unsigned c=i? (unsigned char)name[size-i]:0;uint32_t node=address+(uint32_t)i*16;
  MEM32(node)=(i==size?0:node+16)|(c>>4);MEM32(node+4)=c&15;MEM32(node+8)=i?node-16:0;
 }
}
static void setup(int paris){
 float bounds[]={10,20,30,12,24,36};float speed[]={20,0,0};
 memset(ram,0,0x400000);result_state();g_esp=stack;
 MEM32(stack)=0x16D123;MEM32(stack+4)=unreadable?0xFFFFFFFF:object;
 MEM32(object)=owner;MEM32(object+0xA0)=local;MEM32(object+0xC8)=0x240;
 memcpy((void *)&MEM8(object+0x20),bounds,sizeof bounds);MEM32(owner+0x112C)=5;MEM32(owner+0x13E38)=7;
 MEM8(local)=1;MEM8(local+3)=60;MEM32(local+0x2C)=event;
 interned(0x1A0000,"tree");interned(0x1A0080,"leaves");
 MEM32(local+0x10)=paris?0:0x1A0000;
 MEM32(event+0x27)=0x1A0000;MEM32(event+9)=0x1A0080;
 MEM32(event+0xD)=1;MEMF(event+0x1A)=25;
 memcpy((void *)&MEM8(velocity),speed,sizeof speed);
}
static void solid_speed_cases(void){
 /* Retail 1D137B gates flag-2 hops at 40 km/h. Trees must take the solid label
  * at every speed (below: native solid; above: replaces the hop); other props must not. */
 static const float speeds[]={0.0f,5.0f,11.0f,11.12f,20.0f};
 unsigned i;
 setup(0);MEM16(object+0x100)=2;MEM32(object+0x108)=local;
 for(i=0;i<sizeof speeds/sizeof speeds[0];i++)assert(mm3_tree_use_solid_contact(object,speeds[i])==1);
 setup(0);MEM8(local)=0;MEM32(local+0x10)=0x1A0080;MEM16(object+0x100)=2;MEM32(object+0x108)=local;
 for(i=0;i<sizeof speeds/sizeof speeds[0];i++)assert(mm3_tree_use_solid_contact(object,speeds[i])==0);
}
static unsigned count(const char *needle){
 unsigned n=0;const char *p=logs;while((p=strstr(p,needle))){++n;p+=strlen(needle);}return n;
}
int main(int argc,char **argv){
 int tracing=atoi(argv[1]);State expected_state,actual_state;unsigned before;
 (void)argc;
 _putenv_s("MM3_TREE_TRACE",tracing==2?"contacts":tracing?"1":"0");
 ram=VirtualAlloc(0,0x400000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(ram);
 expected=malloc(0x400000);assert(expected);g_xbox_mem_offset=(ptrdiff_t)ram;
 for(int paris=0;paris<2;paris++){
  setup(paris);direct_call=1;sub_0016AEDB_original();expected_state=state();memcpy(expected,ram,0x400000);
  setup(paris);direct_call=0;sub_0016AEDB();actual_state=state();
  assert(memcmp(expected_state.r,actual_state.r,sizeof actual_state.r)==0);
  assert(memcmp(expected_state.m,actual_state.m,sizeof actual_state.m)==0);
  assert(memcmp(expected_state.c,actual_state.c,sizeof actual_state.c)==0);
  assert(memcmp(expected_state.fp,actual_state.fp,sizeof actual_state.fp)==0);
  assert(memcmp(expected,ram,0x400000)==0);
 }
 assert(calls==4 && events==4 && s_tree_render_depth==0);
 if(tracing==1){
  assert(strstr(logs,"tree=1") && strstr(logs,"sound=none") && strstr(logs,"sound=tree"));
  assert(strstr(logs,"local_bounds_center=(11.000,22.000,33.000)"));
  assert(strstr(logs,"render_pool0=5->6 render_pool1=7->8"));
  assert(strstr(logs,"speed_squared=400.000 threshold_squared=25.000"));
  assert(strstr(logs,"during_render=1 kind=1") && strstr(logs,"output=001A1234"));
 }else if(tracing==2){
  assert(strstr(logs,"mode=contacts object_render=off"));
  assert(!strstr(logs,"[TREE_OBJECT_RENDER]") && strstr(logs,"during_render=0 kind=1"));
 }else assert(log_size==0 && flushes==0);
 unreadable=1;setup(0);sub_0016AEDB();
 if(tracing==1)assert(strstr(logs,"object=FFFFFFFF read=0"));
 unreadable=0;clock_ms+=1001;before=count("[TREE_COLLISION_EVENT]");
 for(unsigned i=0;i<100;i++){setup(0);event_setup();sub_000E1F49();}
 assert(count("[TREE_COLLISION_EVENT]")-before==(tracing?TREE_LINES_PER_SECOND:0));
 clock_ms+=1001;before=count("[TREE_COLLISION_EVENT]");
 for(unsigned i=0;i<100;i++){setup(0);event_setup();sub_000E1F49();}
 assert(count("[TREE_COLLISION_EVENT]")-before==(tracing?TREE_LINES_PER_SECOND:0));
 if(tracing)assert(strstr(logs,"suppressed=68"));
 /* Standalone render events do not spend the next second's contact budget. */
 clock_ms+=1001;before=count("[TREE_COLLISION_EVENT]");
 setup(0);MEM32(event+0xD)=0;event_setup();sub_000E1F49();
 assert(count("[TREE_COLLISION_EVENT]")==before);
 /* Unreadable names/velocity do not introduce a guest fault. */
 setup(0);MEM32(event+9)=0xFFFFFFFF;MEM32(event+0x27)=0xFFFFFFFF;
 event_setup();MEM32(g_esp+24)=0xFFFFFFFF;sub_000E1F49();
 if(tracing)assert(strstr(logs,"particle=none sound=none velocity_read=0"));
 /* A full event/render budget cannot hide a corrected physical contact.
  * Contacts-only mode must keep the solid report with original-call state. */
 setup(0);MEM16(object+0x100)=2;MEM32(object+0x108)=local;
 memcpy(expected,ram,0x400000);result_state();expected_state=state();g_esp=stack;
 before=count("[TREE_BREAK_CONTACT]");sub_001D137B();actual_state=state();
 assert(memcmp(expected_state.r,actual_state.r,sizeof actual_state.r)==0);
 assert(memcmp(expected_state.m,actual_state.m,sizeof actual_state.m)==0);
 assert(memcmp(expected_state.c,actual_state.c,sizeof actual_state.c)==0);
 assert(memcmp(expected_state.fp,actual_state.fp,sizeof actual_state.fp)==0);
 assert(memcmp(expected,ram,0x400000)==0 && MEM16(object+0x100)==2);
 assert(count("[TREE_BREAK_CONTACT]")-before==(tracing?1u:0u));
 if(tracing)assert(strstr(logs,"response=solid original_flags=preserved"));
 solid_speed_cases();
 free(expected);VirtualFree(ram,0,MEM_RELEASE);
 printf("PASS: tree diagnostics tracing=%d, nested original call ABI/memory, solid tree contacts at 0-20 m/s, both city tree identifiers, unreadable metadata, renewable budgets and render filtering\n",tracing);
 return 0;
}
'''


def main():
    cc.skip_unless_windows('the contract includes windows.h')
    cc.require_compiler()
    cmake = (ROOT / 'CMakeLists.txt').read_text()
    assert 'list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_0016AEDB sub_000E1F49 sub_001D137B)' in cmake
    assert cmake.count('src/mm3_tree_diagnostics.c') == 1
    with tempfile.TemporaryDirectory(prefix='mm3-tree-diagnostics-') as path:
        folder = Path(path)
        source, executable = folder / 'contract.c', folder / ('contract' + cc.exe_suffix())
        source.write_text(SOURCE)
        subprocess.run(cc.compile_command([source], executable,
                                          includes=[ROOT / 'src', ROOT / 'tools/xboxrecomp/include', ROOT / 'tools/xboxrecomp/src', ROOT / 'tools/xboxrecomp/src/kernel'],
                                          defines=['_CRT_SECURE_NO_WARNINGS'], std='c11', optimize='none',
                                          warnings='high', obj=folder / 'contract.obj'), check=True, cwd=folder)
        for tracing in ('0', '1', '2'):
            subprocess.run([str(executable), tracing], check=True)


if __name__ == '__main__':
    main()

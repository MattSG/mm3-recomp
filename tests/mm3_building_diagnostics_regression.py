"""Check facade diagnostic ABI, guarded reads and repeatable log budgets.

Run with tools/venv/Scripts/python.exe inside `python scripts/devenv.py --shell`.
The fixture includes the production wrappers and exercises their original
function seams without launching MM3 or changing game/save files.
"""
import re
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
 va_list args;int written;
 va_start(args,format);
 written=vsnprintf(logs+log_size,sizeof logs-log_size,format,args);
 va_end(args);
 assert(written>=0 && (size_t)written<sizeof logs-log_size);
 log_size+=(size_t)written;
}
#define MM3_BUILDING_CLOCK clock_now
#define MM3_BUILDING_LOG trace_log
#define MM3_BUILDING_FLUSH() (++flushes)
#include "mm3_building_diagnostics.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp,g_fs_base,g_eflags;
RECOMP_TLS int g_fp_top,g_df,g_fp_cmp;
RECOMP_TLS uint16_t g_fp_control_word,g_fp_cc;
RECOMP_TLS double g_fp_stack[8];
static unsigned char *ram,*expected;
static int scenario;
static const uint32_t stack=0x200000,object=0x120000,owner=0x130000;
static const uint32_t renderer=0x140000,queue=0x160000,camera=0x170000;

/* NATIVE_ALLOCATOR */

static unsigned allocation_index;
static uint32_t allocation_registers[9][2][11];
static double allocation_fp[9][2][8];
static int allocation_modes[9][2][3];
static uint16_t allocation_controls[9][2][2];
static void allocate(uint32_t caller,unsigned stream_flag,unsigned tail,unsigned quads){
 uint32_t regs[11];int modes[3];uint16_t controls[2];unsigned i=allocation_index++;
 assert(i<2);g_esp=stack-100;MEM32(g_esp)=caller;
 MEM32(g_esp+4)=stream_flag;MEM32(g_esp+8)=tail;g_eax=queue;g_edx=quads;
 sub_00162F75();
 regs[0]=g_eax;regs[1]=g_ecx;regs[2]=g_edx;regs[3]=g_esp;regs[4]=g_ebx;
 regs[5]=g_esi;regs[6]=g_edi;regs[7]=g_ebp;regs[8]=g_seh_ebp;
 regs[9]=g_fs_base;regs[10]=g_eflags;
 modes[0]=g_fp_top;modes[1]=g_df;modes[2]=g_fp_cmp;
 controls[0]=g_fp_control_word;controls[1]=g_fp_cc;
 if(s_active){
  assert(memcmp(regs,allocation_registers[scenario][i],sizeof regs)==0);
  assert(memcmp(g_fp_stack,allocation_fp[scenario][i],sizeof g_fp_stack)==0);
  assert(memcmp(modes,allocation_modes[scenario][i],sizeof modes)==0);
  assert(memcmp(controls,allocation_controls[scenario][i],sizeof controls)==0);
 }else{
  memcpy(allocation_registers[scenario][i],regs,sizeof regs);
  memcpy(allocation_fp[scenario][i],g_fp_stack,sizeof g_fp_stack);
  memcpy(allocation_modes[scenario][i],modes,sizeof modes);
  memcpy(allocation_controls[scenario][i],controls,sizeof controls);
 }
 /* The fixture's caller writes four actual 28-byte vertices per quad. */
 if(g_eax)memset(&MEM8(g_eax),0xA7,quads*4*28);
}

static void result_state(void){
 g_eax=0xABCDEF01;g_ecx=0xBCDEF012;g_edx=0xCDEF0123;g_esp=stack+20;
 g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;
 g_ebp=0x456789AB;g_seh_ebp=0x56789ABC;g_fs_base=0x6789ABCD;g_eflags=0x247;
 g_fp_top=3;g_df=1;g_fp_cmp=-1;g_fp_control_word=0x37F;g_fp_cc=0x4100;
 for(unsigned i=0;i<8;i++)g_fp_stack[i]=i*1.25-3.0;
}
void sub_001C7B15_original(void){g_eax=scenario==0 || MEM32(g_esp)==0x16886A;g_esp+=8;}
void sub_001A67BA_original(void){g_eax=scenario==1 || MEM32(g_esp)==0x168889;g_esp+=8;}
void sub_00166E01_original(void){
 MEMF(g_esi)=22500.0f;MEM8(g_ebx)=0;
 g_eax=0x10000000;g_ecx=0x11000000;g_edx=0x22000000;g_esp+=4;
}
void sub_001681FB_original(void){
 MEM32(0x3C57A0)++;MEM32(0x3C57A4)+=3;MEM32(queue+0x1E4C)++;
 g_esp=stack-80;MEM32(g_esp)=0x16886A;sub_001C7B15();
 g_esp=stack-80;MEM32(g_esp)=0x168889;sub_001A67BA();
 allocate(0x166441,scenario==7?0:1,1,1);
 g_esp+=32;
}
void sub_00167DA5_original(void){
 allocate(0x168070,1,1,2);
 if(scenario==3)allocate(0x168070,0,0,1);
 g_esp+=28;
}
void sub_00166B3E_original(void){MEM32(queue+0x1F84)+=4;g_esp+=8;}
void sub_00168A77_original(void){
 allocation_index=0;
 if(scenario==5){result_state();return;}
 g_esp=stack-80;MEM32(g_esp)=0x168A92;sub_001C7B15();
 if((g_eax&255)==0){
  g_esp=stack-80;MEM32(g_esp)=0x168AB6;sub_001A67BA();
  if((g_eax&255)==0){
   g_esp=stack-80;MEM32(g_esp)=0x168B0E;
   g_esi=stack-40;g_ebx=stack-41;sub_00166E01();
   g_esp=stack-80;
   if(scenario==2 || scenario==7)sub_001681FB();
   if(scenario==3 || scenario==6 || scenario==8)sub_00167DA5();
   if(scenario==4)sub_00166B3E();
  }
 }
 result_state();
}

static void setup(uint32_t facade){
 float center[3]={10,20,30},extents[3]={5,15,1},eye[3]={100,20,30};
 memset(ram,0,0x400000);
 MEM32(facade)=owner;MEM16(facade+8)=6;MEM16(facade+10)=18;
 memcpy(&MEM8(facade+0x48),center,sizeof center);
 memcpy(&MEM8(facade+0x54),extents,sizeof extents);
 MEM32(owner+0x374)=renderer;MEM32(renderer+0x15214)=3;
 MEM32(renderer+0x1521C)=0x190000;MEM32(renderer+0x15368)=0x1A0000;
 MEM32(renderer+0x15378)=queue;
 MEM32(queue+0x1F80)=7;MEM32(queue+0x1F84)=9;MEM32(queue+0x1F88)=4096;
 MEM32(queue+0x1E4C)=2;MEM32(0x3A8C3C)=2;
 MEM32(queue+0x1EC8)=11;MEM32(queue+0x1ECC)=13;MEM32(queue+0x1ED0)=12288;
 MEM32(queue+0x1F24)=17;MEM32(queue+0x1F28)=19;MEM32(queue+0x1F2C)=2048;
 MEM32(queue+0x1EC0)=0x190000;MEM32(0x190024)=0x1C0000;
 MEM32(queue+0x1F1C)=0x1A0000;MEM32(0x1A0024)=0x1D0000;
 if(scenario==6){MEM32(queue+0x1EC8)=12288;MEM32(queue+0x1ECC)=0;}
 if(scenario==7){MEM32(queue+0x1F24)=0;MEM32(queue+0x1F28)=2048;}
 if(scenario==8)MEM32(renderer+0x15378)=0x480000; /* invalid diagnostic stream read */
 MEM32(0x3C57A0)=4;MEM32(0x3C57A4)=7;
 memcpy(&MEM8(camera),eye,sizeof eye);
 MEM32(stack)=0x168DAB;MEM32(stack+4)=camera;MEM32(stack+8)=0x1B0000;
 MEM32(stack+12)=6;MEM32(stack+16)=0x3F;
 result_state();g_eax=facade;g_esp=stack;
}

static void check_state(void){
 assert(g_eax==0xABCDEF01 && g_ecx==0xBCDEF012 && g_edx==0xCDEF0123);
 assert(g_esp==stack+20 && g_ebx==0x12345678 && g_esi==0x23456789 && g_edi==0x3456789A);
 assert(g_ebp==0x456789AB && g_seh_ebp==0x56789ABC && g_fs_base==0x6789ABCD && g_eflags==0x247);
 assert(g_fp_top==3 && g_df==1 && g_fp_cmp==-1 && g_fp_control_word==0x37F && g_fp_cc==0x4100);
 for(unsigned i=0;i<8;i++)assert(g_fp_stack[i]==i*1.25-3.0);
}

static unsigned count_events(void){
 unsigned count=0;const char *p=logs;
 while((p=strstr(p,"[BUILDING] "))!=NULL){count++;p++;}
 return count;
}
static unsigned count_failures(void){
 unsigned count=0;const char *p=logs;
 while((p=strstr(p,"[BUILDING_ALLOCATION_FAILURE] "))!=NULL){count++;p++;}
 return count;
}

int main(int argc,char**argv){
 int tracing;unsigned before,first_window;
 assert(argc==2);tracing=atoi(argv[1]);_putenv_s("MM3_BUILDING_TRACE",argv[1]);
 ram=VirtualAlloc(NULL,0x500000,MEM_RESERVE,PAGE_NOACCESS);
 assert(ram && VirtualAlloc(ram,0x400000,MEM_COMMIT,PAGE_READWRITE)==ram);
 expected=malloc(0x400000);assert(expected);g_xbox_mem_offset=(ptrdiff_t)ram;
 for(scenario=0;scenario<9;scenario++){
  setup(object);sub_00168A77_original();memcpy(expected,ram,0x400000);
  setup(object);if(scenario==5)g_eax=0x480000; /* reserved but uncommitted */
  clock_ms+=300;sub_00168A77();check_state();
  assert(memcmp(ram,expected,0x400000)==0);
 }
 if(tracing){
  assert(strstr(logs,"tier=none frustum_reject=1") && strstr(logs,"auxiliary_reject=1"));
  assert(strstr(logs,"tier=near") && strstr(logs,"tier=mid") && strstr(logs,"tier=far"));
  assert(strstr(logs,"closest_distance=150.000 grid=6x18 detail_floor_cap=2"));
  assert(strstr(logs,"descriptors=4:5 draws=7:10 batches=2:3"));
  assert(strstr(logs,"stream0_head=11:19 stream0_tail=13:13 stream0_cap=12288:12288 stream0_read=1,1"));
  assert(strstr(logs,"stream1_head=17:17 stream1_tail=19:23 stream1_cap=2048:2048 stream1_read=1,1"));
  assert(strstr(logs,"mid0_alloc=1:1:0:2 mid1_alloc=1:1:0:1"));
  assert(strstr(logs,"upper0_alloc=1:1:0:1"));
  assert(strstr(logs,"mid0_alloc=1:0:1:2") && strstr(logs,"upper1_alloc=1:0:1:1"));
  assert(strstr(logs,"alloc_last_caller=00168070") && strstr(logs,"alloc_last_caller=00166441"));
  assert(strstr(logs,"cell_frustum=1:1 cell_auxiliary=1:1"));
  assert(strstr(logs,"stream0_read=0,0") && strstr(logs,"zero_queue_delta=-1"));
  assert(strstr(logs,"tail=9:13"));
  assert(strstr(logs,"facade=00480000") && strstr(logs,"read=0,0,0,0,1"));
  assert(flushes>=2);
 }else assert(log_size==0 && flushes==0);
 scenario=2;clock_ms+=2000;before=count_events();
 for(unsigned i=0;i<100;i++){setup(object+0x1000+i*0x100);sub_00168A77();check_state();}
 first_window=count_events()-before;
 if(tracing)assert(first_window==BUILDING_LINES_PER_SECOND);
 else assert(first_window==0);
 /* Allocation failures remain visible after unrelated facades spend all
  * ordinary lines. Their own smaller budget is also strictly bounded. */
 scenario=6;before=count_failures();
 for(unsigned i=0;i<100;i++){setup(0x240000+i*0x100);sub_00168A77();check_state();}
 if(tracing){
  assert(count_failures()-before==BUILDING_FAILURE_LINES_PER_SECOND);
  assert(strstr(logs,"stream=0 requested_quads=2 result=00000000 stream0_head=12288:12288"));
 }else assert(count_failures()==0);
 /* The next second must regain its budget for repeated approaches. */
 scenario=2;clock_ms+=1001;before=count_events();
 for(unsigned i=0;i<100;i++){setup(object+0x20000+i*0x100);sub_00168A77();check_state();}
 if(tracing){assert(count_events()-before==BUILDING_LINES_PER_SECOND);assert(strstr(logs,"failure_events=8 failure_suppressed=92"));}
 else assert(log_size==0);
 /* Identical visits inside the sample interval delegate without another log. */
 before=count_events();setup(object+0x20000+99*0x100);sub_00168A77();check_state();
 assert(count_events()==before);
 free(expected);VirtualFree(ram,0,MEM_RELEASE);
 printf("PASS: building diagnostics tracing=%d, original ABI/memory, native28-byte allocator success/failure, cell culls, guarded reads and renewable rate limit\n",tracing);
 return 0;
}
'''


def main():
    cc.skip_unless_windows('the contract includes windows.h')
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix='mm3-building-diagnostics-') as path:
        folder = Path(path)
        source, executable = folder / 'contract.c', folder / f'contract{cc.exe_suffix()}'
        generated = (ROOT / 'src/recomp/gen/recomp_0014.c').read_text()
        native = generated.split('void sub_00162F75(void)', 1)[1].split('\n/**', 1)[0]
        native = 'void sub_00162F75_original(void)' + native
        native = re.sub(r'\b(eax|ecx|edx|esp|ebx|esi|edi|ebp)\b', r'g_\1', native)
        source.write_text(SOURCE.replace('/* NATIVE_ALLOCATOR */', native))
        subprocess.run(cc.compile_command(
            [source], executable, includes=[ROOT / 'src', ROOT / 'tools/xboxrecomp/include', ROOT / 'tools/xboxrecomp/src'],
            defines=['_CRT_SECURE_NO_WARNINGS'], std='c11', optimize='none', warnings='high',
            obj=folder / 'contract.obj'), check=True, cwd=folder)
        for tracing in ('0', '1'):
            subprocess.run([str(executable), tracing], check=True)


if __name__ == '__main__':
    main()

"""Exercise bounded indirect-call diagnostics using original Xbox call bytes.

Run after scripts/enter-env.ps1 with tools/venv/Scripts/python.exe. Compiles the
production logger unchanged and never accesses a running game or save files.
"""
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import mm3_cpu_diagnostics_regression as cpu
import mm3_x87_exp_regression as common

ROOT = common.ROOT
CALLS = ((0x1F635D, 0x64), (0x14E6CB, 0x70), (0x14E732, 0x70),
         (0x14E7AF, 0x5C), (0x14E7CC, 0x5C))


def fixture(image):
    source = (ROOT / 'src/recomp_manual.c').read_text()
    bodies = []
    for name in ('mm3_icall_read', 'mm3_icall_decode', 'recomp_icall_not_code_log'):
        body = re.search(r'(?:static (?:int|unsigned)|void) ' + name
                         + r'\([^)]*\)\n\{.*?\n\}', source, re.S)
        assert body, name
        bodies.append(body.group())
    seed_calls = []
    for caller in [caller for caller, _ in CALLS] + [0x35D048]:
        start = caller - 7
        raw = next(raw + start - va for va, raw, size, _ in image.secs
                   if va <= start and caller <= va + size)
        data = ','.join(f'0x{v:02X}' for v in image.d[raw:raw+7])
        seed_calls.append(f'{{unsigned char b[]={{ {data} }};memcpy(memory+0x{start:X},b,7);}}')
    tests = '\n'.join(f'check(0x{caller:X},0,1,0x720000+0x{offset:X},0);'
                      for caller, offset in CALLS)
    return (cpu.COMMON_NATIVE + '\nuint32_t g_xbox_code_lo=0x12000,g_xbox_code_hi=0x362000;\n'
            + 'static int g_mm3_frontend_icall_trace;\n' + '\n'.join(bodies) + r'''
static void check(uint32_t caller,uint32_t target,unsigned kind,uint32_t expected,unsigned reg){
 uint32_t slot=0xAAAAAAAA;unsigned base=99;
 assert(mm3_icall_decode(caller,target,&slot,&base)==kind);
 assert(slot==expected&&base==reg);
}
static void code(const unsigned char*b,size_t n){memcpy(memory+0x50000,b,n);}
static void snapshot(uint32_t*s){
 s[0]=g_eax;s[1]=g_ecx;s[2]=g_edx;s[3]=g_ebx;
 s[4]=g_esp;s[5]=g_ebp;s[6]=g_esi;s[7]=g_edi;
}
int main(int argc,char**argv){
 uint32_t before[8],after[8],word=0,target=0;
 unsigned char*before_memory=(unsigned char*)malloc(sizeof memory);assert(before_memory);
 assert(argc==2);g_xbox_mem_offset=(ptrdiff_t)memory;
 g_esp=0x700000;g_eax=0x720000;g_ecx=0x730000;g_edx=2;
 g_ebx=0x740000;g_ebp=0x710000;g_seh_ebp=0x710020;
 g_esi=0x23456789;g_edi=0x3456789A;
 MEM32(g_ebp)=0x456789AB;MEM32(g_ebp+4)=0x12345678;
 MEM32(g_esp)=0x1F635D;MEM32(g_ecx)=g_eax;
''' + '\n'.join(seed_calls) + tests + r'''
 {unsigned char b[]={0xFF,0x15,0x34,0x28,0x39,0x00};code(b,sizeof b);
  check(0x50006,0,1,0x392834,8);}
 {unsigned char b[]={0xFF,0xD0};code(b,sizeof b);
  check(0x50002,g_eax,2,0,0);}
 {unsigned char b[]={0xFF,0x54,0x90,0x20};code(b,sizeof b);
  check(0x50004,0,1,g_eax+g_edx*4+0x20,0);}
 {unsigned char b[]={0xFF,0x93,0xFC,0xFF,0xFF,0xFF};code(b,sizeof b);
  check(0x50006,0,1,g_ebx-4,3);}
 {unsigned char b[]={0xFF,0x54,0x24,0x04};code(b,sizeof b);
  check(0x50004,0,1,g_esp+8,4);}
 {unsigned char b[]={0xFF,0x14,0x95,0x00,0x00,0x74,0x00};code(b,sizeof b);
  check(0x50007,0,1,0x740000+g_edx*4,8);}
 {unsigned char b[]={0x90,0x90};code(b,sizeof b);
  check(0x50002,0,0,0,9);}
 assert(!mm3_icall_read(0,&word,4));
 assert(!mm3_icall_read(0xFFFFFFFE,&word,4));
 assert(!mm3_icall_read(0x07FFFFFE,&word,4));
 assert(!mm3_icall_read(0x700000,&word,129));
 {unsigned char*pages=(unsigned char*)VirtualAlloc(0,0x30000,MEM_RESERVE,PAGE_NOACCESS);
  DWORD prior;uint32_t buffer[8]={0};ptrdiff_t original_offset=g_xbox_mem_offset;
  assert(pages);assert(VirtualAlloc(pages,0x10000,MEM_COMMIT,PAGE_READWRITE)==pages);
  g_xbox_mem_offset=(ptrdiff_t)pages-0x10000;
  *(uint32_t*)pages=0xCAFEBABE;
  assert(mm3_icall_read(0x10000,&word,4)&&word==0xCAFEBABE);
  assert(!mm3_icall_read(0x20000,&word,4)); /* reserved, uncommitted */
  assert(!mm3_icall_read(0x1FFFF,buffer,sizeof buffer)); /* crosses into uncommitted page */
  assert(VirtualProtect(pages,0x10000,PAGE_NOACCESS,&prior));
  assert(!mm3_icall_read(0x10000,&word,4));
  assert(VirtualProtect(pages,0x10000,PAGE_READWRITE|PAGE_GUARD,&prior));
  assert(!mm3_icall_read(0x10000,&word,4));
  assert(VirtualProtect(pages,0x10000,PAGE_READWRITE,&prior));
  assert(VirtualFree(pages,0,MEM_RELEASE));g_xbox_mem_offset=original_offset;
 }
 if(strcmp(argv[1],"cascade")==0){g_eax=g_ecx=0;MEM32(g_esp)=0x14E6CB;}
 if(strcmp(argv[1],"invalid")==0){
  g_ecx=0x07FFFFFC;g_eax=g_esi=g_edi=0;
  MEM32(g_esp)=0xFB05E;g_mm3_frontend_icall_trace=1;
 }
 if(strcmp(argv[1],"trace")==0)g_mm3_frontend_icall_trace=1;
 if(strcmp(argv[1],"usb")==0||strcmp(argv[1],"bad_request")==0){
  g_eax=strcmp(argv[1],"usb")==0?0x740000:0x07FFFFFC;
  MEM32(g_esp)=0x35D048;MEM32(g_esp+4)=g_eax;MEM32(g_esp+8)=0x750000;
  MEM32(0x730000)=0x696C6E6F;MEM32(0x730004)=0x0000656E;
  MEM32(0x740000)=0x431C;MEM32(0x740004)=0xC0000001;
  MEM32(0x740008)=g_ecx;MEM32(0x74000C)=0x750000;target=g_ecx;
 }
 if(strcmp(argv[1],"bad_stack")==0){g_esp=0xFFFE;g_ebp=0x10;}
 snapshot(before);memcpy(before_memory,memory,sizeof memory);
 for(unsigned i=0;i<100;i++)recomp_icall_not_code_log(target);
 snapshot(after);assert(memcmp(before,after,sizeof before)==0);
 assert(memcmp(before_memory,memory,sizeof memory)==0);free(before_memory);
 assert(MEM32(0x392834)==0&&MEM32(0x720064)==0);
 return 0;
}
''')


def main():
    if not shutil.which('cl'):
        raise SystemExit('Run scripts/enter-env.ps1 first.')
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    scratch = ROOT / 'conformance_tmp'
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='icall-diagnostics-', dir=scratch) as temporary:
        directory = Path(temporary)
        shutil.copyfile(ROOT / 'src/mm3_recomp_types.h', directory / 'recomp_types.h')
        executable = cpu.compile_fixture(directory, 'diagnostics', fixture(image))
        for mode in ('default', 'trace', 'cascade', 'invalid', 'usb', 'bad_request', 'bad_stack'):
            result = subprocess.run([str(executable), mode], capture_output=True, text=True)
            assert result.returncode == 0, (mode, result)
            lines = [line for line in result.stderr.splitlines() if line.startswith('[ICALL]')]
            assert len(lines) == (100 if mode in ('trace', 'invalid') else 50), (mode, result)
            states = [line for line in result.stderr.splitlines() if line.startswith('[ICALL_STATE]')]
            assert len(states) == len(lines), (mode, result)
            assert [int(re.search(r'sample=(\d+)', line)[1]) for line in lines] == list(range(1, len(lines)+1))
            assert all(re.search(r'tick_ms=\d+', line) for line in lines + states)
            if mode == 'bad_stack':
                assert all('stack_read=0' in line and 'frame_read=0' in line for line in states)
            else:
                assert all('stack_read=1' in line and 'frame_base=0070FFE0 frame_read=1' in line
                           and '456789AB,12345678' in line for line in states)
            if mode in ('default', 'trace'):
                assert all('slot=0x00720064' in line and 'caller=0x001F635D' in line
                           and 'kind=memory base=eax slot_read=1' in line
                           and 'object_read=1 object_words=00720000' in line for line in lines)
            if mode == 'cascade':
                assert all('slot=0x00000070' in line and 'slot_read=0' in line
                           and 'object_read=0' in line for line in lines)
            if mode == 'invalid':
                assert all('object_read=0' in line for line in lines)
            requests = [line for line in result.stderr.splitlines() if line.startswith('[ICALL_REQUEST]')]
            if mode in ('usb', 'bad_request'):
                assert len(requests) == len(lines)
                assert all('kind=register base=ecx' in line and 'object_words=696C6E6F,0000656E'
                           in line for line in lines)
                assert all('source=stack_arg' in line for line in requests)
                if mode == 'usb':
                    assert all('request=00740000' in line
                               and 'request_read=1 size=28 kind=67 status=C0000001 callback=00730000 context=00750000'
                               in line for line in requests)
                else:
                    assert all('request=07FFFFFC' in line and 'request_read=0' in line for line in requests)
            else:
                assert not requests
        print('PASS: five original Xbox failing call sites and register/absolute/SIB/ESP '
              'operands resolve exact slots; null cascades, unknown operands, committed/'
              'uncommitted/noaccess/guard/boundary reads stay safe; diagnostics preserve '
              'guest registers and memory, cap at 50, and explicit tracing retains all calls. '
              'Stack/frame snapshots, monotonic timestamps and USB request state remain bounded '
              'and accurately separate callback bytes from the actual request.')


if __name__ == '__main__':
    main()

"""Exercise bounded indirect-call diagnostics using original Xbox call bytes.

Run inside `python scripts/devenv.py --shell` with tools/venv/Scripts/python.exe. The fork's
generic loggers and decoder (src/kernel/icall_diagnostics.c) are compiled directly, with
this title's caller-specific report hook taken from src/recomp_manual.c. It never
accesses a running game or save files.
"""
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import cc
import mm3_cpu_diagnostics_regression as cpu
import mm3_x87_exp_regression as common

ROOT = common.ROOT
CALLS = ((0x1F635D, 0x64), (0x14E6CB, 0x70), (0x14E732, 0x70),
         (0x14E7AF, 0x5C), (0x14E7CC, 0x5C))


def fixture(image):
    source = (ROOT / 'src/recomp_manual.c').read_text()
    # The loggers and decoder are the fork's, compiled from their own file. Only this
    # title's caller-specific report hook is taken from the title source.
    hook = re.search(r'static void mm3_icall_report\(const XboxIcallReport \*report\)\n\{.*?\n\}\n',
                     source, re.S)
    assert hook, 'mm3_icall_report'
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
            + 'static int g_mm3_frontend_icall_trace;\n'
            + 'ptrdiff_t xbox_GetMemoryOffset(void) { return g_xbox_mem_offset; }\n'
            + '#include "icall_diagnostics.c"\n'
            + hook.group() + r'''
static void check(uint32_t caller,uint32_t target,unsigned kind,uint32_t expected,unsigned reg){
 uint32_t slot=0xAAAAAAAA;unsigned base=99;
 assert(icall_decode(caller,target,&slot,&base)==kind);
 assert(slot==expected&&base==reg);
}
static void snapshot(uint32_t*s){
 s[0]=g_eax;s[1]=g_ecx;s[2]=g_edx;s[3]=g_ebx;
 s[4]=g_esp;s[5]=g_ebp;s[6]=g_esi;s[7]=g_edi;
}
int main(int argc,char**argv){
 uint32_t before[8],after[8],target=0;
 unsigned char*before_memory=(unsigned char*)malloc(sizeof memory);assert(before_memory);
 assert(argc==2);g_xbox_mem_offset=(ptrdiff_t)memory;
 xbox_icall_set_report_hook(mm3_icall_report);
 g_esp=0x700000;g_eax=0x720000;g_ecx=0x730000;g_edx=2;
 g_ebx=0x740000;g_ebp=0x710000;g_seh_ebp=0x710020;
 g_esi=0x23456789;g_edi=0x3456789A;
 MEM32(g_ebp)=0x456789AB;MEM32(g_ebp+4)=0x12345678;
 MEM32(g_esp)=0x1F635D;MEM32(g_ecx)=g_eax;
''' + '\n'.join(seed_calls) + tests + r'''
 if(strcmp(argv[1],"cascade")==0){g_eax=g_ecx=0;MEM32(g_esp)=0x14E6CB;}
 if(strcmp(argv[1],"invalid")==0){
  g_ecx=0x07FFFFFC;g_eax=g_esi=g_edi=0;
  MEM32(g_esp)=0xFB05E;g_mm3_frontend_icall_trace=1;
 }
 if(strcmp(argv[1],"trace")==0)g_mm3_frontend_icall_trace=1;
 xbox_icall_set_trace_all(g_mm3_frontend_icall_trace);
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
    cc.skip_unless_windows('the fixture includes windows.h')
    cc.require_compiler()
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    scratch = ROOT / 'conformance_tmp'
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='icall-diagnostics-', dir=scratch) as temporary:
        directory = Path(temporary)
        shutil.copyfile(ROOT / 'src/mm3_recomp_types.h', directory / 'recomp_types.h')
        shutil.copyfile(ROOT / "tools/xboxrecomp/templates/runtime/recomp_types.h", directory / "recomp_base.h")
        shutil.copyfile(ROOT / "tools/xboxrecomp/src/kernel/icall_diagnostics.c", directory / "icall_diagnostics.c")
        source = directory / 'diagnostics.c'
        source.write_text(fixture(image))
        executable = directory / ('diagnostics' + cc.exe_suffix())
        subprocess.run(cc.compile_command(
            [source], executable,
            includes=[directory, ROOT / 'tools/xboxrecomp/include', ROOT / 'tools/xboxrecomp/src',
                      ROOT / 'tools/xboxrecomp/src/kernel', ROOT / 'tools/xboxrecomp/src/nv2a'],
            defines=['_CRT_SECURE_NO_WARNINGS'], std='c11', optimize='none', warnings='high',
            werror=True, disable_warnings=('4102',), obj=directory / 'diagnostics.obj'),
            check=True, cwd=directory)
        for mode in ('default', 'trace', 'cascade', 'invalid', 'usb', 'bad_request', 'bad_stack'):
            result = subprocess.run([str(executable), mode], capture_output=True, text=True)
            assert result.returncode == 0, (mode, result)
            lines = [line for line in result.stderr.splitlines() if line.startswith('[ICALL] non-code')]
            assert len(lines) == (100 if mode in ('trace', 'invalid') else 50), (mode, result)
            summaries = [line for line in result.stderr.splitlines()
                         if line.startswith('[ICALL] suppressed')]
            assert len(summaries) == (0 if mode in ('trace', 'invalid') else 1), (mode, result)
            if summaries:
                assert 'failed-resolve=0 non-code=50' in summaries[0], (mode, result)
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
        print('PASS: five original Xbox failing call sites resolve exact slots through their caller, diagnostics preserve guest registers and memory, cap at 50, and explicit tracing retains all calls. Stack/frame snapshots, monotonic timestamps and USB request state remain bounded and accurately separate callback bytes from the actual request.')


if __name__ == '__main__':
    main()

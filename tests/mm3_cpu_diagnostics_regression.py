"""Compare the recovered CRT CPU probe with Xbox instructions and test logs.

Run inside `python scripts/devenv.py --shell` with tools/venv/Scripts/python.exe. Uses the
original XBE and Unicorn; it never opens a running game or a saved profile.
"""
import os
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

import cc

import mm3_x87_exp_regression as common

ROOT = common.ROOT
STACK = 0x700000
REGISTERS = ((common.x86.UC_X86_REG_EBX, 0x12345678),
             (common.x86.UC_X86_REG_ESI, 0x23456789),
             (common.x86.UC_X86_REG_EDI, 0x3456789A),
             (common.x86.UC_X86_REG_EBP, 0x456789AB))


def original_probe(image, cached, flags, direction):
    machine = common.unicorn.Uc(common.unicorn.UC_ARCH_X86, common.unicorn.UC_MODE_32)
    machine.mem_map(0, 0x1000000)
    for address, raw, size, _ in image.secs:
        machine.mem_write(address, bytes(image.d[raw:raw + size]))
    machine.mem_write(0x393004, struct.pack('<I', cached & 0xFFFFFFFF))
    machine.mem_write(STACK, struct.pack('<II', 0x600000, flags))
    machine.reg_write(common.x86.UC_X86_REG_ESP, STACK)
    machine.reg_write(common.x86.UC_X86_REG_EFLAGS, 0x202 | (direction << 10))
    for register, value in REGISTERS:
        machine.reg_write(register, value)

    def xbox_cpuid(uc, address, size, _):
        if address != 0x9379E:
            return
        # The only queried feature is MMX. Supply it independently of the host.
        uc.reg_write(common.x86.UC_X86_REG_EAX, 0x68A)
        uc.reg_write(common.x86.UC_X86_REG_EBX, 0)
        uc.reg_write(common.x86.UC_X86_REG_ECX, 0)
        uc.reg_write(common.x86.UC_X86_REG_EDX, 0x0383F9FD)
        uc.reg_write(common.x86.UC_X86_REG_EIP, address + size)

    machine.hook_add(common.unicorn.UC_HOOK_CODE, xbox_cpuid)
    machine.emu_start(0x93770, 0x600000, count=1000)
    assert machine.reg_read(common.x86.UC_X86_REG_EIP) == 0x600000
    return [machine.reg_read(common.x86.UC_X86_REG_EAX),
            machine.reg_read(common.x86.UC_X86_REG_ESP),
            *[machine.reg_read(register) for register, _ in REGISTERS],
            *struct.unpack('<I', machine.mem_read(0x393004, 4)),
            *struct.unpack('<I', machine.mem_read(0x46E580, 4)),
            (machine.reg_read(common.x86.UC_X86_REG_EFLAGS) >> 10) & 1]


COMMON_NATIVE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <emmintrin.h>
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp,g_fs_base;
RECOMP_TLS int g_df;
static unsigned char memory[0x1000000];
'''


def cpu_fixture():
    generated = (ROOT / 'src/recomp/gen/recomp_0008.c').read_text()
    original = re.search(r'void sub_00093770\(void\)\n\{.*?\n\}', generated, re.S).group()
    original = original.replace('sub_00093770(void)', 'original_generated_probe(void)')
    # The emitted fallback variable is unused, and generated code normally /w.
    original = original.replace('    int _flags = 0; /* fallback flag var */\n', '')
    original = re.sub(r'\b(eax|ebx|ecx|edx|esp|esi|edi)\b', r'g_\1', original)
    aliases = ''.join(f'#define {r} g_{r}\n' for r in ('eax', 'ebx', 'ecx', 'edx'))
    unaliases = ''.join(f'#undef {r}\n' for r in ('eax', 'ebx', 'ecx', 'edx'))
    original = (aliases + '#pragma warning(push)\n#pragma warning(disable:4102 4127)\n'
                + original + '\n#pragma warning(pop)\n' + unaliases)
    return (COMMON_NATIVE + '\n' + (ROOT / 'src/mm3_cpu_compat.c').read_text()
            + '\nvoid recomp_unimpl(const char*text,uint32_t va){(void)text;(void)va;}\n'
            + original + r'''
static unsigned char expected_memory[sizeof(memory)];
static void init_cpu(char**argv){
 memset(memory,0,sizeof(memory));
 MEM32(0x393004)=(uint32_t)strtol(argv[1],0,0);
 g_esp=0x700000;MEM32(g_esp)=0x600000;MEM32(g_esp+4)=(uint32_t)strtoul(argv[2],0,0);
 g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;g_ebp=0x456789AB;
 g_seh_ebp=0x56789ABC;g_ecx=0x321;g_edx=0x432;g_df=atoi(argv[3]);
}
static void snapshot(uint32_t *s){
 s[0]=g_eax;s[1]=g_ecx;s[2]=g_edx;s[3]=g_esp;s[4]=g_ebx;
 s[5]=g_esi;s[6]=g_edi;s[7]=g_ebp;s[8]=g_seh_ebp;s[9]=(uint32_t)g_df;
}
int main(int argc,char**argv){
 uint32_t expected[10],actual[10];
 assert(argc==4);g_xbox_mem_offset=(ptrdiff_t)memory;
 init_cpu(argv);original_generated_probe();snapshot(expected);
 memcpy(expected_memory,memory,sizeof(memory));
 init_cpu(argv);
 sub_00093770();
 snapshot(actual);
 assert(memcmp(expected,actual,sizeof(expected))==0);
 assert(memcmp(expected_memory,memory,sizeof(memory))==0);
 assert(g_ebp==0x6ffffc && g_seh_ebp==0x6ffffc);
 assert(MEM32(0x700000)==0x600000 && MEM32(0x700004)==(uint32_t)strtoul(argv[2],0,0));
 /* EBP remains local in generated code; the saved word is the x86 register. */
 printf("%u %u %u %u %u %u %u %u %u\n",g_eax,g_esp,g_ebx,g_esi,g_edi,MEM32(0x6ffffc),
        MEM32(0x393004),MEM32(0x46E580),g_df);return 0;
}
''')


def diagnostics_fixture():
    source = (ROOT / 'src/recomp_manual.c').read_text()
    # Compile the complete implementation and its private dependencies together.
    # Copying individual function bodies hid dependencies as diagnostics evolved.
    implementation = source[source.index('static int g_mm3_unimpl_trap;'):
                            source.index('extern void sub_000838C3(void);')]
    # The slice includes the configure hook-up; the runtime's logger module is not linked here.
    stubs = ('typedef struct { int unused; } XboxIcallReport;\n'
             'void xbox_icall_set_trace_all(int enabled) { (void)enabled; }\n'
             'void xbox_icall_set_report_hook(void (*hook)(const XboxIcallReport *)) { (void)hook; }\n'
             'static void mm3_icall_report(const XboxIcallReport *report) { (void)report; }\n')
    return (COMMON_NATIVE + '\n' + stubs + implementation + r'''
int main(int argc,char**argv){
 assert(argc==2);g_xbox_mem_offset=(ptrdiff_t)memory;
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 mm3_configure_diagnostics();
 if(strcmp(argv[1],"trap")==0){recomp_unimpl("cli",0x35F882);return 99;}
 if(strcmp(argv[1],"handled")==0){
  g_esp=0x700000;g_eax=0x1;g_ecx=0x2;g_edx=0x80C0;g_ebx=0x4;g_df=1;
  for(unsigned i=0;i<100000;i++){
   recomp_unimpl("cli",0x35F882);recomp_unimpl("sti",0x35F893);
   recomp_unimpl("out dx, al",0x3475DC);recomp_unimpl("wbinvd",0x345E9D);
  }
  assert(g_esp==0x700000&&g_eax==1&&g_ecx==2&&g_edx==0x80C0&&g_ebx==4&&g_df==1);
 }else if(strcmp(argv[1],"wrong-port")==0){
  g_edx=0x80C1;recomp_unimpl("out dx, al",0x3475DC);return 99;
 }else if(strcmp(argv[1],"wrong-instruction")==0){
  recomp_unimpl("hlt",0x35F882);return 99;
 }else{
  recomp_unimpl("cli",0x123456);return 99;
 }
 return 0;
}
''')


def compile_fixture(directory, name, source, disable_warnings=()):
    path = directory / (name + '.c')
    executable = directory / (name + cc.exe_suffix())
    path.write_text(source)
    subprocess.run(cc.compile_command(
        [path], executable, includes=[directory, ROOT / 'tools/xboxrecomp/include'],
        defines=['_CRT_SECURE_NO_WARNINGS'], std='c11', optimize='none', warnings='high',
        werror=True, disable_warnings=disable_warnings, obj=directory / (name + '.obj')), check=True)
    return executable


def main():
    cc.skip_unless_windows('the fixture includes windows.h')
    cc.require_compiler()
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    (ROOT / 'conformance_tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cpu-diagnostics-', dir=ROOT / 'conformance_tmp') as temporary:
        directory = Path(temporary)
        shutil.copyfile(ROOT / 'src/mm3_recomp_types.h', directory / 'recomp_types.h')
        shutil.copyfile(ROOT / "tools/xboxrecomp/templates/runtime/recomp_types.h", directory / "recomp_base.h")
        cpu = compile_fixture(directory, 'cpu', cpu_fixture())
        cases = 0
        for cached in (-1, -2, 0, 1):
            for flags in (0, 0x40000, 0x40001, 0xFFFFFFFF):
                for direction in (0, 1):
                    output = subprocess.check_output([str(cpu), str(cached), str(flags), str(direction)], text=True)
                    expected = original_probe(image, cached, flags, direction)
                    assert list(map(int, output.split())) == expected
                    cases += 1
        diagnostics = compile_fixture(directory, 'diagnostics', diagnostics_fixture())
        diagnostic_env = dict(os.environ)
        diagnostic_env.pop('RECOMP_UNIMPL_TRAP', None)
        diagnostic_env.pop('MM3_FRONTEND_ICALL_TRACE', None)
        result = subprocess.run([str(diagnostics), 'handled'], capture_output=True, text=True, env=diagnostic_env)
        assert result.returncode == 0 and result.stderr.count('[UNIMPL] known no-op') == 4, result
        for mode, address in (('unknown', '0x00123456'), ('wrong-port', '0x003475DC'),
                              ('wrong-instruction', '0x0035F882')):
            result = subprocess.run([str(diagnostics), mode], capture_output=True, text=True, env=diagnostic_env)
            assert result.returncode not in (0, 99) and address in result.stderr and 'stopping' in result.stderr, result
        trap_env = dict(diagnostic_env, RECOMP_UNIMPL_TRAP='1')
        result = subprocess.run([str(diagnostics), 'trap'], capture_output=True, text=True, env=trap_env)
        assert result.returncode not in (0, 99) and '0x0035F882' in result.stderr, result
        print(f'PASS: {cases} CPU-probe cases match original Xbox instructions and the generated baseline. Known hardware sites preserve registers and log once; unknown instructions and mismatched known-site operands stop execution; explicit traps stop known sites too.')


if __name__ == '__main__':
    main()

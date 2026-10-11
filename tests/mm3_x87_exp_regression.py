"""Compare production MM3 exp with original XBE instructions.

Run with tools/venv/Scripts/python.exe after scripts/enter-env.ps1.
Requires capstone and unicorn. Scratch files stay under conformance_tmp;
the test never reads or changes a running game or save files.
"""
import math
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import unicorn
from unicorn import x86_const as x86

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/xboxrecomp'))
from tools.rtti.rtti import Image

ENTRIES = (0x25371C, 0x9B8C2, 0x9B3DC, 0x9BB15, 0x9BC52,
           0x9BC04, 0x9BC35, 0x9B4E2, 0x9B4D4, 0x9B4E9, 0x9B94E)
VALUES = (-1000000.0, -1000.0, -100.0, -10.0, -1.0, -0.01, -0.0,
          0.0, 0.01, 1.0, 10.0, 100.0, 1000.0, 1000000.0)
SENTINELS = (123.25, -91.5)


def native_source():
    found = {}
    for path in sorted((ROOT / 'src/recomp/gen').glob('recomp_*.c')):
        for match in re.finditer(r'void (sub_\w+)\(void\)\n\{.*?(?=\n/\*\*|\Z)',
                                 path.read_text(), re.S):
            found[match.group(1)] = match.group(0)
    bodies = {f'sub_{va:08X}': found[f'sub_{va:08X}'] for va in ENTRIES}
    references = set()
    for body in bodies.values():
        references.update(re.findall(r'RECOMP_ABI_CALL\(0x\w+u, (sub_\w+)\)', body))
        references.update(re.findall(r'(sub_\w+)\(\)', body))
    stubs = references - bodies.keys()
    declarations = '\n'.join(f'void {name}(void);' for name in bodies.keys() | stubs)
    # Omit only the CRT error-reporting callback in both executions. The
    # caller retains its argument cleanup; the callback cannot alter exp.
    stub_code = '\n'.join(
        ('void sub_0009E5D4(void){g_esp+=4;g_eax=0;}' if name == 'sub_0009E5D4'
         else f'void {name}(void){{fprintf(stderr,"unexpected {name}\\n");exit(99);}}')
        for name in stubs)
    return r'''
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
#include "mm3_recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
uint32_t g_xbox_code_lo=0x12000,g_xbox_code_hi=0x260000;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp,g_fs_base,g_eflags;
RECOMP_TLS int g_fp_top,g_df,g_fp_cmp;
RECOMP_TLS uint16_t g_fp_control_word,g_fp_cc;
RECOMP_TLS double g_fp_stack[8];
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_trace_idx;
volatile uint64_t g_icall_count;
''' + declarations + '\n' + stub_code + '\n' + '\n'.join(bodies.values()) + r'''
recomp_func_t recomp_lookup(uint32_t va){
    if(va==0x9BB15)return sub_0009BB15;
    if(va==0x9B4E9)return sub_0009B4E9;
    fprintf(stderr,"unexpected dispatch %X\n",va);exit(99);return 0;
}
recomp_func_t recomp_lookup_kernel(uint32_t va){return 0;}
recomp_func_t recomp_lookup_manual(uint32_t va){return 0;}
void recomp_icall_fail_log(uint32_t va){exit(99);}
int main(int argc,char**argv){
    unsigned char* ram=(unsigned char*)malloc(0x1000000);
    FILE* in=fopen(argv[1],"rb");if(!in)return 2;
    if(fread(ram,1,0x1000000,in)!=0x1000000)return 3;fclose(in);
    g_xbox_mem_offset=(ptrdiff_t)ram;
    g_fp_control_word=(uint16_t)strtoul(argv[2],0,0);
    MEM32(0x393940)=1;MEM32(0x3BFFEC)=0;
    g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;
    g_ebp=0x456789AB;g_seh_ebp=g_ebp;
    g_esp=0x700000;MEM32(g_esp)=0x600000;
    g_fp_top=5;g_fp_stack[5]=strtod(argv[3],0);
    g_fp_stack[6]=-91.5;g_fp_stack[7]=123.25;
    if(argc>4){
        g_seh_ebp=g_ebp=0x710000;sub_0009BC35();
        if(MEM8(g_ebp-144)!=3)return 4;
    }
    else sub_0025371C();
    printf("%.17g %.17g %.17g %u %u %u %u %u %u\n",
           g_fp_stack[g_fp_top],g_fp_stack[(g_fp_top+1)&7],g_fp_stack[(g_fp_top+2)&7],
           g_fp_top,g_esp,g_fp_control_word,g_ebx,g_esi,g_edi);
    return 0;
}
'''


def original_result(ram, value, control, entry=0x25371C):
    machine = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    machine.mem_map(0, len(ram))
    machine.mem_write(0, bytes(ram))
    machine.mem_write(0x393940, struct.pack('<I', 1))
    machine.mem_write(0x3BFFEC, struct.pack('<I', 0))
    # Seed and observe the stack with original FLD/FSTP instructions.
    machine.mem_write(0x600100, struct.pack('<H', control))
    machine.mem_write(0x600110, struct.pack('<ddd', *SENTINELS, value))
    code = bytearray(b'\xdb\xe3\xd9\x2d' + struct.pack('<I', 0x600100))
    for address in (0x600110, 0x600118, 0x600120):
        code.extend(b'\xdd\x05' + struct.pack('<I', address))
    call = 0x600000 + len(code)
    code.extend(b'\xe8' + struct.pack('<i', entry - call - 5))
    for address in (0x600130, 0x600138, 0x600140):
        code.extend(b'\xdd\x1d' + struct.pack('<I', address))
    code.extend(b'\xd9\x3d' + struct.pack('<I', 0x600148))
    machine.mem_write(0x600000, bytes(code))
    for register, initial in ((x86.UC_X86_REG_ESP, 0x700004),
                              (x86.UC_X86_REG_EBP, 0x710000 if entry==0x9BC35 else 0x456789AB),
                              (x86.UC_X86_REG_EBX, 0x12345678),
                              (x86.UC_X86_REG_ESI, 0x23456789),
                              (x86.UC_X86_REG_EDI, 0x3456789A)):
        machine.reg_write(register, initial)

    def omit_reporting_callback(uc, address, size, _):
        if address == 0x9E5D4:
            sp = uc.reg_read(x86.UC_X86_REG_ESP)
            target = struct.unpack('<I', uc.mem_read(sp, 4))[0]
            uc.reg_write(x86.UC_X86_REG_ESP, sp + 4)
            uc.reg_write(x86.UC_X86_REG_EAX, 0)
            uc.reg_write(x86.UC_X86_REG_EIP, target)

    machine.hook_add(unicorn.UC_HOOK_CODE, omit_reporting_callback)
    machine.emu_start(0x600000, 0x600000 + len(code), count=10000)
    assert machine.reg_read(x86.UC_X86_REG_EIP) == 0x600000 + len(code)
    values = struct.unpack('<ddd', machine.mem_read(0x600130, 24))
    restored_control = struct.unpack('<H', machine.mem_read(0x600148, 2))[0]
    assert values[1:] == SENTINELS[::-1], (value, values)
    assert machine.reg_read(x86.UC_X86_REG_ESP) == 0x700004
    assert restored_control == control
    for register, initial in ((x86.UC_X86_REG_EBX, 0x12345678),
                              (x86.UC_X86_REG_ESI, 0x23456789),
                              (x86.UC_X86_REG_EDI, 0x3456789A)):
        assert machine.reg_read(register) == initial
    assert (machine.reg_read(x86.UC_X86_REG_FPSW) >> 11) & 7 == 0
    if entry == 0x9BC35:
        assert machine.mem_read(0x710000-144,1) == b'\x03'
    return values[0]


def main():
    if not shutil.which('cl'):
        raise SystemExit('Run scripts/enter-env.ps1 first so cl.exe is available.')
    image = Image(str(ROOT / 'game_files/default.xbe'))
    ram = bytearray(0x1000000)
    for address, raw, size, _ in image.secs:
        ram[address:address+size] = image.d[raw:raw+size]
    scratch = ROOT / 'conformance_tmp'
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='x87-exp-', dir=scratch) as directory:
        directory = Path(directory)
        source, executable = directory / 'exp.c', directory / 'exp.exe'
        source.write_text(native_source())
        memory = directory / 'image.bin'
        memory.write_bytes(ram)
        subprocess.run(['cl', '/nologo', '/Od', '/MD', '/std:c11',
                        '/I' + str(ROOT / 'src'), '/I' + str(ROOT / 'tools/xboxrecomp/include'),
                        '/Fo' + str(directory / 'exp.obj'), '/Fe' + str(executable), str(source)], check=True)
        count = 0
        for control in (0x7F, 0x27F, 0x37F, 0xC7F):
            for value in VALUES:
                result = subprocess.run([str(executable), str(memory), hex(control), str(value)],
                                        capture_output=True, text=True, check=True).stdout.split()
                actual, second, third = map(float, result[:3])
                top, stack, restored_control, ebx, esi, edi = map(int, result[3:])
                assert (second, third) == SENTINELS[::-1], (value, result)
                assert (top, stack, restored_control) == (5, 0x700004, control), (value, result)
                assert (ebx, esi, edi) == (0x12345678, 0x23456789, 0x3456789A), (value, result)
                # Unicorn 2.1.4 wraps large FSCALE exponents (e.g. +/-1000).
                # Check those two double-range limits against math instead;
                # ordinary inputs and +/-1e6 helper tails execute the XBE.
                if abs(value) != 1000:
                    original = original_result(ram, value, control)
                    assert (actual == original or math.isclose(actual, original, rel_tol=2e-6, abs_tol=1e-300)), (value, control, actual, original)
                else:
                    assert actual == (0.0 if value<0 else math.inf), (value, actual)
                if abs(value) < 710:
                    assert math.isclose(actual, math.exp(value), rel_tol=2e-6, abs_tol=1e-300), (value, actual)
                count += 1
            result = subprocess.run([str(executable), str(memory), hex(control), '2', 'overflow-helper'],
                                    capture_output=True, text=True, check=True).stdout.split()
            actual, second, third = map(float, result[:3])
            assert actual == original_result(ram, 2.0, control, entry=0x9BC35) == math.inf
            assert (second, third) == SENTINELS[::-1]
            assert tuple(map(int,result[3:])) == (5,0x700004,control,0x12345678,0x23456789,0x3456789A)
        print(f'PASS: {count} native exp cases (48 original-XBE comparisons, 8 double-range limits), plus 4 original-XBE overflow-helper cases; x87 stack, ESP, registers, and control word preserved.')


if __name__ == '__main__':
    main()

"""Compare graph edge lookup and its caller with original XBE instructions.

Run with tools/venv/Scripts/python.exe after scripts/enter-env.ps1.
The actual generated lookup, shared tail, index helpers and vector caller run
against the original XBE in Unicorn. An isolated old-tail negative control
must reproduce the stack/register defect. No game or save files are written.
"""
import math
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

import mm3_x87_exp_regression as common

ROOT = common.ROOT
ENTRIES = (0x144EAE, 0x144EDE, 0x144ED9, 0x144FCB, 0x144FAE,
           0x145069, 0x1F6349, 0x14E6B9, 0x138439, 0x138CED,
           0x144EE4, 0x52348)
RAM_SIZE, STACK, RETURN = 0x1000000, 0x700000, 0x600000
NODE, EDGES, INDICES, CONFIG, OUTPUT = 0x720000, 0x730000, 0x740000, 0x750000, 0x760000
NEIGHBORS = (NODE + 0x200, NODE + 0x400, NODE + 0x600)
EDGE_IDS = (5, 2, 9)  # Deliberately not contiguous or sorted.
REGISTERS = ('EAX', 'ECX', 'EDX', 'ESP', 'EBX', 'ESI', 'EDI', 'EBP')
INITIAL = (0x11223344, NODE, 0x55667788, STACK,
           0x12345678, 0x23456789, 0x3456789A, 0x456789AB)


def native_source(old_tail=False):
    found = {}
    # A recovered entry may be the final function before a dispatch table;
    # stop at its own closing brace instead of copying that table as well.
    pattern = r'void (sub_\w+)\(void\)\n\{.*?\n\}'
    for path in sorted((ROOT / 'src/recomp/gen').glob('recomp_*.c')):
        for match in re.finditer(pattern, path.read_text(), re.S):
            found[match.group(1)] = match.group(0)
    bodies = {f'sub_{va:08X}': found[f'sub_{va:08X}'] for va in ENTRIES}
    if old_tail:
        # Exact former unresolved stub. Only the temporary fixture changes.
        bodies['sub_00144ED9'] = 'void sub_00144ED9(void){g_esp += 8;}'
    references = set()
    for body in bodies.values():
        references.update(re.findall(r'RECOMP_ABI_CALL\(0x\w+u, (sub_\w+)\)', body))
        references.update(re.findall(r'(sub_\w+)\(\)', body))
    stubs = references - bodies.keys()
    code = r'''
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
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_trace_idx;
volatile uint64_t g_icall_count;
'''
    code += '\n'.join(f'void {name}(void);' for name in sorted(bodies.keys() | stubs))
    code += '\n' + '\n'.join(
        f'void {name}(void){{fprintf(stderr,"unexpected {name}\\n");exit(99);}}'
        for name in sorted(stubs))
    code += '\n' + '\n'.join(bodies.values())
    code += '\nrecomp_func_t recomp_lookup(uint32_t va){switch(va){\n'
    code += ''.join(f'case 0x{va:X}:return sub_{va:08X};\n' for va in ENTRIES)
    code += r'''
default:fprintf(stderr,"unexpected dispatch %X\n",va);exit(99);return 0;}}
recomp_func_t recomp_lookup_kernel(uint32_t va){return 0;}
recomp_func_t recomp_lookup_manual(uint32_t va){return 0;}
void recomp_icall_fail_log(uint32_t va){exit(99);}
void recomp_icall_not_code_log(uint32_t va){exit(99);}
int main(int argc,char**argv){
 unsigned char*ram=(unsigned char*)malloc(0x1000000);
 FILE*in=fopen(argv[1],"rb");if(!in)return 2;
 if(fread(ram,1,0x1000000,in)!=0x1000000)return 3;fclose(in);
 g_xbox_mem_offset=(ptrdiff_t)ram;
 g_eax=MEM32(0x750000);g_ecx=MEM32(0x750004);g_edx=MEM32(0x750008);
 g_esp=MEM32(0x75000C);g_ebx=MEM32(0x750010);g_esi=MEM32(0x750014);
 g_edi=MEM32(0x750018);g_ebp=g_seh_ebp=MEM32(0x75001C);
 g_fp_control_word=0x37F;
 recomp_lookup((uint32_t)strtoul(argv[2],0,0))();
 FILE*out=fopen(argv[3],"wb");if(!out)return 4;
 fwrite(ram,1,0x1000000,out);fclose(out);
 printf("%u %u %u %u %u %u %u %u\n",g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp);
 return 0;
}
'''
    return code


def fixture(image, entry, match=None, empty=None, reverse=False):
    ram = bytearray(RAM_SIZE)
    for address, raw, size, _ in image.secs:
        ram[address:address + size] = image.d[raw:raw + size]
    put = lambda address, *values: struct.pack_into('<' + 'I' * len(values), ram, address, *values)
    # Keep the retail vtables intact, including their real type and position
    # methods, rather than replacing graph methods with synthetic callbacks.
    for vtable, type_method in ((0x37E838, 0x138439), (0x37EA48, 0x138CED)):
        for offset, method in ((0x5C, 0x144EE4), (0x60, 0x144FCB),
                               (0x64, 0x144EAE), (0x68, 0x145069),
                               (0x6C, 0x144FAE), (0x70, type_method)):
            assert struct.unpack_from('<I', ram, vtable + offset)[0] == method
    positions = ((10.0, -2.0, 4.0), (13.0, 2.0, 16.0),
                 (4.0, 1.0, 6.0), (1.0, -14.0, 24.0))
    for index, node in enumerate((NODE,) + NEIGHBORS):
        put(node, 0x37E838 if index % 2 == 0 else 0x37EA48)
        struct.pack_into('<fff', ram, node + 0x34, *positions[index])
    put(NODE + 0x50, 0 if empty == 'null' else INDICES,
        0 if empty == 'null' else INDICES + (0 if empty else 6))
    put(0x3C5730, EDGES)
    struct.pack_into('<HHH', ram, INDICES, *EDGE_IDS)
    for index, neighbor in enumerate(NEIGHBORS):
        pair = (neighbor, NODE) if bool(index % 2) ^ reverse else (NODE, neighbor)
        put(EDGES + EDGE_IDS[index] * 8, *pair)
    query = NEIGHBORS[match] if match is not None else NODE + 0x800
    registers = list(INITIAL)
    arguments = (query,)
    if entry == 0x1F6349:
        registers[0], registers[4] = NODE, OUTPUT
        # The first stack argument is not consumed on this route branch.
        arguments = (0xA1B2C3D4, query)
    put(CONFIG, *registers)
    put(STACK, RETURN, *arguments)
    ram[OUTPUT:OUTPUT + 16] = b'\xA5' * 16
    return ram


def original_result(ram, entry):
    machine = common.unicorn.Uc(common.unicorn.UC_ARCH_X86, common.unicorn.UC_MODE_32)
    machine.mem_map(0, len(ram))
    machine.mem_write(0, bytes(ram))
    for name, value in zip(REGISTERS, struct.unpack_from('<8I', ram, CONFIG)):
        machine.reg_write(getattr(common.x86, 'UC_X86_REG_' + name), value)
    # Execute FNINIT so original x87/SSE normalization has the same initial
    # floating-point state as the compiled fixture.
    machine.mem_write(RETURN + 0x100, b'\xDB\xE3')
    machine.emu_start(RETURN + 0x100, RETURN + 0x102)
    machine.emu_start(entry, RETURN, count=10000)
    assert machine.reg_read(common.x86.UC_X86_REG_EIP) == RETURN
    registers = tuple(machine.reg_read(getattr(common.x86, 'UC_X86_REG_' + name))
                      for name in REGISTERS)
    return registers, bytes(machine.mem_read(0, len(ram)))


def native_result(executable, directory, ram, entry):
    memory, output = directory / 'image.bin', directory / 'result.bin'
    memory.write_bytes(ram)
    result = subprocess.check_output([str(executable), str(memory), hex(entry), str(output)], text=True)
    return tuple(map(int, result.split())), output.read_bytes()


def compare(label, entry, initial, native, original):
    native_regs, native_ram = native
    original_regs, original_ram = original
    # Standard-frame lifts keep architectural EBP local; g_ebp publishes the
    # latest nested frame for frameless callees, as in the existing native tests.
    assert native_regs[:7] == original_regs[:7], (label, REGISTERS[:7], native_regs, original_regs)
    assert original_regs[3] == STACK + (12 if entry == 0x1F6349 else 8), label
    assert original_regs[4:8] == struct.unpack_from('<8I', initial, CONFIG)[4:8], label
    if entry == 0x144EAE:
        assert native_regs[7] == original_regs[7], label
    for start, end in ((STACK - 0x100, STACK + 0x20),
                       (0x3C5730, 0x3C5734), (NODE, INDICES + 6),
                       (OUTPUT, OUTPUT + 16)):
        assert native_ram[start:end] == original_ram[start:end], (label, hex(start), hex(end))
    assert native_ram[NODE:INDICES + 6] == initial[NODE:INDICES + 6], label
    assert native_ram[OUTPUT + 12:OUTPUT + 16] == b'\xA5' * 4, label


def main():
    if not shutil.which('cl'):
        raise SystemExit('Run scripts/enter-env.ps1 first.')
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    scratch = ROOT / 'conformance_tmp'
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='graph-edge-', dir=scratch) as path:
        directory = Path(path)
        executables = []
        for old_tail in (False, True):
            name = 'old-tail' if old_tail else 'repaired-tail'
            source, executable = directory / (name + '.c'), directory / (name + '.exe')
            source.write_text(native_source(old_tail))
            subprocess.run(['cl', '/nologo', '/Od', '/MD', '/std:c11',
                            '/I' + str(ROOT / 'src'), '/I' + str(ROOT / 'tools/xboxrecomp/include'),
                            '/Fo' + str(directory / (name + '.obj')), '/Fe' + str(executable), str(source)], check=True)
            executables.append(executable)
        cases = [('lookup-empty-null', 0x144EAE, {'empty': 'null'}),
                 ('lookup-empty-allocated', 0x144EAE, {'empty': 'allocated'}),
                 ('lookup-no-match', 0x144EAE, {})]
        cases += [(f'lookup-match-{index}', 0x144EAE, {'match': index}) for index in range(3)]
        cases += [(f'outer-match-{index}-reverse-{reverse}', 0x1F6349,
                   {'match': index, 'reverse': reverse}) for index in range(3) for reverse in (False, True)]
        controls = 0
        for label, entry, options in cases:
            ram = fixture(image, entry, **options)
            original = original_result(ram, entry)
            native = native_result(executables[0], directory, ram, entry)
            compare(label, entry, ram, native, original)
            match = options.get('match')
            if entry == 0x144EAE:
                expected = 0 if match is None else EDGES + EDGE_IDS[match] * 8
                assert native[0][0] == expected, label
                assert native[1][OUTPUT:OUTPUT + 16] == b'\xA5' * 16, label
            else:
                source = struct.unpack_from('<fff', ram, NODE + 0x34)
                target = struct.unpack_from('<fff', ram, NEIGHBORS[match] + 0x34)
                delta = tuple(b - a for a, b in zip(source, target))
                length = math.sqrt(sum(value * value for value in delta))
                actual = struct.unpack_from('<fff', native[1], OUTPUT)
                assert all(math.isclose(a, b / length, abs_tol=1e-7)
                           for a, b in zip(actual, delta)), (label, actual, delta)
            if label in ('lookup-match-0', 'outer-match-0-reverse-False'):
                broken = native_result(executables[1], directory, ram, entry)
                try:
                    compare(label, entry, ram, broken, original)
                except AssertionError:
                    pass
                else:
                    raise AssertionError('Old unresolved tail unexpectedly passed: ' + label)
                if entry == 0x144EAE:
                    assert broken[0][3] == original[0][3] - 8, broken[0]
                else:
                    assert broken[0][3] == original[0][3] and broken[0][5:7] != original[0][5:7], broken[0]
                controls += 1
        assert controls == 2
        print(f'PASS: {len(cases)} native/original-XBE graph cases; empty/no-match/first/middle/last, both edge orientations, return registers, stack bytes and normalized output. Two old-tail negative controls reproduce stack imbalance and outer ESI/EDI corruption.')


if __name__ == '__main__':
    main()

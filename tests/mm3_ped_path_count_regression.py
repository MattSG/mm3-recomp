"""Compare the recovered pedestrian path-count helper with the original XBE.

The former unresolved stub is a negative control. No live game, profile or
capture is needed; the captured taxi path's five-point layout is reconstructed
without copying game assets. Run after scripts/enter-env.ps1.
"""
import json
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

import mm3_x87_exp_regression as common

ROOT = common.ROOT
RAM_SIZE, STACK, RETURN = 0x1000000, 0x700000, 0x600000
NODE, POINTS = 0x720000, 0x730000
ENTRIES = (0x6726A, 0x135B2A)
REGISTERS = ('EAX', 'ECX', 'EDX', 'ESP', 'EBX', 'ESI', 'EDI', 'EBP')
INITIAL = (NODE + 0x14, NODE, 0x55667788, STACK,
           0x12345678, 0x23456789, 0x3456789A, 0x456789AB)


def gen_function(name):
    """Generated chunks are renumbered between translator versions: search them all."""
    head = 'void ' + name + '(void)\n{'
    for path in sorted((ROOT / 'src/recomp/gen').glob('recomp_*.c')):
        text = path.read_text()
        start = text.find(head)
        if start >= 0:
            return text[start:text.index('\n}', start) + 2]
    raise AssertionError(name)


def native_source(old=False):
    bodies = []
    for address in (0x6726A, 0x4DBC7, 0x135B2A):
        body = gen_function(f'sub_{address:08X}')
        bodies.append('void sub_0006726A(void){g_esp+=4;}'
                      if old and address == 0x6726A else body)
    return r'''
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
#include "mm3_recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS int g_df;
void sub_0006726A(void);void sub_0004DBC7(void);void sub_00135B2A(void);
/* The translator now emits the 135B2A -> 6726A tail jump as an indirect tail
   call through the function table; resolve it directly in the harness. */
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(va) do { if ((va) == 0x6726Au) sub_0006726A(); } while (0)
''' + '\n'.join(bodies) + r'''
int main(int argc,char**argv){
 unsigned char*ram=(unsigned char*)malloc(0x1000000);
 FILE*in=fopen(argv[1],"rb");if(!in)return 2;
 if(fread(ram,1,0x1000000,in)!=0x1000000)return 3;fclose(in);
 g_xbox_mem_offset=(ptrdiff_t)ram;
 g_eax=0x720014;g_ecx=0x720000;g_edx=0x55667788;g_esp=0x700000;
 g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;
 g_ebp=g_seh_ebp=0x456789AB;
 if(strtoul(argv[2],0,0)==0x6726A)sub_0006726A();else sub_00135B2A();
 printf("%u %u %u %u %u %u %u %u\n",g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp);
 return 0;
}
'''


def fixture(image, count, empty=False):
    ram = bytearray(RAM_SIZE)
    for address, raw, size, _ in image.secs:
        ram[address:address+size] = image.d[raw:raw+size]
    # 135B2A passes the address of its path object slot at node+14. The
    # captured taxi node's slot pointed back to itself and held five points.
    struct.pack_into('<I', ram, NODE + 0x14, NODE)
    struct.pack_into('<II', ram, NODE + 8,
                     0 if empty else POINTS, POINTS + count * 12)
    struct.pack_into('<I', ram, STACK, RETURN)
    return ram


def original_result(ram, entry):
    machine = common.unicorn.Uc(common.unicorn.UC_ARCH_X86, common.unicorn.UC_MODE_32)
    machine.mem_map(0, len(ram)); machine.mem_write(0, bytes(ram))
    for register, value in zip(REGISTERS, INITIAL):
        machine.reg_write(getattr(common.x86, 'UC_X86_REG_' + register), value)
    machine.emu_start(entry, RETURN, count=1000)
    assert machine.reg_read(common.x86.UC_X86_REG_EIP) == RETURN
    return tuple(machine.reg_read(getattr(common.x86, 'UC_X86_REG_' + register))
                 for register in REGISTERS)


def main():
    if not shutil.which('cl'):
        raise SystemExit('Run scripts/enter-env.ps1 first.')
    entries = json.loads((ROOT / 'mm3_runtime_entry_repairs.json').read_text())
    entry = next(item for item in entries if item['start'] == '0x0006726A')
    assert entry['end'] == '0x0006727D'
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    scratch = ROOT / 'conformance_tmp'; scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ped-path-count-', dir=scratch) as temporary:
        folder = Path(temporary)
        executables = []
        for old in (False, True):
            source = folder / ('old.c' if old else 'fixed.c')
            executable = source.with_suffix('.exe')
            source.write_text(native_source(old))
            subprocess.run(['cl', '/nologo', '/Od', '/MD', '/std:c11',
                            '/I' + str(ROOT / 'src'), '/I' + str(ROOT / 'tools/xboxrecomp/include'),
                            '/Fo' + str(source.with_suffix('.obj')), '/Fe' + str(executable),
                            str(source)], check=True, cwd=folder)
            executables.append(executable)
        memory = folder / 'image.bin'
        cases = 0
        for entry_address in ENTRIES:
            for count, empty in [(n, False) for n in (0, 1, 2, 3, 4, 5, 12, 64, 4096)] + [(0, True)]:
                ram = fixture(image, count, empty); memory.write_bytes(ram)
                original = original_result(ram, entry_address)
                actual = tuple(map(int, subprocess.run(
                    [str(executables[0]), str(memory), hex(entry_address)],
                    capture_output=True, text=True, check=True).stdout.split()))
                assert actual == original, (entry_address, count, empty, actual, original)
                assert actual[0] == max(count, 4), (count, actual)
                assert actual[3:] == (STACK + 4, *INITIAL[4:]), actual
                cases += 1
        # The exact former stub returns a pointer as the count. The captured
        # segment index twelve consequently appeared to be inside five points.
        memory.write_bytes(fixture(image, 5))
        old = tuple(map(int, subprocess.run(
            [str(executables[1]), str(memory), hex(0x135B2A)],
            capture_output=True, text=True, check=True).stdout.split()))
        assert old[0] == NODE + 0x14 and old[0] != 5
        assert 12 < old[0] and not 12 < 5
    print(f'PASS pedestrian path count: {cases} original-XBE helper/tail comparisons, '
          'empty/minimum/five-point/long paths, exact register/stack ABI, '
          'and former pointer-count overshoot reproduced')


if __name__ == '__main__':
    main()

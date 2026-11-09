"""Compare pedestrian arrival with the retail Xbox routine, without a game run."""
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

import mm3_x87_exp_regression as common

ROOT = common.ROOT
ENTRY, INSTANCE, STACK, RETURN = 0x1466D9, 0x20000, 0x700000, 0x600000
REGISTERS = ((common.x86.UC_X86_REG_EBX, 0x12345678),
             (common.x86.UC_X86_REG_ESI, 0x23456789),
             (common.x86.UC_X86_REG_EDI, 0x3456789A),
             (common.x86.UC_X86_REG_EBP, 0x456789AB))


def original_machine():
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    section = next(s for s in image.secs if s[0] <= ENTRY < s[0] + s[2])
    offset = section[1] + ENTRY - section[0]
    machine = common.unicorn.Uc(common.unicorn.UC_ARCH_X86, common.unicorn.UC_MODE_32)
    machine.mem_map(0, 0x800000)
    machine.mem_write(ENTRY, image.d[offset:offset + 0x29])
    return machine


def expected(machine, state, cursor):
    machine.mem_write(INSTANCE + 0xC4, struct.pack('<II', state, cursor))
    machine.mem_write(STACK, struct.pack('<I', RETURN))
    machine.reg_write(common.x86.UC_X86_REG_ESP, STACK)
    machine.reg_write(common.x86.UC_X86_REG_ECX, INSTANCE)
    machine.reg_write(common.x86.UC_X86_REG_EAX, 0xABCDEF12)
    machine.reg_write(common.x86.UC_X86_REG_EFLAGS, 0x202)
    for reg, value in REGISTERS:
        machine.reg_write(reg, value)
    machine.emu_start(ENTRY, RETURN, count=100)
    assert machine.reg_read(common.x86.UC_X86_REG_EIP) == RETURN
    return [machine.reg_read(reg) for reg in
            (common.x86.UC_X86_REG_EAX, common.x86.UC_X86_REG_ECX,
             common.x86.UC_X86_REG_ESP, *(reg for reg, _ in REGISTERS))]


def source(old):
    if old:
        # Generated chunks are renumbered between translator versions: search them all.
        for chunk in sorted((ROOT / 'src/recomp/gen').glob('recomp_*.c')):
            generated = chunk.read_text()
            body = re.search(r'void sub_001466D9\(void\)\n\{.*?\n\}', generated, re.S)
            if body:
                body = body.group()
                break
        else:
            raise AssertionError('sub_001466D9 not found in generated code')
        body = 'void sub_001466FF(void){ SET_LO8(g_eax,1);g_esp+=4; }\n' + body
    else:
        body = (ROOT / 'src/mm3_pedestrian_compat.c').read_text()
    return r'''
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
static unsigned char memory[0x800000];
''' + body + r'''
int main(int argc,char**argv) {
 unsigned state,cursor;
 if(argc!=2)return 2;
 FILE*in=fopen(argv[1],"rb");if(!in)return 3;
 g_xbox_mem_offset=(ptrdiff_t)memory;
 while(fread(&state,4,1,in)==1) {
  if(fread(&cursor,4,1,in)!=1)return 4;
  MEM32(0x200C4)=state;MEM32(0x200C8)=cursor;
  g_eax=0xABCDEF12;g_ecx=0x20000;g_esp=0x700000;
  g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;g_ebp=0x456789AB;
  g_seh_ebp=g_ebp;
  sub_001466D9();
  if(g_seh_ebp!=0x456789AB||MEM32(0x200C4)!=state||MEM32(0x200C8)!=cursor)return 5;
  printf("%u %u %u %u %u %u %u\n",g_eax,g_ecx,g_esp,g_ebx,g_esi,g_edi,g_ebp);
 }
 fclose(in);return 0;
}
'''


def main():
    if not shutil.which('cl'):
        raise SystemExit('Run from a Visual Studio developer shell (cl on PATH).')
    machine = original_machine()
    cases = [(state, cursor) for state in (*range(12), 0xFFFFFFFF)
             for cursor in (0, INSTANCE + 0x18, INSTANCE + 0x30, 0x12345678)]
    with tempfile.TemporaryDirectory(prefix='pedestrian-completion-', dir=ROOT / 'conformance_tmp') as tmp:
        folder = Path(tmp)
        shutil.copyfile(ROOT / 'src/mm3_recomp_types.h', folder / 'recomp_types.h')
        shutil.copyfile(ROOT / 'tools/xboxrecomp/templates/runtime/recomp_types.h', folder / 'recomp_base.h')
        inputs = folder / 'cases.bin'
        inputs.write_bytes(b''.join(struct.pack('<II', *case) for case in cases))
        path, exe = folder / 'generated.c', folder / 'generated.exe'
        path.write_text(source(True))
        subprocess.run(['cl', '/nologo', '/O2', '/W3', '/wd4102', '/std:c11',
                        '/I' + str(folder), '/I' + str(ROOT / 'tools/xboxrecomp/include'),
                        '/Fo' + str(folder / 'generated.obj'), '/Fe' + str(exe), str(path)],
                       cwd=folder, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        rows = subprocess.check_output([str(exe), str(inputs)], text=True, timeout=10).splitlines()
        assert len(rows) == len(cases)
        for case, row in zip(cases, rows):
            actual = list(map(int, row.split()))
            assert actual == expected(machine, *case), (case, actual, expected(machine, *case))
        print(f'PASS: the generated sub_001466D9 matches the retail Xbox routine on {len(cases)} cases')


if __name__ == '__main__':
    main()

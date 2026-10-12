"""Verify facade VB resizing against actual PAL allocation instructions.

Run after scripts/enter-env.ps1 using tools/venv/Scripts/python.exe. No game,
assets, saves or live processes are modified. Only the object heap and final
guest VB allocator are deterministic seams; their byte requests are captured.
"""
import json
import math
import re
import shutil
import struct
import subprocess
import uuid
from pathlib import Path

import mm3_graph_edge_regression as graph
import mm3_x87_exp_regression as common

ROOT = common.ROOT
ENTRIES = (0x1633B7, 0xF01B8, 0xF00AF, 0xEFE7D, 0x162F75)
SELF, DEVICE, OBJECTS, VBS, BUFFER = 0x760000, 0x730000, 0x780000, 0x790000, 0x800000
META = 0x670000
CAPS = (0x3000, 0x800, 0x200)
OFFSETS = (0x1ED0, 0x1F2C, 0x1F88)


def native_source():
    graph.ENTRIES = ENTRIES
    source = graph.native_source()
    for name, body in {
        'sub_0016FF04': r'''void sub_0016FF04(void) {
 assert(MEM32(g_esp+4)==0x28); g_eax=object_cursor; object_cursor+=0x40;
 memset((void*)(g_xbox_mem_offset+g_eax),0,0x28); g_esp+=8;
}''',
        'sub_00344250': r'''void sub_00344250(void) {
 assert(vb_calls<16); byte_requests[vb_calls]=MEM32(g_esp+4); ++vb_calls;
 g_eax=vb_calls==fail_at?0:VBS+vb_calls*0x40; g_esp+=8;
}''',
        'sub_000F0080': 'void sub_000F0080(void){g_esp+=4;}',
        'sub_00170ED1': 'void sub_00170ED1(void){g_esp+=4;}',
        'sub_00170EE2': 'void sub_00170EE2(void){fprintf(stderr,"unexpected CPU-only VB\\n");exit(99);}',
    }.items():
        pattern = r'void ' + name + r'\(void\)\{fprintf\(stderr,.*?exit\(99\);\}'
        source, changed = re.subn(pattern, lambda _: body, source)
        assert changed == 1, name
    source = source.replace('void sub_001633B7(void)', 'void sub_001633B7_original(void)')
    source = source.replace('ptrdiff_t g_xbox_mem_offset;', 'void sub_001633B7(void);\nptrdiff_t g_xbox_mem_offset;')
    source = source.replace('int main(int argc,char**argv){', r'''
#include "mm3_facade_capacity.c"
float mm3_draw_distance_scale(void){return test_scale;}
int main(int argc,char**argv){''')
    source = source.replace('g_fp_control_word=0x37F;', r'''
 g_fp_control_word=0x37F;
 test_scale=(float)strtod(argv[4],0); fail_at=(unsigned)strtoul(argv[5],0,0);
''')
    source = source.replace('FILE*out=fopen(argv[3],"wb");if(!out)return 4;', r'''
 if(argc>6 && !strcmp(argv[6],"recreate")) {
   g_esp=MEM32(0x75000C);g_esi=MEM32(0x750014);sub_001633B7();
 }
 if(argc>6 && !strcmp(argv[6],"retry")) {
   assert(!(g_eax&0xFF));fail_at=vb_calls=0;object_cursor=0x780000u;
   g_esp=MEM32(0x75000C);g_esi=MEM32(0x750014);sub_001633B7();
 }
 if(argc>6 && !strcmp(argv[6],"allocate")) {
   uint32_t resource=MEM32(0x760000+0x1EC0);
   assert(MEM32(resource+0x14)==MEM32(0x760000+0x1ED0)*28);
   MEM32(resource+0x24)=BUFFER;
   g_esp=0x700000; MEM32(g_esp)=0x600000;
   MEM32(g_esp+4)=1;MEM32(g_esp+8)=1;
   MEM32(0x760000+0x1EC8)=0x3000;MEM32(0x760000+0x1ECC)=0;
   MEM32(0x760000+0x1E74)=1;MEM32(0x760000+0x1F8C)=0;
   g_eax=0x760000;g_edx=1;sub_00162F75();
   MEM32(0x670060)=g_eax;
   if(g_eax) assert(g_eax+112 <= BUFFER+MEM32(resource+0x14));
 }
 if(argc>6 && !strcmp(argv[6],"captured-upper-cells")) {
   uint32_t resource=MEM32(0x760000+0x1EC0),completed=0;
   assert(MEM32(resource+0x14)==MEM32(0x760000+0x1ED0)*28);
   MEM32(resource+0x24)=BUFFER;
   MEM32(0x760000+0x1EC8)=7844;MEM32(0x760000+0x1ECC)=4444;
   MEM32(0x760000+0x1E74)=1;MEM32(0x760000+0x1F8C)=0;
   for(unsigned cell=0;cell<20;++cell) {
     g_esp=0x700000;MEM32(g_esp)=0x600000;
     MEM32(g_esp+4)=1;MEM32(g_esp+8)=1;
     g_eax=0x760000;g_edx=1;sub_00162F75();
     if(g_eax) { ++completed;assert(g_eax+112<=BUFFER+MEM32(resource+0x14)); }
   }
   MEM32(0x670064)=completed;
 }
 MEM32(0x670000)=vb_calls;
 for(unsigned i=0;i<vb_calls;++i)MEM32(0x670004+i*4)=byte_requests[i];
 FILE*out=fopen(argv[3],"wb");if(!out)return 4;''')
    return source.replace('#include <stdio.h>', r'''
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#define VBS 0x790000u
#define BUFFER 0x800000u
static uint32_t object_cursor=0x780000u, byte_requests[16];
static unsigned vb_calls,fail_at;
static float test_scale;''')


def fixture(image, caps=CAPS):
    ram = bytearray(graph.RAM_SIZE)
    for address, raw, size, _ in image.secs:
        ram[address:address + size] = image.d[raw:raw + size]
    def put(address, *values):
        struct.pack_into('<' + 'I' * len(values), ram, address, *values)
    for offset, value in zip(OFFSETS, caps):
        put(SELF + offset, value)
    put(SELF + 0x48, 0x600)
    put(SELF + 0x1F94, DEVICE)
    put(DEVICE + 0x4B8, 0x44444444)
    regs = list(graph.INITIAL)
    regs[5] = SELF
    put(graph.CONFIG, *regs)
    put(graph.STACK, graph.RETURN)
    return ram


def native_result(exe, folder, ram, scale, fail_at=0, mode=None):
    memory, output = folder / 'image.bin', folder / 'result.bin'
    memory.write_bytes(ram)
    args = [str(exe), str(memory), '0x1633B7', str(output), str(scale), str(fail_at)]
    if mode:
        args.append(mode)
    result = subprocess.run(args, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError((args, result.stdout, result.stderr, result.returncode))
    return tuple(map(int, result.stdout.split())), output.read_bytes()


def original_result(ram, fail_at=0):
    machine = common.unicorn.Uc(common.unicorn.UC_ARCH_X86, common.unicorn.UC_MODE_32)
    machine.mem_map(0, len(ram))
    machine.mem_write(0, bytes(ram))
    for name, value in zip(graph.REGISTERS, struct.unpack_from('<8I', ram, graph.CONFIG)):
        machine.reg_write(getattr(common.x86, 'UC_X86_REG_' + name), value)
    requests, object_cursor = [], OBJECTS
    def seam(uc, address, size, _):
        nonlocal object_cursor
        sp = uc.reg_read(common.x86.UC_X86_REG_ESP)
        ret = struct.unpack('<I', uc.mem_read(sp, 4))[0]
        if address == 0x16FF04:
            assert struct.unpack('<I', uc.mem_read(sp + 4, 4))[0] == 0x28
            uc.reg_write(common.x86.UC_X86_REG_EAX, object_cursor)
            uc.mem_write(object_cursor, bytes(0x28))
            object_cursor += 0x40
            delta = 8
        elif address == 0x344250:
            requests.append(struct.unpack('<I', uc.mem_read(sp + 4, 4))[0])
            uc.reg_write(common.x86.UC_X86_REG_EAX,
                         0 if len(requests) == fail_at else VBS + len(requests) * 0x40)
            delta = 8
        elif address in (0xF0080, 0x170ED1):
            delta = 4
        else:
            return
        uc.reg_write(common.x86.UC_X86_REG_ESP, sp + delta)
        uc.reg_write(common.x86.UC_X86_REG_EIP, ret)
    machine.hook_add(common.unicorn.UC_HOOK_CODE, seam)
    machine.emu_start(0x1633B7, graph.RETURN, count=10000)
    assert machine.reg_read(common.x86.UC_X86_REG_EIP) == graph.RETURN
    machine.mem_write(META, struct.pack('<' + 'I' * (len(requests) + 1), len(requests), *requests))
    registers = tuple(machine.reg_read(getattr(common.x86, 'UC_X86_REG_' + name))
                      for name in graph.REGISTERS)
    return registers, bytes(machine.mem_read(0, len(ram)))


def capacities(memory):
    return tuple(struct.unpack_from('<I', memory, SELF + offset)[0] for offset in OFFSETS)


def requests(memory):
    count = struct.unpack_from('<I', memory, META)[0]
    return struct.unpack_from('<' + 'I' * count, memory, META + 4)


def expected_sizes(caps):
    return (caps[0] * 28, caps[1] * 28, caps[2] * 24, 0x600 * 32) * 2


def main():
    if not shutil.which('cl'):
        raise SystemExit('Run scripts/enter-env.ps1 first.')
    evidence_directory = ROOT / 'conformance_tmp/facade-capacity-regression'
    directory = evidence_directory / ('run-' + uuid.uuid4().hex[:8])
    directory.mkdir(parents=True, exist_ok=True)
    source, exe = directory / 'native.c', directory / 'native.exe'
    source.write_text(native_source())
    subprocess.run(['cl', '/nologo', '/W3', '/wd4102', '/Od', '/MD', '/std:c11',
                    '/I' + str(ROOT / 'src'), '/I' + str(ROOT / 'src/recomp/gen'),
                    '/I' + str(ROOT / 'tools/xboxrecomp/include'),
                    '/Fo' + str(directory / 'native.obj'), '/Fe' + str(exe), str(source)], check=True)
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    evidence = []
    for fail_at in (0, 1, 3, 5, 8):
        ram = fixture(image)
        native = native_result(exe, directory, ram, 1, fail_at)
        original = original_result(ram, fail_at)
        assert native[0][:7] == original[0][:7], (native[0], original[0])
        for start, end in ((SELF, SELF + 0x2000), (OBJECTS, OBJECTS + 0x1000),
                           (META, META + 100), (graph.STACK - 0x100, graph.STACK + 8)):
            assert native[1][start:end] == original[1][start:end], (fail_at, hex(start))
        evidence.append({'case': 'stock-raw-XBE', 'fail_at': fail_at, 'sizes': requests(native[1])})
    for scale in (1, 1.25, 1.5, 2, 3, 4):
        factor = min(4, math.ceil(scale * scale))
        wanted = tuple(value * factor for value in CAPS)
        for mode in (None, 'recreate', 'allocate', 'captured-upper-cells'):
            native = native_result(exe, directory, fixture(image), scale, mode=mode)
            assert capacities(native[1]) == wanted
            assert requests(native[1]) == expected_sizes(wanted) * (2 if mode == 'recreate' else 1)
            assert wanted[0] < 65536
            if mode == 'allocate':
                pointer = struct.unpack_from('<I', native[1], META + 0x60)[0]
                assert pointer == (0 if scale == 1 else BUFFER + CAPS[0] * 28), (scale, pointer)
                assert struct.unpack_from('<I', native[1], SELF + 0x1EC8)[0] == CAPS[0] + (4 if factor > 1 else 0)
            if mode == 'captured-upper-cells':
                completed = struct.unpack_from('<I', native[1], META + 0x64)[0]
                assert completed == (0 if factor == 1 else 20)
                head, tail = struct.unpack_from('<II', native[1], SELF + 0x1EC8)
                assert (head, tail) == (7844 + completed * 4, 4444)
            evidence.append({'case': mode or 'expanded-create', 'scale': scale,
                             'factor': factor, 'caps': wanted, 'sizes': requests(native[1])})
        for fail_at in (1, 3, 5, 8):
            native = native_result(exe, directory, fixture(image), scale, fail_at)
            assert capacities(native[1]) == CAPS
            assert not (native[0][0] & 0xFF)
            assert requests(native[1]) == expected_sizes(wanted)[:fail_at]
            evidence.append({'case': 'failed-creation-restores-caps', 'scale': scale, 'fail_at': fail_at})
            retried = native_result(exe, directory, fixture(image), scale, fail_at, 'retry')
            assert capacities(retried[1]) == wanted
            assert requests(retried[1]) == expected_sizes(wanted)
            assert retried[0][0] & 0xFF
            evidence.append({'case': 'retry-after-failed-creation', 'scale': scale, 'fail_at': fail_at})
    for custom in ((0x2000, 0x800, 0x200), (0x3000, 0x400, 0x200), (0x3000, 0x800, 0x100)):
        native = native_result(exe, directory, fixture(image, custom), 2)
        assert capacities(native[1]) == custom
        assert requests(native[1]) == expected_sizes(custom)
        evidence.append({'case': 'custom-preserved', 'caps': custom})
    encoded = json.dumps(evidence, indent=2)
    (directory / 'verification.json').write_text(encoded)
    (evidence_directory / 'verification.json').write_text(encoded)
    print(f'PASS: {len(evidence)} facade capacity cases; stock raw-XBE ABI/memory, actual VB byte requests, bounded growth, recreation, failure restore, custom capacities, and original-full allocator success.')


if __name__ == '__main__':
    main()

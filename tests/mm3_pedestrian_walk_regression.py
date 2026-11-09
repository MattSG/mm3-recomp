"""Exercise the real boarding locomotion callback at its polyline boundary.

Compare the generated callback with original XBE instructions. Controlled
virtual animation callbacks expose point selection, path advancement and the
final zero-velocity request; they do not replace the callback's control flow.
The old pointer-as-count result is retained as a negative control.
"""
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

import mm3_x87_exp_regression as common

ROOT = common.ROOT
ENTRY, RETURN, STACK = 0x136ACA, 0x600000, 0x700000
LOCOMOTOR, ACTOR, WALKER, NODE1, NODE2, OUTPUT = (0x710000, 0x711000, 0x712000,
                                              0x712018, 0x712030, 0x713000)
VT_LOCOMOTOR, VT_ACTOR, VT_WALKER, VT_NODE = 0x720000, 0x720100, 0x720200, 0x720300
GET_COUNT, ADVANCE, GET_NODE, GET_NEXT, GET_POINT, SET_POINT, GET_VELOCITY, ZERO_VELOCITY = range(0x601000, 0x601008)


def native_source():
    # Generated chunks are renumbered between translator versions: search them all.
    for chunk in sorted((ROOT / 'src/recomp/gen').glob('recomp_*.c')):
        body = re.search(r'void sub_00136ACA\(void\)\n\{.*?\n\}', chunk.read_text(), re.S)
        if body:
            body = body.group()
            break
    else:
        raise AssertionError('sub_00136ACA not found in generated code')
    return r'''
#define RECOMP_GENERATED_CODE
#include <stdio.h>
#include <stdlib.h>
#include "mm3_recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
uint32_t g_xbox_code_lo=0x12000,g_xbox_code_hi=0x700000;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp,g_fs_base,g_eflags;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint16_t g_fp_cc,g_fp_control_word;
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_trace_idx;
volatile uint64_t g_icall_count;
static int old_count, advances, points, zeroes, selected;
void get_count(void){g_eax=old_count?0x053D2888:5;g_esp+=4;}
void advance(void){advances++;MEM32(0x7120c8)=MEM32(0x7120c8)==0x712018?0x712030:0;g_esp+=4;}
void get_node(void){g_eax=MEM32(0x7120c8);g_esp+=4;}
void get_next(void){g_eax=0;g_esp+=4;}
void get_point(void){points++;selected=MEM32(g_esp+8);g_eax=MEM32(g_esp+4);
 MEM32(g_eax)=0x42bf933b;MEM32(g_eax+4)=0x41878f4b;MEM32(g_eax+8)=0x43e2aa2d;g_esp+=12;}
void set_point(void){g_esp+=8;}
void get_velocity(void){g_eax=0x713000;g_esp+=4;}
void zero_velocity(void){zeroes++;uint32_t p=MEM32(g_esp+4);
 if(MEM32(p)||MEM32(p+4)||MEM32(p+8))exit(98);g_esp+=12;}
recomp_func_t recomp_lookup(uint32_t va){switch(va){
 case 0x601000:return get_count;case 0x601001:return advance;case 0x601002:return get_node;
 case 0x601003:return get_next;case 0x601004:return get_point;case 0x601005:return set_point;
 case 0x601006:return get_velocity;case 0x601007:return zero_velocity;default:exit(99);return 0;}}
recomp_func_t recomp_lookup_kernel(uint32_t va){return 0;}
recomp_func_t recomp_lookup_manual(uint32_t va){return 0;}
void recomp_icall_fail_log(uint32_t va){exit(99);}
void recomp_icall_not_code_log(uint32_t va){exit(99);}
''' + body + r'''
int main(int argc,char**argv){
 unsigned char*ram=calloc(1,0x800000);g_xbox_mem_offset=(ptrdiff_t)ram;
 FILE*in=fopen(argv[1],"rb");if(!in||fread(ram,1,0x800000,in)!=0x800000)return 2;fclose(in);
 old_count=atoi(argv[2]);selected=-1;
 g_esp=0x700000;g_ecx=0x710000;g_eax=0x12345678;
 g_ebx=0x23456789;g_esi=0x3456789a;g_edi=0x456789ab;g_ebp=g_seh_ebp=0x76543210;
 g_fp_top=3;g_fp_control_word=0x37f;
 sub_00136ACA();
 printf("%u %u %u %u %u %u %u %d %d %d %d\n",g_eax,g_esp,g_ebx,g_esi,g_edi,
 MEM32(0x710048),MEM32(0x7120c8),advances,points,zeroes,selected);
 free(ram);return 0;
}
'''


def fixture(index, second):
    memory = bytearray(0x800000)
    def put(address, *values):
        struct.pack_into('<' + 'I' * len(values), memory, address, *values)
    node = NODE2 if second else NODE1
    put(STACK, RETURN)
    put(LOCOMOTOR + 4, ACTOR, WALKER, VT_LOCOMOTOR)
    put(LOCOMOTOR + 0x48, index, node)
    put(ACTOR, VT_ACTOR)
    put(WALKER, VT_WALKER)
    put(WALKER + 0xC8, node)
    put(NODE1, VT_NODE)
    put(NODE2, VT_NODE)
    put(OUTPUT, VT_ACTOR)
    put(VT_LOCOMOTOR + 8, SET_POINT)
    put(VT_ACTOR + 0xC, ZERO_VELOCITY)
    put(VT_ACTOR + 0x1C, GET_VELOCITY)
    put(VT_WALKER + 0x10, GET_NODE)
    put(VT_WALKER + 0x18, ADVANCE)
    put(VT_WALKER + 0x2C, GET_NEXT)
    put(VT_NODE + 0xC, GET_COUNT, GET_POINT)
    return memory


def original(image, memory, old):
    machine = common.unicorn.Uc(common.unicorn.UC_ARCH_X86, common.unicorn.UC_MODE_32)
    machine.mem_map(0, len(memory))
    machine.mem_write(0, bytes(memory))
    section = next(s for s in image.secs if s[0] <= ENTRY < s[0] + s[2])
    offset = section[1] + ENTRY - section[0]
    machine.mem_write(ENTRY, image.d[offset:offset + 0xBB])
    registers = (('ESP', STACK), ('ECX', LOCOMOTOR), ('EAX', 0x12345678),
                 ('EBX', 0x23456789), ('ESI', 0x3456789A), ('EDI', 0x456789AB), ('EBP', 0x76543210))
    for name, value in registers:
        machine.reg_write(getattr(common.x86, 'UC_X86_REG_' + name), value)
    counts = [0, 0, 0, -1]
    read = lambda address: struct.unpack('<I', machine.mem_read(address, 4))[0]
    def callback(uc, address, size, _):
        if not GET_COUNT <= address <= ZERO_VELOCITY:
            return
        sp = uc.reg_read(common.x86.UC_X86_REG_ESP)
        cleanup = 4
        if address == GET_COUNT:
            uc.reg_write(common.x86.UC_X86_REG_EAX, 0x053D2888 if old else 5)
        elif address == ADVANCE:
            counts[0] += 1
            uc.mem_write(WALKER + 0xC8, struct.pack('<I', NODE2 if read(WALKER + 0xC8) == NODE1 else 0))
        elif address in (GET_NODE, GET_NEXT):
            uc.reg_write(common.x86.UC_X86_REG_EAX, read(WALKER + 0xC8) if address == GET_NODE else 0)
        elif address == GET_POINT:
            counts[1] += 1
            counts[3] = read(sp + 8)
            output = read(sp + 4)
            uc.reg_write(common.x86.UC_X86_REG_EAX, output)
            uc.mem_write(output, struct.pack('<III', 0x42BF933B, 0x41878F4B, 0x43E2AA2D))
            cleanup = 12
        elif address == SET_POINT:
            cleanup = 8
        elif address == GET_VELOCITY:
            uc.reg_write(common.x86.UC_X86_REG_EAX, OUTPUT)
        elif address == ZERO_VELOCITY:
            counts[2] += 1
            assert bytes(uc.mem_read(read(sp + 4), 12)) == bytes(12)
            cleanup = 12
        uc.reg_write(common.x86.UC_X86_REG_EIP, read(sp))
        uc.reg_write(common.x86.UC_X86_REG_ESP, sp + cleanup)
    machine.hook_add(common.unicorn.UC_HOOK_CODE, callback)
    machine.emu_start(ENTRY, RETURN, count=10000)
    assert machine.reg_read(common.x86.UC_X86_REG_EIP) == RETURN, hex(machine.reg_read(common.x86.UC_X86_REG_EIP))
    return [machine.reg_read(getattr(common.x86, 'UC_X86_REG_' + name))
            for name in ('EAX', 'ESP', 'EBX', 'ESI', 'EDI')] + [read(LOCOMOTOR + 0x48), read(WALKER + 0xC8)] + counts


def main():
    if not shutil.which('cl'):
        raise SystemExit('Run scripts/enter-env.ps1 first.')
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    with tempfile.TemporaryDirectory(prefix='pedestrian-walk-', dir=ROOT / 'conformance_tmp') as path:
        directory = Path(path)
        source, executable = directory / 'callback.c', directory / 'callback.exe'
        source.write_text(native_source())
        subprocess.run(['cl', '/nologo', '/O2', '/MD', '/std:c11',
                        '/I' + str(ROOT / 'src'), '/I' + str(ROOT / 'tools/xboxrecomp/include'),
                        '/Fo' + str(directory / 'callback.obj'), '/Fe' + str(executable), str(source)],
                       check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        results = {}
        for old in (False, True):
            for second in (False, True):
                for index in (0, 3, 4, 12):
                    memory = fixture(index, second)
                    inputs = directory / 'memory.bin'
                    inputs.write_bytes(memory)
                    result = list(map(int, subprocess.check_output([str(executable), str(inputs), str(int(old))], text=True).split()))
                    assert result == original(image, memory, old), (old, second, index, result)
                    results[old, second, index] = result
        assert results[False, False, 12][7:] == [1, 1, 0, 1]
        assert results[False, True, 12][7:] == [1, 0, 1, -1]
        assert results[True, False, 12][7:] == [0, 1, 0, 13]
    print('PASS: 16 original-instruction boarding walk callbacks; recovered count advances paths and stops at the end; old pointer count extrapolates point 13')


if __name__ == '__main__':
    main()

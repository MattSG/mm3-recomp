"""Check recovered array-destructor callbacks against original XBE code.

Run with tools/venv/Scripts/python.exe inside `python scripts/devenv.py --shell`.
Heap free and manager deregistration are recorded identically in both runs;
the original array iterator, recovered adapters, and member cleanup execute.
No running game or save files are accessed.
"""
import re
import struct
import subprocess
import tempfile
from pathlib import Path

import cc
import mm3_x87_exp_regression as common

ROOT = common.ROOT
CALLBACKS = (0x4C78C, 0x139C1, 0x138CF1, 0x12B37)
ENTRIES = CALLBACKS + (0x12B8A, 0x24D249, 0x24D275, 0x24D2E9,
                       0x1550AE, 0x4E6ED, 0x144DD1, 0x145145)
PRIMITIVES = {0xF061B: 8, 0xF0749: 8, 0xF210E: 8, 0x170ED1: 4}
ARRAY, STRIDE, TRACE = 0x720000, 0x200, 0x750000


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
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_trace_idx;
volatile uint64_t g_icall_count;
static void record(uint32_t va){
 uint32_t n=MEM32(0x750000),p=0x750004+n*12;
 MEM32(p)=va;MEM32(p+4)=MEM32(g_esp+4);MEM32(p+8)=g_esi;
 MEM32(0x750000)=n+1;
}
'''
    code += '\n'.join(f'void {name}(void);' for name in bodies.keys() | stubs)
    for name in stubs:
        va = int(name[4:],16)
        if va in PRIMITIVES:
            code += f'\nvoid {name}(void){{record(0x{va:X});g_esp+={PRIMITIVES[va]};}}'
        else:
            code += f'\nvoid {name}(void){{fprintf(stderr,"unexpected {name}\\n");exit(99);}}'
    code += '\n' + '\n'.join(bodies.values())
    code += '\nrecomp_func_t recomp_lookup(uint32_t va){switch(va){\n'
    for va in CALLBACKS:
        code += f'case 0x{va:X}:return sub_{va:08X};\n'
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
 g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;
 g_ebp=g_seh_ebp=0x456789AB;g_fp_control_word=0x37F;
 g_esp=0x700000;MEM32(g_esp)=0x600000;
 MEM32(g_esp+4)=0x720000;MEM32(g_esp+8)=0x200;
 MEM32(g_esp+12)=strtoul(argv[3],0,0);MEM32(g_esp+16)=strtoul(argv[2],0,0);
 sub_00012B8A();
 FILE*out=fopen(argv[4],"wb");fwrite(ram,1,0x1000000,out);fclose(out);
 printf("%u %u %u %u %u\n",g_esp,g_ebp,g_ebx,g_esi,g_edi);return 0;
}
'''
    return code


def fixture(image, callback, populated):
    ram = bytearray(0x1000000)
    for address, raw, size, _ in image.secs:
        ram[address:address+size] = image.d[raw:raw+size]
    for index in range(3):
        obj = ARRAY + index*STRIDE
        struct.pack_into('<I',ram,obj,0x11223344)
        if callback == 0x138CF1:
            # Member containers contain recognizable lengths and addresses.
            for offset in (0x4C,0x5C,0x7C,0x8C,0x9C,0xAC,0xBC):
                struct.pack_into('<III',ram,obj+offset+4,
                                 0x780000+index*0x1000+offset if populated else 0,7,9)
            struct.pack_into('<I',ram,obj+0x6C,0x790000+index*0x1000 if populated else 0)
            struct.pack_into('<I',ram,obj+0x78,0)
        else:
            struct.pack_into('<I',ram,obj+(0x10 if callback==0x12B37 else 4),
                             0x760000+index*0x1000 if populated else 0)
    return ram


def original_result(ram, callback, count):
    machine = common.unicorn.Uc(common.unicorn.UC_ARCH_X86,common.unicorn.UC_MODE_32)
    machine.mem_map(0,len(ram));machine.mem_write(0,bytes(ram))
    machine.mem_write(0x700004,struct.pack('<IIII',ARRAY,STRIDE,count,callback))
    code = b'\xe8'+struct.pack('<i',0x12B8A-0x600005)
    machine.mem_write(0x600000,code)
    for register, initial in ((common.x86.UC_X86_REG_ESP,0x700004),
                              (common.x86.UC_X86_REG_EBP,0x456789AB),
                              (common.x86.UC_X86_REG_EBX,0x12345678),
                              (common.x86.UC_X86_REG_ESI,0x23456789),
                              (common.x86.UC_X86_REG_EDI,0x3456789A)):
        machine.reg_write(register,initial)

    def record_primitive(uc,address,size,_):
        if address not in PRIMITIVES:return
        sp=uc.reg_read(common.x86.UC_X86_REG_ESP)
        target,argument=struct.unpack('<II',uc.mem_read(sp,8))
        index=struct.unpack('<I',uc.mem_read(TRACE,4))[0]
        manager=uc.reg_read(common.x86.UC_X86_REG_ESI)
        uc.mem_write(TRACE+4+index*12,struct.pack('<III',address,argument,manager))
        uc.mem_write(TRACE,struct.pack('<I',index+1))
        uc.reg_write(common.x86.UC_X86_REG_ESP,sp+PRIMITIVES[address])
        uc.reg_write(common.x86.UC_X86_REG_EIP,target)

    machine.hook_add(common.unicorn.UC_HOOK_CODE,record_primitive)
    machine.emu_start(0x600000,0x600005,count=100000)
    assert machine.reg_read(common.x86.UC_X86_REG_EIP)==0x600005
    registers=tuple(machine.reg_read(reg) for reg in
                    (common.x86.UC_X86_REG_ESP,common.x86.UC_X86_REG_EBP,
                     common.x86.UC_X86_REG_EBX,common.x86.UC_X86_REG_ESI,common.x86.UC_X86_REG_EDI))
    assert registers==(0x700014,0x456789AB,0x12345678,0x23456789,0x3456789A)
    return bytes(machine.mem_read(ARRAY,STRIDE*3)),bytes(machine.mem_read(TRACE,0x200))


def main():
    cc.require_compiler()
    image=common.Image(str(ROOT/'game_files/default.xbe'))
    scratch=ROOT/'conformance_tmp';scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='destructor-callback-',dir=scratch) as directory:
        directory=Path(directory);source=directory/'callbacks.c';executable=directory/('callbacks'+cc.exe_suffix())
        source.write_text(native_source())
        subprocess.run(cc.compile_command([source],executable,includes=[ROOT/'src',ROOT/'tools/xboxrecomp/include'],
                                          std='c11',optimize='none',dynamic_crt=True,obj=directory/'callbacks.obj'),check=True)
        cases=0
        for callback in CALLBACKS:
            for populated in (False,True):
                for count in (0,1,3):
                    ram=fixture(image,callback,populated);memory=directory/'image.bin';memory.write_bytes(ram)
                    output=directory/'result.bin'
                    result=subprocess.check_output([str(executable),str(memory),hex(callback),str(count),str(output)],text=True)
                    # The lifter keeps architectural EBP local and publishes
                    # its latest frame in g_ebp for frameless callees. The
                    # caller republishes its frame before its next call.
                    assert tuple(map(int,result.split()))==(0x700014,0x6FFFFC,0x12345678,0x23456789,0x3456789A),(hex(callback),populated,count,result)
                    native=output.read_bytes();original_objects,original_trace=original_result(ram,callback,count)
                    assert native[ARRAY:ARRAY+STRIDE*3]==original_objects,(hex(callback),populated,count)
                    assert native[TRACE:TRACE+0x200]==original_trace,(hex(callback),populated,count)
                    for index in range(count):
                        expected=0x37E81C if callback==0x138CF1 else 0x3803B8
                        assert struct.unpack_from('<I',native,ARRAY+index*STRIDE)[0]==expected
                    if populated and callback!=0x138CF1:
                        calls=struct.unpack_from('<I',native,TRACE)[0]
                        assert calls==count
                        objects=[struct.unpack_from('<I',native,TRACE+8+i*12)[0] for i in range(count)]
                        assert objects==[ARRAY+i*STRIDE for i in reversed(range(count))]
                    cases+=1
        print(f'PASS: {cases} native/original-XBE array destructor cases; zero/single/multiple elements, empty/populated cleanup, reverse order, memory effects, ESP and callee-saved registers.')


if __name__=='__main__':main()

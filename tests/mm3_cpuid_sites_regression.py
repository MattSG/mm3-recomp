"""Verify exact CPUID handling and its original library CPU gate.

Run after tools/bin/msvc/setup_x64.bat with tools/venv/Scripts/python.exe.
The XBE instruction bytes, generated descriptor/gate bodies and production
header are used directly. No running game, save data or submodule is changed.
"""
import re
import shutil
import struct
import subprocess
import tempfile

from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import mm3_cpu_diagnostics_regression as diagnostics
import mm3_x87_exp_regression as common

ROOT = common.ROOT
STACK = 0x700000
DESCRIPTOR = 0x800000
LEAVES = ((2, 0x756E6547, 0x6C65746E, 0x49656E69),
          (0x68A, 0, 0, 0x0383F9FD),
          (0x03020101, 0, 0, 0x0C040841))


def original_result(image, entry, direction):
    machine = common.unicorn.Uc(common.unicorn.UC_ARCH_X86, common.unicorn.UC_MODE_32)
    machine.mem_map(0, 0x1000000)
    for address, raw, size, _ in image.secs:
        machine.mem_write(address, bytes(image.d[raw:raw + size]))
    machine.mem_write(STACK, struct.pack('<II', 0x600000, DESCRIPTOR))
    machine.mem_write(DESCRIPTOR, bytes([0xA5]) * 44)
    machine.reg_write(common.x86.UC_X86_REG_EAX, 0x11111111)
    machine.reg_write(common.x86.UC_X86_REG_ECX, 0x321)
    machine.reg_write(common.x86.UC_X86_REG_EDX, 0x432)
    machine.reg_write(common.x86.UC_X86_REG_ESP, STACK)
    machine.reg_write(common.x86.UC_X86_REG_EFLAGS, 0x202 | (direction << 10))
    for register, value in diagnostics.REGISTERS:
        machine.reg_write(register, value)

    def xbox_cpuid(uc, address, size, _):
        if address not in (0xA4FED, 0xA504B):
            return
        leaf = uc.reg_read(common.x86.UC_X86_REG_EAX)
        assert leaf == (1 if address == 0xA4FED else 2)
        for register, value in zip((common.x86.UC_X86_REG_EAX, common.x86.UC_X86_REG_EBX,
                                    common.x86.UC_X86_REG_ECX, common.x86.UC_X86_REG_EDX), LEAVES[leaf]):
            uc.reg_write(register, value)
        uc.reg_write(common.x86.UC_X86_REG_EIP, address + size)

    machine.hook_add(common.unicorn.UC_HOOK_CODE, xbox_cpuid)
    machine.emu_start(entry, 0x600000, count=1000)
    assert machine.reg_read(common.x86.UC_X86_REG_EIP) == 0x600000
    result = [machine.reg_read(register) for register in (
        common.x86.UC_X86_REG_EAX, common.x86.UC_X86_REG_ECX, common.x86.UC_X86_REG_EDX,
        common.x86.UC_X86_REG_ESP, *[r for r, _ in diagnostics.REGISTERS])]
    result.append((machine.reg_read(common.x86.UC_X86_REG_EFLAGS) >> 10) & 1)
    if entry == 0xA4FE0:
        result.extend(struct.unpack('<11I', machine.mem_read(DESCRIPTOR, 44)))
    return result


def native_source():
    source = (ROOT / 'src/recomp/gen/recomp_0008.c').read_text()
    names = ('sub_000A4FE0', 'sub_000A5060')
    bodies = []
    for name in names:
        body = re.search(r'void ' + name + r'\(void\)\n\{.*?\n\}', source, re.S).group()
        body = body.replace('    int _flags = 0; /* fallback flag var */\n', '')
        bodies.append(body)
    aliases = ''.join(f'#define {r} g_{r}\n' for r in ('eax', 'ebx', 'ecx', 'edx', 'esp', 'esi', 'edi'))
    unaliases = ''.join(f'#undef {r}\n' for r in ('eax', 'ebx', 'ecx', 'edx', 'esp', 'esi', 'edi'))
    return (diagnostics.COMMON_NATIVE.replace('#include "recomp_types.h"',
            '#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"') + r'''
RECOMP_TLS uint32_t g_eflags;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp;
RECOMP_TLS uint16_t g_fp_control_word,g_fp_cc;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS RecompMmx g_mm0,g_mm1,g_mm2,g_mm3,g_mm4,g_mm5,g_mm6,g_mm7;
static unsigned fallback_calls;
void recomp_unimpl(const char *text,uint32_t va){
 assert(strcmp(text,"cpuid")==0 && va==0xA4FEE);fallback_calls++;
}
void sub_000A4FE0(void);
void sub_000A5060(void);
''' + aliases + '#pragma warning(push)\n#pragma warning(disable:4102 4127)\n'
            + '\n'.join(bodies) + r'''
static void run_site(uint32_t site){RECOMP_UNIMPL("cpuid",site);}
''' + '#pragma warning(pop)\n' + unaliases + r'''
static RecompXmm *xmm[8];
static RecompMmx *mm[8];
static RecompXmm saved_xmm[8];
static RecompMmx saved_mm[8];
static double saved_fp[8];
static unsigned char saved_memory[sizeof(memory)];
static void init_state(int direction){
 RecompXmm *initial_xmm[8]={&g_xmm0,&g_xmm1,&g_xmm2,&g_xmm3,&g_xmm4,&g_xmm5,&g_xmm6,&g_xmm7};
 RecompMmx *initial_mm[8]={&g_mm0,&g_mm1,&g_mm2,&g_mm3,&g_mm4,&g_mm5,&g_mm6,&g_mm7};
 g_xbox_mem_offset=(ptrdiff_t)memory;memset(memory,0,sizeof(memory));
 g_eax=0x11111111;g_ecx=0x321;g_edx=0x432;g_esp=0x700000;
 g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;g_ebp=0x456789AB;
 g_seh_ebp=0x56789ABC;g_fs_base=0x6789ABCD;g_df=direction;g_eflags=0x202;
 g_fp_top=3;g_fp_cmp=-1;g_fp_control_word=0x27F;g_fp_cc=0x4100;
 for(unsigned i=0;i<8;i++){
  xmm[i]=initial_xmm[i];mm[i]=initial_mm[i];
  memset(xmm[i],(int)(0xA0+i),sizeof(*xmm[i]));saved_xmm[i]=*xmm[i];
  memset(mm[i],(int)(0xB0+i),sizeof(*mm[i]));saved_mm[i]=*mm[i];
  saved_fp[i]=g_fp_stack[i]=(double)i+0.125;
 }
 MEM32(g_esp)=0x600000;MEM32(g_esp+4)=0x800000;
 memset((void*)XBOX_PTR(0x800000),0xA5,44);
}
static void check_other_state(int direction){
 assert(g_esi==0x23456789&&g_edi==0x3456789A&&g_ebp==0x456789AB);
 assert(g_seh_ebp==0x56789ABC&&g_fs_base==0x6789ABCD&&g_df==direction&&g_eflags==0x202);
 assert(g_fp_top==3&&g_fp_cmp==-1&&g_fp_control_word==0x27F&&g_fp_cc==0x4100);
 assert(memcmp(g_fp_stack,saved_fp,sizeof(saved_fp))==0);
 for(unsigned i=0;i<8;i++){
  assert(memcmp(xmm[i],&saved_xmm[i],sizeof(*xmm[i]))==0);
  assert(memcmp(mm[i],&saved_mm[i],sizeof(*mm[i]))==0);
 }
}
int main(int argc,char **argv){
 int direction;assert(argc>=3);direction=atoi(argv[argc-1]);init_state(direction);
 if(strcmp(argv[1],"site")==0){
  uint32_t site,leaf;assert(argc==6);
  site=(uint32_t)strtoul(argv[2],0,0);leaf=(uint32_t)strtoul(argv[3],0,0);
  g_eax=leaf;g_ecx=(uint32_t)strtoul(argv[4],0,0);
  memcpy(saved_memory,memory,sizeof(memory));run_site(site);check_other_state(direction);
  assert(g_esp==0x700000&&memcmp(saved_memory,memory,sizeof(memory))==0);
  assert(fallback_calls==(unsigned)(site==0xA4FEE));
  printf("%u %u %u %u\n",g_eax,g_ebx,g_ecx,g_edx);
 }else{
  assert(argc==3);
  if(strcmp(argv[1],"descriptor")==0)sub_000A4FE0();else sub_000A5060();
  check_other_state(direction);assert(fallback_calls==0);
  assert(g_ebx==0x12345678&&g_ecx==0x321&&g_edx==0x432&&g_esp==0x700004);
  printf("%u %u %u %u %u %u %u %u %u",g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_df);
  if(strcmp(argv[1],"descriptor")==0){for(unsigned i=0;i<11;i++)printf(" %u",MEM32(0x800000+i*4));}
  puts("");
 }
 return 0;
}
''')


def main():
    image = common.Image(str(ROOT / 'game_files/default.xbe'))
    disassembler = Cs(CS_ARCH_X86, CS_MODE_32)
    for site, leaf in ((0xA4FED, 1), (0xA504B, 2)):
        raw = image.raw(site - 5)
        instructions = list(disassembler.disasm(bytes(image.d[raw:raw + 7]), site - 5))
        assert [(i.mnemonic, i.op_str) for i in instructions] == [('mov', f'eax, {leaf}'), ('cpuid', '')]
        assert bytes(instructions[-1].bytes) == b'\x0f\xa2'
    (ROOT / 'conformance_tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cpuid-sites-', dir=ROOT / 'conformance_tmp') as temporary:
        from pathlib import Path
        directory = Path(temporary)
        shutil.copyfile(ROOT / 'src/mm3_recomp_types.h', directory / 'recomp_types.h')
        executable = diagnostics.compile_fixture(directory, 'sites', native_source())
        cases = 0
        for site in (0x9379E, 0xA4FED, 0xA504B, 0xA4FEE):
            for leaf in (0, 1, 2, 0xFFFFFFFF, 0x40000000):
                for subleaf in (0, 0x12345678):
                    for direction in (0, 1):
                        output = subprocess.check_output([str(executable), 'site', str(site), str(leaf), str(subleaf), str(direction)], text=True)
                        expected = (leaf, 0x12345678, subleaf, 0x432) if site == 0xA4FEE else (
                            (0, 0, 0, 0) if leaf == 0x40000000 else LEAVES[min(leaf, 2)])
                        assert list(map(int, output.split())) == list(expected)
                        cases += 1
        for mode, entry in (('descriptor', 0xA4FE0), ('gate', 0xA5060)):
            for direction in (0, 1):
                output = subprocess.check_output([str(executable), mode, str(direction)], text=True)
                expected = original_result(image, entry, direction)
                assert list(map(int, output.split())) == expected
                if mode == 'gate':
                    assert expected[0] == 0
        print(f'PASS: XBE CPUID bytes/leaves confirmed; {cases} exact-site full-output/preservation cases; original descriptor and initialization gate match Unicorn in both DF states.')


if __name__ == '__main__':
    main()

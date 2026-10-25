"""Run the real tree contact seam against retail XBE contact resolution.

Requires `python scripts/devenv.py --shell`, MSVC and Unicorn. The unmodified high-speed
tree negative control must hop and skip the solid contact; the correction
keeps native first-impact resistance/effects, then removes its native piece.
"""
import re
import struct
import subprocess
import tempfile
from pathlib import Path

import cc
import mm3_graph_edge_regression as common

ROOT = common.ROOT
CAR, SIM, BODY, RECORD = 0x760000, 0x770000, 0x780000, 0x790000
PLANES, OFFSETS, LOCAL, EVENTS, NAME = 0x7A0000, 0x7B0000, 0x7C0000, 0x7D0000, 0x7E0000
ENTRIES = (0x1D137B, 0x13A63, 0x194EA, 0x1B8DB, 0x1955F,
           0x1959F, 0x19580, 0x13B7E, 0x13ABD, 0x13A34,
           0x195DA, 0x13A84, 0x2CCA0, 0x1259E, 0x1260D,
           0x16CABD, 0x15FFE0, 0x15CA4B, 0x15B86B, 0x163C28, 0x163C6F)
CITY, CELLS, CELL, INSTANCES, DIRTY, CHANGES = (0x800000, 0x820000, 0x830000,
                                             0x840000, 0x850000, 0x860000)

RESISTANCE_TEST = r'''
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL impact line %d: %s\n",__LINE__,#c);exit(2);}}while(0)
enum { NORMAL, FAILED_RETIREMENT, OTHER_PROP, CHANGED_BODY, UNREADABLE_BODY, MISSING_BODY };
typedef struct {
 const char *name;float before[3],resolved[3],expected[3];unsigned guard;
} ImpactCase;
static const ImpactCase impact_cases[]={
 {"fast impact",{30,2.5f,0},{30,3.5f,0},{28.5f,3.5f,0},NORMAL},
 {"slow break",{5,2.5f,0},{5,3.5f,0},{4.6f,3.5f,0},NORMAL},
 {"solver already slowed",{30,2.5f,0},{1,3.5f,0},{.92f,3.5f,0},NORMAL},
 {"oblique solver response",{18,2.5f,24},{20,3.5f,15},{19.1f,3.5f,13.8f},NORMAL},
 {"reverse response",{30,2.5f,0},{-4,3.5f,2},{-4,3.5f,2},NORMAL},
 {"sideways response",{30,2.5f,0},{0,3.5f,10},{0,3.5f,10},NORMAL},
 {"solver accelerated",{30,2.5f,0},{60,3.5f,0},{58.5f,3.5f,0},NORMAL},
 {"invalid saved velocity",{NAN,2.5f,0},{30,3.5f,0},{30,3.5f,0},NORMAL},
 {"invalid resolved velocity",{30,2.5f,0},{NAN,3.5f,0},{NAN,3.5f,0},NORMAL},
 {"stationary saved velocity",{0,2.5f,0},{30,3.5f,0},{30,3.5f,0},NORMAL},
 {"failed retirement",{30,2.5f,0},{30,3.5f,0},{30,3.5f,0},FAILED_RETIREMENT},
 {"different prop",{30,2.5f,0},{30,3.5f,0},{30,3.5f,0},OTHER_PROP},
 {"body replaced",{30,2.5f,0},{30,3.5f,0},{30,3.5f,0},CHANGED_BODY},
 {"body unreadable",{30,2.5f,0},{30,3.5f,0},{30,3.5f,0},UNREADABLE_BODY},
 {"saved body missing",{30,2.5f,0},{30,3.5f,0},{30,3.5f,0},MISSING_BODY},
};
static unsigned original_calls;
static const ImpactCase *current_case;
void resistance_original_stage(void){
 mm3_tree_use_solid_contact(0x790000,30.0f);
 if(original_calls++==0){
  uint32_t body=0x780000u;
  if(current_case->guard==CHANGED_BODY){body=0x781000u;MEM32(0x770004)=body;}
  if(current_case->guard==UNREADABLE_BODY)MEM32(0x770004)=0xFFFFFFFFu;
  memcpy((void *)XBOX_PTR(body+0x20),current_case->resolved,sizeof current_case->resolved);
 }
}
typedef struct {uint32_t r[10];int f[3];uint16_t c[2];double fp[8];RecompXmm xmm[8];} ImpactState;
static ImpactState impact_state(void){
 ImpactState s;memset(&s,0,sizeof s);
 uint32_t r[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp,g_fs_base};
 memcpy(s.r,r,sizeof r);s.f[0]=g_fp_top;s.f[1]=g_df;s.f[2]=g_fp_cmp;
 s.c[0]=g_fp_cc;s.c[1]=g_fp_control_word;memcpy(s.fp,g_fp_stack,sizeof s.fp);
 RecompXmm xmm[]={g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7};
 memcpy(s.xmm,xmm,sizeof xmm);return s;
}
static void impact_setup(const ImpactCase *test){
 original_calls=0;current_case=test;
 memcpy((void *)XBOX_PTR(0x780020),test->before,sizeof test->before);
 memset((void *)XBOX_PTR(0x78002C),0xB7,0x40);
 const float position[]={12.25f,-3.5f,99.75f};
 memcpy((void *)XBOX_PTR(0x7800C8),position,sizeof position);
 memcpy((void *)XBOX_PTR(0x781000),(const void *)XBOX_PTR(0x780000),0x200);
 if(test->guard==FAILED_RETIREMENT)MEM32(0x820004)=0;
 if(test->guard==OTHER_PROP)MEM32(0x7D0027)=0;
 if(test->guard==MISSING_BODY)MEM32(0x770004)=0;
 g_eax=0x11111111;g_ecx=0x22222222;g_edx=0x33333333;g_esp=0x700000;
 g_ebx=0x44444444;g_esi=0x55555555;g_edi=0x66666666;g_ebp=g_seh_ebp=0x77777777;
 g_fs_base=0x88888888;g_fp_top=3;g_df=1;g_fp_cmp=-1;g_fp_cc=0x4100;g_fp_control_word=0x27F;
 for(unsigned i=0;i<8;i++)g_fp_stack[i]=100.5+i;
 memset(&g_xmm0,0xA0,sizeof g_xmm0);memset(&g_xmm7,0xA7,sizeof g_xmm7);
 MEM32(g_esp)=0x600000;MEM32(g_esp+4)=0x760000;
}
int main(int argc,char **argv){
 CHECK(argc==3);_putenv_s("MM3_TREE_TRACE",argv[2]);
 unsigned char *ram=VirtualAlloc(0,0x1000000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 unsigned char *initial=malloc(0x1000000),*expected=malloc(0x1000000);CHECK(ram&&initial&&expected);
 FILE *input=fopen(argv[1],"rb");CHECK(input&&fread(initial,1,0x1000000,input)==0x1000000);fclose(input);
 g_xbox_mem_offset=(ptrdiff_t)ram;
 for(unsigned case_index=0;case_index<sizeof impact_cases/sizeof impact_cases[0];case_index++){
  const ImpactCase *test=&impact_cases[case_index];
  memcpy(ram,initial,0x1000000);impact_setup(test);sub_001D137B_original();
  ImpactState direct=impact_state();memcpy(expected,ram,0x1000000);
  memcpy(ram,initial,0x1000000);impact_setup(test);sub_001D137B();ImpactState wrapped=impact_state();
  CHECK(!memcmp(&direct,&wrapped,sizeof direct));
  int retired=test->guard!=FAILED_RETIREMENT&&test->guard!=OTHER_PROP;
  if(retired){
   *(uint32_t *)(expected+0x8401A0)=0xFFFFFFF7u;
   *(uint32_t *)(expected+0x850018)=0x860004u;
   *(uint32_t *)(expected+0x860000)=MEM32(0x790104);
  }
  uint32_t body=test->guard==CHANGED_BODY?0x781000u:0x780000u;
  memcpy(expected+body+0x20,test->expected,sizeof test->expected);
  CHECK(!memcmp(expected,ram,0x1000000)); /* Only X/Z resistance plus native retirement. */
  if(case_index==3){
   float lateral_before=.8f*test->resolved[0]-.6f*test->resolved[2];
   float lateral_after=.8f*MEMF(body+0x20)-.6f*MEMF(body+0x28);
   CHECK(fabsf(lateral_after-lateral_before)<0.00001f);
  }
  if(retired){
   /* The same selected contact can survive until another physics substep.
    * A retired tree cannot charge another impact or append another ID. */
   memcpy(expected,ram,0x1000000);g_esp=0x700000;sub_001D137B();
   CHECK(!memcmp(expected,ram,0x1000000)&&s_tree_impact_speed_loss==0);
  }
 }
 printf("PASS: %u one-time tree impact cases tracing=%s: solver response, oblique motion, failed/non-tree/body/velocity guards, repeated contacts and whole-image/guest-state preservation\n",(unsigned)(sizeof impact_cases/sizeof impact_cases[0]),argv[2]);
 free(initial);free(expected);VirtualFree(ram,0,MEM_RELEASE);return 0;
}
'''


def break_fixture(ram, kind=0, index=2, piece=3, cell_index=1):
    put = lambda a, *v: struct.pack_into('<' + 'I' * len(v), ram, a, *v)
    stride, mask_offset, identity = ((0xCC, 8, 0xC8) if kind == 0 else (0xF8, 0xC, 0xF4))
    packed_id = (cell_index << 23) | (kind << 21) | (index << 5) | piece
    put(0x4291F0, CITY)
    put(CITY + 0x15010, 2, 1)
    put(CITY + 0x15304, CELLS, DIRTY)
    put(CELLS + cell_index * 4, CELL)
    put(CELL + (0x18 if kind == 0 else 0x10), INSTANCES, INSTANCES + stride * 4)
    instance = INSTANCES + index * stride
    put(instance + mask_offset, 0xFFFFFFFF)
    put(instance + identity, packed_id & ~31)
    context, container, descriptor, geometry, indices, owner, models = (
        0x870000, 0x871000, 0x871100, 0x872000, 0x872100, 0x873000, 0x874000)
    put(instance, context)
    if kind == 0:
        struct.pack_into('<H', ram, instance + 4, 0)
        put(context, owner, container)
        put(container + 4, descriptor)
    else:
        put(instance + 4, owner)
        struct.pack_into('<H', ram, instance + 8, 0)
        put(context + 4, descriptor)
    put(descriptor, geometry)
    put(descriptor + 0x28, 0, 32)
    put(geometry + 0x14, indices)
    put(owner + 0x11D0, models)
    put(models + 0xA0, LOCAL)
    put(RECORD + 0x104, packed_id)
    put(DIRTY + cell_index * 16, 0, CHANGES, CHANGES, CHANGES + 32)
    return packed_id, instance + mask_offset, DIRTY + cell_index * 16


# Test double for the fork's xbox_GuestRead (tree_read): the host-checked read over this fixture's RAM.
GUEST_READ_DOUBLE = r'''
extern ptrdiff_t g_xbox_mem_offset;
int xbox_GuestRead(uint32_t va, void *out, size_t size)
{
    SIZE_T copied = 0;
    uintptr_t host;
    if (va < 0x10000u || size > UINT32_MAX || va > UINT32_MAX - size) return 0;
    host = (uintptr_t)va + (uintptr_t)g_xbox_mem_offset;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)host, out, size, &copied) && copied == size;
}
'''


def source():
    previous = common.ENTRIES
    try:
        common.ENTRIES = ENTRIES
        return common.native_source()
    finally:
        common.ENTRIES = previous


def apply_seam(directory, code):
    original, transformed = directory / 'before.c', directory / 'after.c'
    original.write_text(code)
    script = directory / 'apply.cmake'
    script.write_text(f'''include("{(ROOT / 'scripts/mm3_tree_contact_seam.cmake').as_posix()}")
file(READ "{original.as_posix()}" content)
mm3_add_tree_contacts(content)
file(WRITE "{transformed.as_posix()}" "${{content}}")
''')
    subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True, text=True)
    return transformed.read_text()


def interned(ram, name):
    # Real retail interned nibble-trie layout, decoded by sub_0017069B.
    characters = '\0' + name[::-1]
    for i, character in enumerate(characters):
        address, value = NAME + i * 16, ord(character)
        parent = address + 16 if i + 1 < len(characters) else 0
        struct.pack_into('<3I', ram, address, parent | value >> 4,
                         value & 15, address - 16 if i else 0)


def fixture(image, flags, speed, kind='tree-event', vertical=0.0):
    ram = bytearray(common.RAM_SIZE)
    for address, raw, size, _ in image.secs:
        ram[address:address + size] = image.d[raw:raw + size]
    put = lambda a, *v: struct.pack_into('<' + 'I' * len(v), ram, a, *v)
    pf = lambda a, *v: struct.pack_into('<' + 'f' * len(v), ram, a, *v)
    put(common.CONFIG, 0x11223344, 0x22334455, 0x33445566, common.STACK,
        0x12345678, 0x23456789, 0x3456789A, 0x456789AB)
    put(common.STACK, common.RETURN, CAR, 0)
    put(CAR + 0x80, 1)
    ram[CAR + 0xEC] = 1
    put(CAR + 0x3C, SIM)
    put(SIM + 4, BODY)
    put(CAR + 0x43F4, 1, RECORD)
    put(CAR + 0x88, 100)
    put(CAR + 0xE0, 100)  # Native sound/effect cooldown avoids external audio.
    pf(BODY + 0x20, speed, vertical, 0)
    pf(BODY + 0xA4, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)
    put(BODY + 0x0C, 1)
    put(BODY + 0x188, PLANES, OFFSETS)
    pf(PLANES, 1, 0, 0)
    pf(OFFSETS, 1)
    pf(RECORD, 0.8, 0, 0)
    put(RECORD + 0xC0, 1)
    struct.pack_into('<H', ram, RECORD + 0x100, flags)
    if kind != 'no-metadata':
        put(RECORD + 0x108, LOCAL)
        ram[LOCAL] = 1
        put(LOCAL + 0x2C, EVENTS)
        put(EVENTS + 0x27, NAME)
        interned(ram, 'tree' if kind == 'tree-event' else 'trashcan')
    return ram


def check_impact_resistance(corrected, directory, image):
    # Keep real native retirement and the production wrapper. A controlled
    # original-call stage supplies solver outcomes and body replacement so
    # guards are tested independently of the native solver's normal path.
    guarded, count = re.subn(r'void sub_001D137B_original\(void\)\n\{.*?\n\}',
                            'void sub_001D137B_original(void){resistance_original_stage();g_esp+=12;}',
                            corrected, count=1, flags=re.S)
    assert count == 1
    guarded, count = re.subn(r'int main\(int argc,char\*\*argv\)\{.*?\n\}', '',
                            guarded, count=1, flags=re.S)
    assert count == 1
    guarded = 'void resistance_original_stage(void);\n' + guarded + RESISTANCE_TEST
    code, executable = directory / 'resistance.c', directory / ('resistance' + cc.exe_suffix())
    code.write_text(guarded)
    subprocess.run(cc.compile_command([code], executable,
                                      includes=[ROOT / 'src', ROOT / 'tools/xboxrecomp/include', ROOT / 'tools/xboxrecomp/src', ROOT / 'tools/xboxrecomp/src/kernel'],
                                      std='c11', optimize='none', dynamic_crt=True,
                                      obj=directory / 'resistance.obj'), check=True)
    ram = fixture(image, 2, 30)
    break_fixture(ram)
    memory = directory / 'resistance.bin'
    memory.write_bytes(ram)
    for tracing in ('0', '1', 'contacts'):
        subprocess.run([str(executable), str(memory), tracing], check=True)
    return 45


def main():
    cc.skip_unless_windows('the corrected fixture includes windows.h')
    cc.require_compiler()
    image = common.common.Image(str(ROOT / 'game_files/default.xbe'))
    with tempfile.TemporaryDirectory(prefix='mm3-tree-contact-') as temporary:
        directory = Path(temporary)
        retail = source()
        corrected = retail.replace('void sub_001D137B(void)\n{',
                                   'void sub_001D137B_original(void)\n{')
        corrected = apply_seam(directory, corrected)
        assert corrected.count('if (mm3_tree_use_solid_contact(ecx, MEMF(ebp + 0x6C)))') == 1
        # Sound eligibility must still read the authored flag in the final gate.
        final_sound_gate = retail[retail.index('loc_001D1A10: ;'):retail.index('loc_001D1A19: ;')]
        assert final_sound_gate in corrected
        corrected = ('#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n#include "guest_state.c"\n#include "vec3_rotate.c"\n#include "guest_read.c"\n'
                     'RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3, g_mm4, g_mm5, g_mm6, g_mm7;\n'
                     'RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7;\n'
                     + GUEST_READ_DOUBLE + corrected)
        corrected = corrected.replace('int main(', 'static unsigned tree_expire_contacts(uint32_t, uint32_t, uint32_t);\nint main(', 1)
        corrected += '\nvoid sub_0016AEDB_original(void){}\nvoid sub_000E1F49_original(void){}\n'
        # Model only allocation-free vector append; the REAL generated city
        # removal/ID decoding/piece mask functions execute below and are also
        # compared directly against retail XBE instructions with bool=0.
        corrected = re.sub(r'void sub_0002B969\(void\)\{.*?\}', r'''void sub_0002B969(void){
          uint32_t input=MEM32(g_esp+4), end=MEM32(g_eax+8);
          if(end+4>MEM32(g_eax+12))exit(99);
          MEM32(end)=MEM32(input);MEM32(g_eax+8)=end+4;g_esp+=8;
          /* Volatile/FPU/SIMD sentinels expose a missing state restore. */
          g_eax=0xABCDEF00;g_ecx=0xABCD;g_edx=0xDEF;
          g_ebp=g_seh_ebp=0xDEADBEEF;g_fs_base=0;
          g_fp_top=5;g_fp_cmp=1;g_fp_cc=0x4100;g_fp_control_word=0x37F;g_df=0;
          for(unsigned i=0;i<8;i++)g_fp_stack[i]=-25.5-i;
          memset(&g_xmm0,0,sizeof g_xmm0);memset(&g_xmm7,0,sizeof g_xmm7);
        }''', corrected, count=1)
        corrected += f'#include "{(ROOT / "src/mm3_tree_diagnostics.c").as_posix()}"\n'
        classify = r'''if (strtoul(argv[2],0,0) == 0x600105) {
          g_eax=tree_expire_contacts(g_eax,MEM32(0x790104),0x7C0000);g_esp+=4;
        } else if (strtoul(argv[2],0,0) >= 0x600100 && strtoul(argv[2],0,0) <= 0x600104) {
          uint32_t before[10], after[10]; double fp_before[8]; int i, result;
          RecompXmm xmm_before[8], xmm_after[8];
          g_fp_top=3; g_fp_control_word=0x27F; g_fp_cc=0x4000; g_fp_cmp=-1;
          g_fs_base=0xABCD0000; g_df=1;
          for(i=0;i<8;++i)g_fp_stack[i]=123.5+i;
          memset(&g_xmm0,0xA0,sizeof g_xmm0);memset(&g_xmm1,0xA1,sizeof g_xmm1);
          memset(&g_xmm2,0xA2,sizeof g_xmm2);memset(&g_xmm3,0xA3,sizeof g_xmm3);
          memset(&g_xmm4,0xA4,sizeof g_xmm4);memset(&g_xmm5,0xA5,sizeof g_xmm5);
          memset(&g_xmm6,0xA6,sizeof g_xmm6);memset(&g_xmm7,0xA7,sizeof g_xmm7);
          xmm_before[0]=g_xmm0;xmm_before[1]=g_xmm1;xmm_before[2]=g_xmm2;xmm_before[3]=g_xmm3;
          xmm_before[4]=g_xmm4;xmm_before[5]=g_xmm5;xmm_before[6]=g_xmm6;xmm_before[7]=g_xmm7;
          before[0]=g_eax;before[1]=g_ecx;before[2]=g_edx;before[3]=g_esp;
          before[4]=g_ebx;before[5]=g_esi;before[6]=g_edi;before[7]=g_ebp;
          before[8]=g_seh_ebp;before[9]=g_fs_base;
          memcpy(fp_before,g_fp_stack,sizeof fp_before);
          result=mm3_tree_use_solid_contact(g_eax, strtoul(argv[2],0,0)>=0x600102?30.0f:0.0f);
          if(strtoul(argv[2],0,0)==0x600101){
            uint32_t name=MEM32(0x7D0027);if(result!=1)return 99;
            MEM32(0x7D0027)=0;result=mm3_tree_use_solid_contact(g_eax, 0.0f);MEM32(0x7D0027)=name;
          }
          if(strtoul(argv[2],0,0)>=0x600102){
            if(result!=1)return 99;
            uint32_t city=MEM32(0x4291F0), name=MEM32(0x7D0027);
            if(strtoul(argv[2],0,0)==0x600103)MEM32(0x4291F0)=0;
            if(strtoul(argv[2],0,0)==0x600104)MEM32(0x7D0027)=0;
            result=tree_remove_native();
            MEM32(0x4291F0)=city;MEM32(0x7D0027)=name;
          }
          after[0]=g_eax;after[1]=g_ecx;after[2]=g_edx;after[3]=g_esp;
          after[4]=g_ebx;after[5]=g_esi;after[6]=g_edi;after[7]=g_ebp;
          after[8]=g_seh_ebp;after[9]=g_fs_base;
          xmm_after[0]=g_xmm0;xmm_after[1]=g_xmm1;xmm_after[2]=g_xmm2;xmm_after[3]=g_xmm3;
          xmm_after[4]=g_xmm4;xmm_after[5]=g_xmm5;xmm_after[6]=g_xmm6;xmm_after[7]=g_xmm7;
          if(memcmp(before,after,sizeof before)||memcmp(fp_before,g_fp_stack,sizeof fp_before)
             ||memcmp(xmm_before,xmm_after,sizeof xmm_before)
             ||g_fp_top!=3||g_fp_control_word!=0x27F||g_fp_cc!=0x4000||g_fp_cmp!=-1||g_df!=1)return 98;
          g_eax=result;g_esp+=4;
        } else {
          recomp_lookup((uint32_t)strtoul(argv[2],0,0))();
          if(g_fp_top!=0||g_fp_control_word!=0x37F||g_df!=0)return 97;
        }'''
        corrected = corrected.replace('recomp_lookup((uint32_t)strtoul(argv[2],0,0))();', classify)
        executable = directory / ('corrected' + cc.exe_suffix())
        code = directory / 'corrected.c'
        code.write_text(corrected)
        subprocess.run(cc.compile_command([code], executable,
                                          includes=[ROOT / 'src', ROOT / 'tools/xboxrecomp/include', ROOT / 'tools/xboxrecomp/src', ROOT / 'tools/xboxrecomp/src/kernel'],
                                          std='c11', optimize='none', dynamic_crt=True,
                                          obj=directory / 'corrected.obj'), check=True)
        cases, controls = 0, 0
        for flags in (0, 2, 4, 6):
            for speed in (0, 5, 15, 30, 60):
                for kind in ('tree-event', 'other', 'no-metadata'):
                    ram = fixture(image, flags, speed, kind)
                    expected_input = ram.copy()
                    if kind == 'tree-event':
                        struct.pack_into('<H', expected_input, RECORD + 0x100, flags & ~2)
                    expected_regs, expected_ram = common.original_result(expected_input, 0x1D137B)
                    actual_regs, actual_ram = common.native_result(executable, directory, ram, 0x1D137B)
                    assert actual_regs[:7] == expected_regs[:7], (flags, speed, kind, actual_regs, expected_regs)
                    for start, end in ((CAR, RECORD + 0x100), (RECORD + 0x102, NAME + 256),
                                       (0x3C0344, 0x3C0440), (0x4078E0, 0x407BB0)):
                        assert actual_ram[start:end] == expected_ram[start:end], (flags, speed, kind, hex(start))
                    assert actual_ram[RECORD + 0x100:RECORD + 0x102] == ram[RECORD + 0x100:RECORD + 0x102]
                    if kind == 'tree-event':
                        assert struct.unpack_from('<f', actual_ram, BODY + 0x24)[0] == 0
                        assert struct.unpack_from('<I', actual_ram, 0x3C0344)[0] == 1
                        assert struct.unpack_from('<f', actual_ram, 0x3C03C0)[0] > 0
                        if flags & 2 and speed > 11.12:
                            _, negative = common.original_result(ram, 0x1D137B)
                            assert struct.unpack_from('<f', negative, BODY + 0x24)[0] > 0
                            assert struct.unpack_from('<I', negative, 0x3C0344)[0] == 0
                            controls += 1
                    cases += 1
        # A pre-existing vertical motion must survive normal resolution too.
        ram = fixture(image, 2, 30, vertical=2.5)
        _, result = common.native_result(executable, directory, ram, 0x1D137B)
        assert struct.unpack_from('<f', result, BODY + 0x24)[0] == 2.5
        break_cases = 0
        for city_kind in (0, 1):
            for speed in (0, 4.99, 5, 15, 60):
                ram = fixture(image, 2, speed)
                packed_id, mask, vector = break_fixture(ram, city_kind)
                registers, result = common.native_result(executable, directory, ram, 0x1D137B)
                expected_input = ram.copy()
                struct.pack_into('<H', expected_input, RECORD + 0x100, 0)
                expected_registers, expected = common.original_result(expected_input, 0x1D137B)
                assert registers[:7] == expected_registers[:7]
                expected_body = bytearray(expected[BODY:BODY + 0x200])
                if speed >= 5:
                    loss = {5: .4, 15: 1.2, 60: 1.5}[speed]
                    resolved = struct.unpack_from('<f', expected_body, 0x20)[0]
                    struct.pack_into('<f', expected_body, 0x20, resolved - loss)
                assert result[BODY:BODY + 0x200] == expected_body
                assert struct.unpack_from('<H', result, RECORD + 0x100)[0] == 2
                broken = speed >= 5
                assert struct.unpack_from('<I', result, mask)[0] == (0xFFFFFFF7 if broken else 0xFFFFFFFF)
                assert struct.unpack_from('<I', result, vector + 8)[0] == CHANGES + (4 if broken else 0)
                assert struct.unpack_from('<I', result, CHANGES)[0] == (packed_id if broken else 0)
                # Availability masks drive both native collision and native
                # render loops. Nearby instances and all other bits survive.
                initial_instances = ram[INSTANCES:INSTANCES + 0x1000]
                expected_instances = bytearray(initial_instances)
                if broken:
                    struct.pack_into('<I', expected_instances, mask - INSTANCES, 0xFFFFFFF7)
                assert result[INSTANCES:INSTANCES + 0x1000] == expected_instances
                break_cases += 1
        # Directly compare the actual native remove API with retail, across
        # both instance classes, different indices and high/low piece bits.
        for city_kind in (0, 1):
            for index in (0, 2):
                for piece in (0, 3, 31):
                    ram = fixture(image, 2, 30)
                    packed_id, mask, _ = break_fixture(ram, city_kind, index, piece)
                    struct.pack_into('<I', ram, common.CONFIG + 24, CITY)
                    struct.pack_into('<3I', ram, common.STACK, common.RETURN, packed_id, 0)
                    original_regs, original_ram = common.original_result(ram, 0x16CABD)
                    actual_regs, actual_ram = common.native_result(executable, directory, ram, 0x16CABD)
                    assert actual_regs[:7] == original_regs[:7]
                    assert actual_ram[INSTANCES:INSTANCES + 0x1000] == original_ram[INSTANCES:INSTANCES + 0x1000]
                    assert struct.unpack_from('<I', actual_ram, mask)[0] == 0xFFFFFFFF ^ (1 << piece)
                    break_cases += 1
        # Malformed IDs, missing cells, pointer/identity reuse and non-tree
        # contacts must not retire an unrelated piece or append change IDs.
        for invalid in ('other', 'unloaded-cell', 'wrong-id', 'array-overrun', 'bad-grid',
                        'changed-piece-metadata', 'bad-descriptor', 'missing-piece'):
            ram = fixture(image, 2, 30, 'other' if invalid == 'other' else 'tree-event')
            _, _, _ = break_fixture(ram)
            if invalid == 'unloaded-cell': struct.pack_into('<I', ram, CELLS + 4, 0)
            elif invalid == 'wrong-id': struct.pack_into('<I', ram, INSTANCES + 2 * 0xCC + 0xC8, 0)
            elif invalid == 'array-overrun': struct.pack_into('<I', ram, CELL + 0x1C, INSTANCES)
            elif invalid == 'bad-grid': struct.pack_into('<I', ram, CITY + 0x15010, 0)
            elif invalid == 'changed-piece-metadata': struct.pack_into('<I', ram, 0x8740A0, 0)
            elif invalid == 'bad-descriptor': struct.pack_into('<I', ram, 0x871004, 0xFFFFFFFF)
            elif invalid == 'missing-piece': struct.pack_into('<I', ram, 0x87112C, 3)
            _, result = common.native_result(executable, directory, ram, 0x1D137B)
            assert result[INSTANCES:CHANGES + 32] == ram[INSTANCES:CHANGES + 32], invalid
            expected_input = ram.copy()
            if invalid != 'other': struct.pack_into('<H', expected_input, RECORD + 0x100, 0)
            _, expected = common.original_result(expected_input, 0x1D137B)
            assert result[BODY:BODY + 0x200] == expected[BODY:BODY + 0x200], invalid
            break_cases += 1
        for entry, removed in ((0x600102, True), (0x600103, False), (0x600104, False)):
            ram = fixture(image, 2, 30)
            _, mask, _ = break_fixture(ram)
            struct.pack_into('<I', ram, common.CONFIG, RECORD)
            registers, result = common.native_result(executable, directory, ram, entry)
            assert registers[0] == removed
            assert struct.unpack_from('<I', result, mask)[0] == (0xFFFFFFF7 if removed else 0xFFFFFFFF)
            # A changed city or reused metadata cannot consume a pending ID.
            if not removed: assert result == ram
            break_cases += 1
        # Live play exposed reused narrowphase points between physics substeps.
        # Retire all matching copies in both native inline vectors, then run
        # the REAL contact selector again: the adjacent prop must still collide.
        ram = fixture(image, 2, 30)
        packed_id, mask, _ = break_fixture(ram)
        record = bytes(ram[RECORD:RECORD + 0x10C])
        cached = []
        for vector_offset, buffer_offset in ((0x43F0, 0xF0), (0x8700, 0x4400)):
            start = CAR + buffer_offset
            struct.pack_into('<3I', ram, CAR + vector_offset, 64, 4, start)
            for index in range(4):
                address = start + index * 0x10C
                ram[address:address + 0x10C] = record
                struct.pack_into('<f', ram, address, 0.8 + index * 0.04)
                if index == 2:  # Adjacent solid prop, with a different native ID.
                    struct.pack_into('<H', ram, address + 0x100, 0)
                    struct.pack_into('<2I', ram, address + 0x104, packed_id + 1, 0)
                elif index == 3:  # Reused ID with a different metadata owner.
                    struct.pack_into('<f', ram, address, 5)
                    struct.pack_into('<I', ram, address + 0x108, LOCAL + 4)
                cached.append((address, index < 2))
        _, after_break = common.native_result(executable, directory, ram, 0x1D137B)
        assert struct.unpack_from('<I', after_break, mask)[0] == 0xFFFFFFF7
        for address, expired in cached:
            expected_record = bytearray(ram[address:address + 0x10C])
            if expired: struct.pack_into('<I', expected_record, 0xC0, 0)
            assert after_break[address:address + 0x10C] == expected_record
        for vector_offset in (0x43F0, 0x8700):
            assert after_break[CAR + vector_offset:CAR + vector_offset + 12] == ram[CAR + vector_offset:CAR + vector_offset + 12]
        before_count = struct.unpack_from('<I', after_break, 0x3C0344)[0]
        expected_registers, expected = common.original_result(after_break, 0x1D137B)
        registers, next_substep = common.native_result(executable, directory, after_break, 0x1D137B)
        assert registers[:7] == expected_registers[:7]
        assert struct.unpack_from('<I', next_substep, 0x3C0344)[0] == before_count + 1
        for start, end in ((CAR, CAR + 0x8710), (BODY, BODY + 0x200),
                           (0x3C0344, 0x3C0440), (0x4078E0, 0x407BB0)):
            assert next_substep[start:end] == expected[start:end]
        expiry_cases = 1
        for malformed in ('capacity', 'count', 'borrowed-buffer', 'overflow-car'):
            invalid = ram.copy()
            struct.pack_into('<I', invalid, common.CONFIG, CAR)
            for vector_offset in (0x43F0, 0x8700):
                if malformed == 'capacity': struct.pack_into('<I', invalid, CAR + vector_offset, 65)
                elif malformed == 'count': struct.pack_into('<I', invalid, CAR + vector_offset + 4, 65)
                elif malformed == 'borrowed-buffer': struct.pack_into('<I', invalid, CAR + vector_offset + 8, RECORD)
            if malformed == 'overflow-car': struct.pack_into('<I', invalid, common.CONFIG, 0xFFFFF000)
            registers, result = common.native_result(executable, directory, invalid, 0x600105)
            assert registers[0] == 0
            assert result == invalid
            expiry_cases += 1
        classifier_cases = 0
        for kind, eligible in (('tree-main', 1), ('tree-event', 1), ('other', 0),
                               ('bad-metadata', 0), ('bad-name', 0), ('overflow-record', 0),
                               ('unflagged', 1), ('reused-city-address', 0)):
            ram = fixture(image, 2, 30)
            struct.pack_into('<I', ram, common.CONFIG, RECORD)
            if kind == 'tree-main':
                struct.pack_into('<I', ram, LOCAL + 0x10, NAME)
                ram[LOCAL] = 0
            elif kind == 'other':
                interned(ram, 'trashcan')
            elif kind == 'bad-metadata':
                struct.pack_into('<I', ram, RECORD + 0x108, 0xFFFFFFFF)
            elif kind == 'bad-name':
                struct.pack_into('<I', ram, EVENTS + 0x27, 0xFFFFFFFF)
            elif kind == 'overflow-record':
                struct.pack_into('<I', ram, common.CONFIG, 0xFFFFFFFE)
            elif kind == 'unflagged':
                struct.pack_into('<H', ram, RECORD + 0x100, 0)
            registers, result = common.native_result(executable, directory, ram,
                                                     0x600101 if kind == 'reused-city-address' else 0x600100)
            assert registers[0] == eligible, (kind, registers)
            assert result == ram, kind  # The entire 16MB guest image is unchanged.
            classifier_cases += 1
        # Fail closed if regeneration changes, duplicates, or reapplies the seam.
        for malformed in (retail.replace('MEM32(ebp + 0x64) = ecx;', 'MEM32(ebp + 0x64) = 0;'),
                          retail + retail, corrected):
            try:
                apply_seam(directory, malformed)
            except subprocess.CalledProcessError:
                pass
            else:
                raise AssertionError('Changed/duplicate/already-applied seam accepted')
        impact_cases = check_impact_resistance(corrected, directory, image)
        print(f'PASS: {cases + 1} tree/non-tree native/XBE contact cases, {controls} retail-hop negative controls; {break_cases} native instance retirement/streaming-list cases, 5m/s threshold, both static instance classes and retail removal parity; {expiry_cases} narrowphase expiry/substep guards with adjacent prop collision; {impact_cases} bounded one-time resistance cases; authored flags, other props, ABI/x87 and sound gate preserved; {classifier_cases} guarded tree classification cases; seam fails closed.')


if __name__ == '__main__':
    main()

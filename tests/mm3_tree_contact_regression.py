"""Run the real tree contact seam against retail XBE contact resolution.

Requires scripts/enter-env.ps1, MSVC and Unicorn. The unmodified high-speed
tree negative control must hop and skip the solid contact; the correction
must match retail normal contact/penetration resolution while keeping flags.
"""
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

import mm3_graph_edge_regression as common

ROOT = common.ROOT
CAR, SIM, BODY, RECORD = 0x760000, 0x770000, 0x780000, 0x790000
PLANES, OFFSETS, LOCAL, EVENTS, NAME = 0x7A0000, 0x7B0000, 0x7C0000, 0x7D0000, 0x7E0000
ENTRIES = (0x1D137B, 0x13A63, 0x194EA, 0x1B8DB, 0x1955F,
           0x1959F, 0x19580, 0x13B7E, 0x13ABD, 0x13A34,
           0x195DA, 0x13A84, 0x2CCA0, 0x1259E, 0x1260D)


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


def main():
    if not shutil.which('cl'):
        raise SystemExit('Run scripts/enter-env.ps1 first.')
    image = common.common.Image(str(ROOT / 'game_files/default.xbe'))
    with tempfile.TemporaryDirectory(prefix='mm3-tree-contact-') as temporary:
        directory = Path(temporary)
        retail = source()
        corrected = retail.replace('void sub_001D137B(void)\n{',
                                   'void sub_001D137B_original(void)\n{')
        corrected = apply_seam(directory, corrected)
        assert corrected.count('if (mm3_tree_use_solid_contact(ecx))') == 1
        # Sound eligibility must still read the authored flag in the final gate.
        final_sound_gate = retail[retail.index('loc_001D1A10: ;'):retail.index('loc_001D1A19: ;')]
        assert final_sound_gate in corrected
        corrected = '#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n' + corrected
        corrected += '\nvoid sub_0016AEDB_original(void){}\nvoid sub_000E1F49_original(void){}\n'
        corrected += f'#include "{(ROOT / "src/mm3_tree_diagnostics.c").as_posix()}"\n'
        classify = r'''if (strtoul(argv[2],0,0) == 0x600100 || strtoul(argv[2],0,0) == 0x600101) {
          uint32_t before[11], after[11]; double fp_before[8]; int i, result;
          RecompXmm xmm_before[8], xmm_after[8];
          g_fp_top=3; g_fp_control_word=0x27F; g_fp_cc=0x4000; g_fp_cmp=-1;
          g_eflags=0xA545; g_fs_base=0xABCD0000; g_df=1;
          for(i=0;i<8;++i)g_fp_stack[i]=123.5+i;
          memset(&g_xmm0,0xA0,sizeof g_xmm0);memset(&g_xmm1,0xA1,sizeof g_xmm1);
          memset(&g_xmm2,0xA2,sizeof g_xmm2);memset(&g_xmm3,0xA3,sizeof g_xmm3);
          memset(&g_xmm4,0xA4,sizeof g_xmm4);memset(&g_xmm5,0xA5,sizeof g_xmm5);
          memset(&g_xmm6,0xA6,sizeof g_xmm6);memset(&g_xmm7,0xA7,sizeof g_xmm7);
          xmm_before[0]=g_xmm0;xmm_before[1]=g_xmm1;xmm_before[2]=g_xmm2;xmm_before[3]=g_xmm3;
          xmm_before[4]=g_xmm4;xmm_before[5]=g_xmm5;xmm_before[6]=g_xmm6;xmm_before[7]=g_xmm7;
          before[0]=g_eax;before[1]=g_ecx;before[2]=g_edx;before[3]=g_esp;
          before[4]=g_ebx;before[5]=g_esi;before[6]=g_edi;before[7]=g_ebp;
          before[8]=g_seh_ebp;before[9]=g_eflags;before[10]=g_fs_base;
          memcpy(fp_before,g_fp_stack,sizeof fp_before);
          result=mm3_tree_use_solid_contact(g_eax);
          if(strtoul(argv[2],0,0)==0x600101){
            uint32_t name=MEM32(0x7D0027);if(result!=1)return 99;
            MEM32(0x7D0027)=0;result=mm3_tree_use_solid_contact(g_eax);MEM32(0x7D0027)=name;
          }
          after[0]=g_eax;after[1]=g_ecx;after[2]=g_edx;after[3]=g_esp;
          after[4]=g_ebx;after[5]=g_esi;after[6]=g_edi;after[7]=g_ebp;
          after[8]=g_seh_ebp;after[9]=g_eflags;after[10]=g_fs_base;
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
        executable = directory / 'corrected.exe'
        code = directory / 'corrected.c'
        code.write_text(corrected)
        subprocess.run(['cl', '/nologo', '/Od', '/MD', '/std:c11',
                        '/I' + str(ROOT / 'src'), '/I' + str(ROOT / 'tools/xboxrecomp/include'),
                        '/Fo' + str(directory / 'corrected.obj'), '/Fe' + str(executable), str(code)], check=True)
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
        classifier_cases = 0
        for kind, eligible in (('tree-main', 1), ('tree-event', 1), ('other', 0),
                               ('bad-metadata', 0), ('bad-name', 0), ('overflow-record', 0),
                               ('unflagged', 0), ('reused-city-address', 0)):
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
        for malformed in (retail.replace('loc_001D1547: ;', 'loc_001D1547_changed: ;'),
                          retail + retail, corrected):
            try:
                apply_seam(directory, malformed)
            except subprocess.CalledProcessError:
                pass
            else:
                raise AssertionError('Changed/duplicate/already-applied seam accepted')
        print(f'PASS: {cases + 1} tree/non-tree native/XBE contact cases, {controls} retail-hop negative controls; solid penetration correction, vertical velocity, original flags, other props, register/stack ABI, balanced x87 and final sound gate preserved; {classifier_cases} guarded main/event tree classification cases preserve all guest registers/x87/memory; seam fails closed.')


if __name__ == '__main__':
    main()

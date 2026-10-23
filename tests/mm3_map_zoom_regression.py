"""Run the actual CMake map zoom seams and native road/marker geometry offline.

Requires `python scripts/devenv.py --shell`, generated code and the user's PAL XBE. Graphics
submission, player lookup and input are capture boundaries; native quad creation,
rotation, map scale setup and all guest calling conventions remain executable.
"""
from pathlib import Path
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]
SEAMS = ("sub_0011CA25", "sub_0011D1E8", "sub_0011D808", "sub_0011CAE8")
MATH = ("sub_0002DC95", "sub_0006ACC3", "sub_000C6ADC",
        "sub_0006AC61", "sub_0006AE0C", "sub_0006AEE2", "sub_0001259E",
        "sub_0006AD9F", "sub_0006AE60")


def generated():
    found = {}
    names = SEAMS + MATH
    for path in sorted((Path(os.environ.get("MM3_TEST_GENERATED_DIR", ROOT / "src/recomp/gen"))).glob("recomp_*.c")):
        source = path.read_text()
        for name in names:
            match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", source, re.M | re.S)
            if match:
                assert name not in found, name
                found[name] = match.group()
    assert found.keys() == set(names), set(names) - found.keys()
    return found


def constants(source):
    xbe = (ROOT / "game_files/default.xbe").read_bytes()
    u32 = lambda offset: struct.unpack_from("<I", xbe, offset)[0]
    sections = [struct.unpack_from("<4I", xbe, u32(0x120) - u32(0x104) + i * 56 + 4)
                for i in range(u32(0x11C))]
    def word(address):
        section = next(s for s in sections if s[0] <= address <= s[0] + s[3] - 4)
        return u32(section[2] + address - section[0])
    addresses = set()
    for kind, address in re.findall(r"MEM([FD])\(0x([0-9A-Fa-f]+)\)", source):
        addresses.add(int(address, 16))
        if kind == "D":
            addresses.add(int(address, 16) + 4)
    return "\nstatic void native_constants(void) {\n" + "".join(
        f"    MEM32(0x{address:X}) = 0x{word(address):08X}u;\n"
        for address in sorted(addresses)) + "}\n"


def main():
    cc.require_compiler()
    if not shutil.which("cmake"):
        raise SystemExit("Run `python scripts/devenv.py --shell` first so CMake is available.")
    cmake = (ROOT / "CMakeLists.txt").read_text()
    # The marker owner also has race-guidance seams; both owner groups can
    # register it before CMake removes duplicates.
    registered = set()
    for group in re.findall(r"list\(APPEND MM3_SEAM_FUNCTIONS\s+([^)]+)\)", cmake):
        registered.update(group.split())
    assert set(SEAMS) <= registered
    assert "foreach(map_owner " + " ".join(SEAMS) + ")" in cmake
    assert 'include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_map_zoom_seam.cmake")' in cmake
    assert "mm3_add_map_zoom(distance_content ${map_owner})" in cmake
    assert "src/mm3_map_zoom.c" in cmake
    native = generated()
    with tempfile.TemporaryDirectory(prefix="mm3-map-zoom-") as temporary:
        folder = Path(temporary)
        original_path, modified_path = folder / "original.c", folder / "modified.c"
        script = folder / "seam.cmake"
        script.write_text(
            f'include("{(ROOT / "scripts/mm3_map_zoom_seam.cmake").as_posix()}")\n'
            'file(READ "${INPUT}" content)\n'
            'mm3_add_map_zoom(content "${HOOKED}")\n'
            'file(WRITE "${OUTPUT}" "${content}")\n')
        def transform(source, name, reject=False):
            original_path.write_bytes(source.encode())
            result = subprocess.run(["cmake", f"-DINPUT={original_path}",
                                     f"-DOUTPUT={modified_path}", f"-DHOOKED={name}",
                                     "-P", str(script)], capture_output=True, text=True)
            if reject:
                assert result.returncode and "MM3 map zoom" in result.stderr, result
                return None
            if result.returncode:
                raise RuntimeError(result.stderr)
            return modified_path.read_text()
        modified = {}
        calls = {
            SEAMS[0]: "    mm3_map_zoom_reset(ecx);\n",
            SEAMS[1]: "    mm3_map_zoom_update(ecx, MEMF(esp + 0x10));\n",
            SEAMS[2]: "    mm3_map_zoom_apply(ebx, MEM32(ebp + -4));\n",
            SEAMS[3]: "    fp_top() = RECOMP_FP_PC(fp_top() * mm3_map_zoom_checkpoint_scale(ebx, MEM32(ebp + 0x40)));\n",
        }
        labels = {SEAMS[0]: "loc_0011CA25", SEAMS[1]: "loc_0011D1E8",
                  SEAMS[2]: "loc_0011D8F6", SEAMS[3]: "loc_0011CC53"}
        instructions = {
            SEAMS[0]: "    PUSH32(esp, ebp);",
            SEAMS[1]: "    PUSH32(esp, ebp);",
            SEAMS[2]: "    PUSH32(esp, edi);",
            SEAMS[3]: "    { double _t = fp_top(); fp_push(_t); } /* fld st(0) */",
        }
        for name in SEAMS:
            modified[name] = transform(native[name], name)
            assert transform(native[name].replace("\n", "\r\n"), name) == modified[name]
            assert modified[name].count(calls[name]) == 1
            restored = modified[name].replace(calls[name], "")
            assert restored[restored.index("void " + name):] == native[name]
            transform(native[name].replace(labels[name] + ": ;", labels[name] + "BAD: ;"), name, True)
            transform(native[name] + "\n" + native[name], name, True)
            label = labels[name] + ": ;\n"
            instruction = instructions[name]
            transform(native[name].replace(label, label + instruction + "\n" + label), name, True)
            unrelated = "\nvoid unrelated(void)\n{\n" + label + instruction + "\n}\n"
            separate = transform(native[name] + unrelated, name)
            assert separate.endswith(unrelated)
            assert separate.count(calls[name]) == 1
            transform(modified[name], name, True)
        body = "\n".join(native[name] for name in MATH)
        for name, title in zip(SEAMS, ("reset", "update", "draw", "marker")):
            body += "\n" + native[name].replace("void " + name, "void retail_" + title, 1)
            body += "\n" + modified[name].replace("void " + name, "void zoom_" + title, 1)
        declarations = "\n".join("void " + name + "(void);" for name in MATH)
        (folder / "native_map_zoom.inc").write_text(declarations + "\n" + body + constants(body))
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "tools/xboxrecomp/templates/runtime/recomp_types.h", folder / "recomp_base.h")
        production = (ROOT / "src/mm3_map_zoom.c").read_text()
        (folder / "production_map_zoom.c").write_text(production)
        executable = folder / f"contract{cc.exe_suffix()}"
        command = cc.compile_command([ROOT / "tests/mm3_map_zoom_contract.c"], executable,
                                     includes=[folder, ROOT / "tools/xboxrecomp/include"], std="c11",
                                     optimize="none", warnings="normal", werror=True,
                                     disable_warnings=("4101", "4102"), obj=folder / "contract.obj")
        subprocess.run(command, check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)
        if "--check-mutants" in sys.argv:
            mutants = {
                "missing common map scale": production.replace(
                    "MEMF(node + 0x50) = s_map_zoom[slot].baseline / s_map_zoom[slot].span;",
                    "MEMF(node + 0x50) = scale;"),
                "immediate zoom jump": production.replace(
                    "blend = 1.0f - expf(-dt / 0.6f);", "blend = 1.0f;"),
                "missing speed target": production.replace(
                    "speed = fabsf((float)SMEM32(player_record + 4));", "speed = 0.0f;"),
                "checkpoint zoom cancels speed zoom": production.replace(
                    "if (!s_map_zoom[slot].baseline) s_map_zoom[slot].baseline = scale;",
                    "s_map_zoom[slot].baseline = scale;"),
            }
            for label, mutant in mutants.items():
                assert mutant != production, label
                (folder / "production_map_zoom.c").write_text(mutant)
                subprocess.run(command, check=True, cwd=folder, capture_output=True)
                result = subprocess.run([str(executable)], cwd=folder, capture_output=True, text=True)
                assert result.returncode, "Regression failed to reject " + label
                print("PASS negative control:", label)
    print("PASS map zoom seams: unchanged native instructions, split owners, LF/CRLF and invalid anchors rejected")


if __name__ == "__main__":
    main()

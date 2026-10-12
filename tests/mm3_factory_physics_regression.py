"""Compile the production factory hooks against a bounded guest/ABI fixture."""
from pathlib import Path
import os
import json
import shutil
import sys
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def native_wheel_path():
    names = ("sub_001CBB52", "sub_001D2C3A", "sub_001D4794")
    found = {}
    generated = Path(os.environ.get("MM3_TEST_GENERATED_DIR", ROOT / "src/recomp/gen"))
    for path in sorted(generated.glob("recomp_*.c")):
        source = path.read_text()
        for name in names:
            match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", source, re.M | re.S)
            if match:
                assert name not in found, name
                found[name] = match.group()
    assert found.keys() == set(names), set(names) - found.keys()
    def function(name):
        return found[name]
    distribution = function("sub_001CBB52")
    body = function("sub_001D2C3A")
    def wrap(original, name, instructions):
        header = original[:original.index("\nloc_")]
        header = re.sub(r"void sub_\w+", "void " + name, header, count=1)
        cleanup = original[original.rindex("    #undef fp_push"):]
        return header + "\n" + instructions + "\n    return;\n" + cleanup
    def between(text, start, end):
        return text[text.index(start):text.index(end, text.index(start))]
    native = wrap(distribution, "native_distribute", between(distribution,
        "loc_001CBF14: ;", "loc_001CC080: ;") +
        "\nloc_001CC080: ; return;\n" + between(distribution,
        "loc_001CC15C: ;", "loc_001CC27E: ;"))
    native += wrap(body, "native_wheel_force", between(body,
        "loc_001D38EF: ;", "    fp_push(MEMF(ebx + 0x18));"))
    native += wrap(body, "native_accumulate_force", between(body,
        "loc_001D3A5E: ;", "    fp_push(MEMF(ebp + -60));"))
    native += wrap(body, "native_dt_inverse_mass", between(body,
        "    fp_push(MEMF(ecx + 0x14));", "    PUSH32(esp, 0x001D2C7Bu);"))
    native += wrap(body, "native_integrate_velocity", between(body,
        "    fp_push(MEMF(ebp + 0x40)); /* fld float */\n"
        "    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(ebp + -40));",
        "    eax = MEM32(ebx + 0x3C);\n    eax = MEM32(eax + 4);\n"
        "    fp_push(MEMF(eax + 0x98));"))
    native += wrap(function("sub_001D4794"), "native_physics_tick", between(
        function("sub_001D4794"), "loc_001D47AA: ;",
        "    _fa = (uint32_t)(MEM8(esi + 0x5C))"))
    # Read the pinned title's actual constant words, especially 384D00=2.0.
    # Assuming this constant was1 would hide the four-wheel amplification.
    xbe = (ROOT / "game_files/default.xbe").read_bytes()
    u32 = lambda offset: struct.unpack_from("<I", xbe, offset)[0]
    sections = [struct.unpack_from("<4I", xbe, u32(0x120) - u32(0x104) + i * 56 + 4)
                for i in range(u32(0x11C))]
    addresses = set()
    for kind, addr in re.findall(r"MEM([FD])\(0x([0-9A-Fa-f]+)\)", native):
        addresses.add(int(addr, 16))
        if kind == "D":
            addresses.add(int(addr, 16) + 4)
    constants = "\nstatic void native_constants(void) {\n"
    for addr in sorted(addresses):
        section = next(s for s in sections if s[0] <= addr < s[0] + s[3])
        constants += f"    MEM32(0x{addr:X}) = 0x{u32(section[2] + addr - section[0]):08X}u;\n"
    return native + constants + "}\n"


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-factory-physics-") as folder:
        folder = Path(folder)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        (folder / "production_wheel_path.inc").write_text(native_wheel_path())
        data = folder / "game_files/Data"
        data.mkdir(parents=True)
        if "--defaults" in sys.argv:
            sys.path.insert(0, str(ROOT / "tools/car-mods"))
            from factory_specs import factory_physics_settings
            config = json.loads((ROOT / "tools/car-mods/cars.json").read_text())
            (data / "mm3_factory_physics.ini").write_bytes(factory_physics_settings(config))
        else:
            (data / "mm3_factory_physics.ini").write_text(
                "# Only explicit factory profiles are emitted by prepare.py\n"
                "Viper 18000 .33 2 0 1 19000 .8\n"  # Retail identity must be ignored.
                "Form1 18000 .33 4 0 .2 14000 1 18000 .9 19000 .8\n")
        executable = folder / "contract.exe"
        mode = ["/O2", "/DMM3_FACTORY_WINDOWS_MAPPING"] if "--windows-mapping" in sys.argv else ["/Od"]
        if "--defaults" in sys.argv:
            mode.append("/DMM3_FACTORY_DEFAULTS")
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/wd4102", *mode, "/std:c11",
                        "/I" + str(folder), "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        str(ROOT / "tests/mm3_factory_physics_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        result = subprocess.run([str(executable)], capture_output=True, text=True, cwd=folder)
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr, end="")
        result.check_returncode()


if __name__ == "__main__":
    main()

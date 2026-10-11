"""Exercise camera hooks through the retail chase callers and projection math.

Run after scripts/enter-env.ps1. Requires generated code and the user's PAL XBE;
extracts its constants at test time. No game process, GPU or assets are modified.
"""
from pathlib import Path
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def native_source():
    names = ("sub_001EF87E", "sub_001EF738", "sub_000F4AC3", "sub_0025968E",
             "sub_001EF926")
    found = {}
    for path in sorted((ROOT / "src/recomp/gen").glob("recomp_*.c")):
        source = path.read_text()
        for name in names:
            match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", source, re.M | re.S)
            if match:
                assert name not in found, name
                found[name] = match.group()
    assert found.keys() == set(names), set(names) - found.keys()
    # Keep the real caller's stack frame, matrix call, visibility-field stores
    # and RET 16. Exclude unrelated D3D state queries / GPU upload calls.
    setup = found["sub_000F4AC3"]
    setup = (setup[:setup.index("loc_000F4AF5: ;")] +
             setup[setup.index("loc_000F4B6F: ;"):setup.index("    eax = MEM32(ebp + -16);")] +
             setup[setup.index("loc_000F4BAB: ;"):])
    setup = setup.replace("void sub_000F4AC3", "void native_projection_setup", 1)
    native = setup + "\n" + found["sub_0025968E"].replace(
        "void sub_0025968E", "void sub_0025968E_original", 1)
    for name, label in (("sub_001EF87E", "near"), ("sub_001EF738", "far")):
        native += "\n" + found[name]
        # Differential reference follows the identical generated caller with
        # only its common-solver dispatch redirected past the production hook.
        native += "\n" + found[name].replace("void " + name, "void retail_" + label, 1).replace(
            "sub_001EEC53", "sub_001EEC53_original")
    xbe = (ROOT / "game_files/default.xbe").read_bytes()
    u32 = lambda offset: struct.unpack_from("<I", xbe, offset)[0]
    sections = [struct.unpack_from("<4I", xbe, u32(0x120) - u32(0x104) + i * 56 + 4)
                for i in range(u32(0x11C))]
    def word(address):
        section = next(s for s in sections if s[0] <= address <= s[0] + s[3] - 4)
        return u32(section[2] + address - section[0])
    # Prove camera IDs against the executable's actual dispatch table and
    # corresponding generated branch, rather than assuming near means mode 0.
    assert word(0x001EFAFE + 5 * 4) == 0x001EF9D5
    assert word(0x001EFAFE + 7 * 4) == 0x001EF9FA
    dispatch = found["sub_001EF926"]
    assert "sub_001EF87E" in dispatch.split("loc_001EF9D5: ;")[1].split("\nloc_")[0]
    assert "sub_001EF738" in dispatch.split("loc_001EF9FA: ;")[1].split("\nloc_")[0]
    constants = set()
    for kind, addr in re.findall(r"MEM([FD])\(0x([0-9A-Fa-f]+)\)", native):
        constants.add(int(addr, 16))
        if kind == "D":
            constants.add(int(addr, 16) + 4)
    native += "\nstatic void native_constants(void) {\n"
    for address in sorted(constants):
        native += f"    MEM32(0x{address:X}) = 0x{word(address):08X}u;\n"
    return native + "}\n"


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    cmake = (ROOT / "CMakeLists.txt").read_text()
    assert "list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_001EEC53)" in cmake
    with tempfile.TemporaryDirectory(prefix="mm3-camera-") as folder:
        folder = Path(folder)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        production = (ROOT / "src/mm3_widescreen.c").read_text()
        (folder / "production_widescreen.c").write_text(production, encoding="utf-8")
        (folder / "native_camera.inc").write_text(native_source(), encoding="utf-8")
        executable = folder / "contract.exe"
        command = ["cl", "/nologo", "/W3", "/WX", "/wd4102", "/Od", "/std:c11",
                        "/I" + str(folder), "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        str(ROOT / "tests/mm3_camera_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)]
        subprocess.run(command, check=True, cwd=folder)
        for value in (None, "", "60", "80", "95.5", "100", "110", "nan", "inf", "59", "111", "80garbage"):
            environment = os.environ.copy()
            environment.pop("MM3_FOV_DEGREES", None)
            if value is not None:
                environment["MM3_FOV_DEGREES"] = value
            expected = value if value in ("60", "80", "95.5", "100", "110") else "0"
            result = subprocess.run([str(executable), expected], env=environment,
                                    capture_output=True, text=True, cwd=folder)
            print(result.stdout, end="")
            if result.returncode:
                print(result.stderr, end="")
            result.check_returncode()
        if "--check-mutants" in sys.argv:
            mutants = {
                "missing near-distance adjustment": production.replace(
                    "if (MEM32(g_esp) == 0x001EF922u)", "if (0)"),
                "matrix/culling FOV mismatch": production.replace(
                    "MEMF(g_ebp + 8) = fovy;", "(void)fovy;"),
                "write after native RET 20": production.replace(
                    "    sub_0025968E_original();", "    sub_0025968E_original();\n"
                    "    if (patch) MEMF(g_esp + 8) = aspect;"),
            }
            environment = os.environ.copy()
            environment["MM3_FOV_DEGREES"] = "100"
            for name, mutant in mutants.items():
                assert mutant != production, name
                (folder / "production_widescreen.c").write_text(mutant, encoding="utf-8")
                subprocess.run(command, check=True, cwd=folder, capture_output=True)
                result = subprocess.run([str(executable), "100"], env=environment,
                                        cwd=folder, capture_output=True, text=True)
                assert result.returncode, "Regression failed to reject " + name
                print("PASS negative control:", name)


if __name__ == "__main__":
    main()

"""Native map resource lifecycle and asynchronous renderer query contract.

Runs the generated map loader/unloader, with only resource allocation and base
class calls replaced. No game process or game assets are changed. Run inside `python scripts/devenv.py --shell` to make cl.exe available.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def native_source():
    names = ("sub_0011C742", "sub_0011CAAC", "sub_0007CEC3",
             "sub_001164F7", "sub_00116509")
    found = {}
    for path in sorted((Path(os.environ.get("MM3_TEST_GENERATED_DIR", ROOT / "src/recomp/gen"))).glob("recomp_*.c")):
        source = path.read_text()
        for name in names:
            match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", source, re.M | re.S)
            if match:
                assert name not in found, name
                found[name] = match.group()
    assert found.keys() == set(names), set(names) - found.keys()
    # The generic deleting destructor has unrelated, unreachable lifted tail
    # fragments after its first RET. Test its actual entry through that RET.
    destructor = found["sub_0007CEC3"]
    end = destructor.index("    esp += 8; return; /* ret 4 */")
    found["sub_0007CEC3"] = destructor[:end] + "    esp += 8; return;\n}\n"
    for name in names[:3]:
        found[name] = found[name].replace("void " + name, "void " + name + "_original", 1)
    return "\n".join(found[name] for name in names)


def main():
    cc.skip_unless_windows("the contract includes windows.h")
    cc.require_compiler()
    cmake = (ROOT / "CMakeLists.txt").read_text()
    assert "list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_0011C742 sub_0011CAAC sub_0007CEC3)" in cmake
    with tempfile.TemporaryDirectory(prefix="mm3-map-ui-") as folder:
        folder = Path(folder)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "src/mods/mods.h", folder / "mods.h")
        shutil.copyfile(ROOT / "src/mods/widescreen.c", folder / "production_widescreen.c")
        (folder / "native_map.inc").write_text(native_source(), encoding="utf-8")
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_map_ui_contract.c"], executable,
            includes=[folder, ROOT / "tools/xboxrecomp/include"], std="c11", optimize="speed",
            warnings="normal", werror=True, disable_warnings=("4102",),
            obj=folder / "contract.obj"), cwd=folder, check=True)
        subprocess.run([str(executable)], cwd=folder, check=True)


if __name__ == "__main__":
    main()

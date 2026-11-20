"""Compile and run the single-file seam contracts that have no runner of their own.

Each contract includes the source it tests and supplies its own guest state, so
it builds alone like mm3_collision_cell_regression.py does.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]
KERNEL = ROOT / "tools/xboxrecomp/src/kernel"
PLATFORM = ROOT / "tools/xboxrecomp/src/platform"
# contract -> runtime sources it links beside the source it includes
CONTRACTS = {
    "mm3_ai_difficulty_contract.c": [],
    "mm3_collision_contract.c": [],
    "mm3_custom_car_names_contract.c": [KERNEL / "guest_strings.c"],
    "mm3_dev_tools_contract.c": [],
    "mm3_teleport_contract.c": [PLATFORM / "line_server.c"],
    "mm3_video_capture_contract.c": [],
    "mm3_timing_hold_contract.c": [ROOT / "src/mm3_tick_clock.c"],
}


def run(name, extra, folder):
    executable = folder / f"{Path(name).stem}{cc.exe_suffix()}"
    subprocess.run(cc.compile_command(
        [ROOT / "tests" / name, *extra], executable, defines=["_CRT_SECURE_NO_WARNINGS="],  # empty, as the sources define it
        includes=[folder, ROOT / "tools/xboxrecomp/include", ROOT / "tools/xboxrecomp/src/kernel",
                  ROOT / "tools/xboxrecomp/src", ROOT / "tools/xboxrecomp/src/video", ROOT / "src/mods"], std="c11",
        optimize="speed", warnings="normal", werror=True,
        obj=f"{folder}{os.sep}"), check=True, cwd=folder)
    subprocess.run([str(executable)], check=True, cwd=folder)


def main():
    cc.require_compiler()
    failed = []
    with tempfile.TemporaryDirectory(prefix="mm3-seams-") as temporary:
        folder = Path(temporary)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "tools/xboxrecomp/templates/runtime/recomp_types.h", folder / "recomp_base.h")
        for name, extra in CONTRACTS.items():
            try:
                run(name, extra, folder)
            except subprocess.CalledProcessError as error:
                print(f"FAIL {name}: {error}", file=sys.stderr)
                failed.append(name)
    if failed:
        sys.exit(f"{len(failed)} seam contract(s) failed: {', '.join(failed)}")
    print(f"PASS {len(CONTRACTS)} seam contracts")


if __name__ == "__main__":
    main()

"""Compile and run the production retired-car lookup hook in isolation.

Run inside `python scripts/devenv.py --shell`. Uses the real guest register/memory header;
does not launch the game or open its archives or saved profiles.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-car-selection-") as temporary:
        folder = Path(temporary)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "tools/xboxrecomp/templates/runtime/recomp_types.h", folder / "recomp_base.h")
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_car_selection_compat_contract.c"], executable,
            includes=[folder, ROOT / "tools/xboxrecomp/include"], std="c11", optimize="none",
            warnings="high", werror=True, obj=folder / "contract.obj"), check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()

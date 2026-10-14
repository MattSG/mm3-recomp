"""Compile and run the production retired-car lookup hook in isolation.

Run after scripts/enter-env.ps1. Uses the real guest register/memory header;
does not launch the game or open its archives or saved profiles.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-car-selection-") as temporary:
        folder = Path(temporary)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        executable = folder / "contract.exe"
        subprocess.run([
            "cl", "/nologo", "/W4", "/WX", "/Od", "/std:c11",
            "/I" + str(folder),
            "/I" + str(ROOT / "tools/xboxrecomp/include"),
            str(ROOT / "tests/mm3_car_selection_compat_contract.c"),
            "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable),
        ], check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()

"""Compile and run facade fade contracts without launching or modifying MM3.

Run after scripts/enter-env.ps1 with tools/venv/Scripts/python.exe.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    hooks = re.search(r"set\(MM3_DISTANCE_FUNCTIONS\s+(.*?)\)", cmake, re.S).group(1)
    assert "sub_001630A7" in hooks
    assert "sub_001633B7" not in hooks
    assert "src/mm3_facade_capacity.c" not in cmake
    with tempfile.TemporaryDirectory(prefix="mm3-facade-fade-") as folder:
        folder = Path(folder)
        executable = folder / "contract.exe"
        subprocess.run(["cl", "/nologo", "/W4", "/Od", "/std:c11",
                        "/D_CRT_SECURE_NO_WARNINGS",
                        "/I" + str(ROOT / "src/recomp/gen"),
                        "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        str(ROOT / "tests/mm3_facade_fade_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        for scale in ("1", "2", "4"):
            subprocess.run([str(executable), scale], check=True)


if __name__ == "__main__":
    main()

"""Compile and run facade fade contracts without launching or modifying MM3.

Run inside `python scripts/devenv.py --shell` with tools/venv/Scripts/python.exe.
"""
from pathlib import Path
import re
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    hooks = re.search(r"set\(MM3_DISTANCE_FUNCTIONS\s+(.*?)\)", cmake, re.S).group(1)
    assert "sub_001630A7" in hooks
    assert "sub_001633B7" not in hooks
    assert "src/mm3_facade_capacity.c" not in cmake
    with tempfile.TemporaryDirectory(prefix="mm3-facade-fade-") as folder:
        folder = Path(folder)
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_facade_fade_contract.c"], executable,
            includes=[ROOT / "src/recomp/gen", ROOT / "tools/xboxrecomp/include"],
            defines=["_CRT_SECURE_NO_WARNINGS"], std="c11", optimize="none", warnings="high",
            obj=folder / "contract.obj"), check=True, cwd=folder)
        for scale in ("1", "2", "4"):
            subprocess.run([str(executable), scale], check=True)


if __name__ == "__main__":
    main()

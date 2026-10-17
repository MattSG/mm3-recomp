"""Compile and run the shared production profile helper with real environment variables."""
from pathlib import Path
import os
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-performance-profile-") as temporary:
        folder = Path(temporary)
        executable = folder / f"contract{cc.exe_suffix()}"
        # POSIX has no _putenv_s: the runtime compat layer provides it.
        sources = [ROOT / "tests/mm3_performance_profile_contract.c"]
        if os.name != "nt":
            sources.append(ROOT / "tools/xboxrecomp/src/platform/win32_compat.c")
        subprocess.run(cc.compile_command(
            sources, executable,
            includes=[ROOT / "src", ROOT / "tools/xboxrecomp/src"],
            defines=["_CRT_SECURE_NO_WARNINGS"], std="c11",
            optimize="none", warnings="high", werror=True, obj=folder / "contract.obj"),
            check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()

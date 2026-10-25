"""Run title overlay placement and route upload contracts without a GPU."""
from pathlib import Path
import os
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-overlay-") as temporary:
        folder = Path(temporary)
        executable = folder / f"contract{cc.exe_suffix()}"
        # POSIX has no Interlocked* in libc: the runtime compat layer provides them.
        sources = [ROOT / "tests/mm3_overlay_contract.c", ROOT / "src/runtime/piece_cluster.c"]
        if os.name != "nt":
            sources.append(ROOT / "tools/xboxrecomp/src/platform/win32_compat.c")
        subprocess.run(cc.compile_command(
            sources, executable,
            includes=[ROOT / "src/mods", ROOT / "tools/xboxrecomp/src/video", ROOT / "tools/xboxrecomp/src", ROOT / "tools/xboxrecomp/src/kernel"], defines=["_CRT_SECURE_NO_WARNINGS"], std="c11",
            optimize="speed", warnings="normal", werror=True),
            check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()

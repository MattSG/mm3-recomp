"""Check modular startup, disabled widescreen aspect and contextual menu input."""
from pathlib import Path
import os
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-mods-init-") as temporary:
        folder = Path(temporary)
        executable = folder / f"contract{cc.exe_suffix()}"
        # POSIX has no _putenv_s: the runtime compat layer provides it.
        sources = [ROOT / "tests/mm3_mods_init_contract.c", ROOT / "src/runtime/feature_switch.c"]
        if os.name != "nt":
            sources.append(ROOT / "tools/xboxrecomp/src/platform/win32_compat.c")
        subprocess.run(cc.compile_command(
            sources, executable,
            includes=[ROOT / "tools/xboxrecomp/src/video", ROOT / "tools/xboxrecomp/include", ROOT / "tools/xboxrecomp/src", ROOT / "tools/xboxrecomp/src/kernel"],
            defines=["_CRT_SECURE_NO_WARNINGS"], std="c11", optimize="speed", warnings="normal",
            werror=True), check=True, cwd=folder)
        for mode in ("enabled", "disabled"):
            subprocess.run([str(executable), mode], check=True, cwd=folder)


if __name__ == "__main__":
    main()

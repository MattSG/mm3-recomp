"""Check upstream particle budgets and retained explicit overrides at native ABI."""
from pathlib import Path
import os
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-particle-budget-") as temporary:
        folder = Path(temporary)
        executable = folder / f"contract{cc.exe_suffix()}"
        # POSIX has no _putenv_s: the runtime compat layer provides it.
        sources = [ROOT / "tests/mm3_particle_budget_contract.c",
                   ROOT / "tools/xboxrecomp/src/kernel/env_range.c",
                   ROOT / "tools/xboxrecomp/src/nv2a/nv2a_pb_reserve.c"]
        if os.name != "nt":
            sources.append(ROOT / "tools/xboxrecomp/src/platform/win32_compat.c")
        subprocess.run(cc.compile_command(
            sources, executable,
            includes=[ROOT / "src/recomp/gen", ROOT / "tools/xboxrecomp/include", ROOT / "tools/xboxrecomp/src", ROOT / "tools/xboxrecomp/src/nv2a", ROOT / "tools/xboxrecomp/src/video", ROOT / "tools/xboxrecomp/src/kernel"],
            defines=["_CRT_SECURE_NO_WARNINGS"], std="c11", optimize="none", warnings="normal",
            werror=True), check=True, cwd=folder)
        for mode in ("default", "baseline", "explicit", "low", "title", "0", "256", "garbage", "1extra"):
            subprocess.run([str(executable), mode], check=True, cwd=folder)


if __name__ == "__main__":
    main()

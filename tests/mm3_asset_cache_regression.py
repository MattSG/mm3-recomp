"""Compile the real cache helper and exercise it only in a temporary directory."""
from pathlib import Path
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.skip_unless_windows("the contract includes windows.h")
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-asset-cache-") as temporary:
        folder = Path(temporary)
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_asset_cache_contract.c"], executable,
            includes=[ROOT / "tools/xboxrecomp/src", ROOT / "tools/xboxrecomp/src/kernel"],
            defines=["_CRT_SECURE_NO_WARNINGS"], std="c11", optimize="none",
            warnings="high", werror=True, obj=folder / "contract.obj"), check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()

"""Compile the real cache helper and exercise it only in a temporary directory."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-asset-cache-") as temporary:
        folder = Path(temporary)
        executable = folder / "contract.exe"
        subprocess.run([
            "cl", "/nologo", "/W4", "/WX", "/Od", "/std:c11", "/D_CRT_SECURE_NO_WARNINGS",
            "/I" + str(ROOT / "tools/xboxrecomp/src"),
            "/I" + str(ROOT / "tools/xboxrecomp/src/kernel"),
            str(ROOT / "tests/mm3_asset_cache_contract.c"),
            "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable),
        ], check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()

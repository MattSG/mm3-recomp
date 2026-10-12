"""Differential-test the optimized visibility leaf against generated Xbox code.

Run after scripts/enter-env.ps1; --benchmark also reports helper throughput.
No installed assets or running game are accessed.
"""
import argparse
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmark", action="store_true")
    args = parser.parse_args()
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    signature = "void sub_001C793A(void)\n{"
    owners = []
    for path in (Path(os.environ.get("MM3_TEST_GENERATED_DIR", ROOT / "src/recomp/gen"))).glob("*.c"):
        contents = path.read_text(encoding="utf-8")
        if signature in contents:
            start = contents.index(signature)
            end = contents.index("\n}", start) + 2
            owners.append(contents[start:end])
    if len(owners) != 1:
        raise SystemExit(f"Expected one generated visibility leaf, found {len(owners)}")
    oracle = owners[0].replace("sub_001C793A", "visibility_oracle", 1)
    with tempfile.TemporaryDirectory(prefix="mm3-visibility-") as folder:
        folder = Path(folder)
        (folder / "visibility_oracle.inc").write_text(oracle, encoding="utf-8")
        executable = folder / "contract.exe"
        subprocess.run([
            "cl", "/nologo", "/w", "/O2", "/Ob1", "/std:c11",
            "/D_CRT_SECURE_NO_WARNINGS", "/DRECOMP_GENERATED_CODE",
            "/I" + str(folder), "/I" + str(Path(os.environ.get("MM3_TEST_GENERATED_DIR", ROOT / "src/recomp/gen"))),
            str(ROOT / "tests/mm3_visibility_contract.c"),
            "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable),
        ], check=True, cwd=folder)
        subprocess.run([str(executable)] + (["--benchmark"] if args.benchmark else []),
                       check=True)


if __name__ == "__main__":
    main()

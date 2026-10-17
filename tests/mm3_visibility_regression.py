"""Differential-test the optimized visibility leaf against generated Xbox code.

Run inside `python scripts/devenv.py --shell`; --benchmark also reports helper throughput.
No installed assets or running game are accessed.
"""
import argparse
from pathlib import Path
import os
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmark", action="store_true")
    args = parser.parse_args()
    cc.skip_unless_windows("the contract includes windows.h")
    cc.require_compiler()
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
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_visibility_contract.c"], executable,
            includes=[folder, Path(os.environ.get("MM3_TEST_GENERATED_DIR", ROOT / "src/recomp/gen"))],
            defines=["_CRT_SECURE_NO_WARNINGS", "RECOMP_GENERATED_CODE"], std="c11", optimize="speed",
            warnings="none", msvc_extra=["/Ob1"], obj=folder / "contract.obj"), check=True, cwd=folder)
        subprocess.run([str(executable)] + (["--benchmark"] if args.benchmark else []),
                       check=True)


if __name__ == "__main__":
    main()

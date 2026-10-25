"""Build and run the port's own runtime helpers (src/runtime) contracts."""
from pathlib import Path
import os
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]
RT = ROOT / "src/runtime"
INCLUDES = [RT, ROOT / "tools/xboxrecomp/src", ROOT / "tools/xboxrecomp/src/kernel",
            ROOT / "tools/xboxrecomp/src/nv2a", ROOT / "tools/xboxrecomp/include"]
CHECKS = [("geometry", ["polyline", "road_graph", "vec3_rotate", "kb_spline", "guest_read", "thick_line"])]
if os.name == "nt":  # debug registers and Win32 threads
    CHECKS += [("write_watch", ["host_write_watch"]), ("wbc_flush", ["nv2a_wbc_flush"])]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-runtime-") as temporary:
        folder = Path(temporary)
        for name, helpers in CHECKS:
            exe = folder / f"{name}{cc.exe_suffix()}"
            sources = [ROOT / f"tests/mm3_runtime_{name}_contract.c"] + [RT / f"{h}.c" for h in helpers]
            subprocess.run(cc.compile_command(sources, exe, includes=INCLUDES,
                defines=["_CRT_SECURE_NO_WARNINGS"], std="c11", optimize="none",
                warnings="normal", gcc_extra=["-lm"]),
                check=True, cwd=folder)
            subprocess.run([str(exe)], check=True, cwd=folder)


if __name__ == "__main__":
    main()

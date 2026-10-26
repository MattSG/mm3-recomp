"""Run the port's geometry contracts without a GPU or game assets."""
from pathlib import Path
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-geometry-") as temporary:
        folder = Path(temporary)
        executable = folder / f"contract{cc.exe_suffix()}"
        sources = [ROOT / "tests/mm3_geometry_contract.c"]
        sources += [ROOT / f"src/geometry/{name}.c" for name in
                    ("polyline", "road_graph", "vec3_rotate", "kb_spline", "thick_line")]
        sources.append(ROOT / "src/fixed_step.c")
        subprocess.run(cc.compile_command(sources, executable, std="c11",
                                         warnings="high", werror=True),
                       check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()

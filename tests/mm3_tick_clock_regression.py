"""Exercise host-rate independence, overload, reset and settings parsing."""
from pathlib import Path
import subprocess
import tempfile
import cc

ROOT = Path(__file__).resolve().parents[1]

def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-tick-clock-") as temporary:
        exe = Path(temporary) / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_tick_clock_contract.c", ROOT / "src/mm3_tick_clock.c"],
            exe, std="c11", warnings="high", werror=True), check=True, cwd=temporary)
        subprocess.run([str(exe)], check=True, cwd=temporary)

if __name__ == "__main__":
    main()

"""Check the real timing wrappers with a deterministic host and guest ABI fixture."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import cc

ROOT = Path(__file__).resolve().parents[1]

def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-timing-") as temporary:
        folder = Path(temporary)
        (folder / "platform").mkdir()
        (folder / "platform/xbox_winnt.h").write_text(
            "#ifndef TEST_XBOX_WINNT_H\n#define TEST_XBOX_WINNT_H\n"
            "#include <stddef.h>\n#include <stdint.h>\n"
            "typedef int BOOL;\ntypedef void *HANDLE;\n"
            "typedef struct { long long QuadPart; } LARGE_INTEGER;\n"
            "int QueryPerformanceCounter(LARGE_INTEGER*);\n"
            "int QueryPerformanceFrequency(LARGE_INTEGER*);\nvoid Sleep(unsigned long);\n"
            "#endif\n")
        (folder / "platform/frame_limit.h").write_text("int recomp_frame_limit_from_env(void);\n")
        (folder / "recomp_types.h").write_text(
            "#define MEM32(a) (*(uint32_t*)(memory+(a)))\n"
            "#define MEM8(a) memory[(a)]\n"
            "#define MEMF(a) (*(float*)(memory+(a)))\n"
            "#define MEMD(a) (*(double*)(memory+(a)))\n")
        (folder / "mods.h").write_text("enum {MM3_MOD_FRAMERATE}; int mm3_mod_on(int);\n")
        (folder / "nv2a_d3d11.h").write_text(
            "void nv2a_d3d_manual_present(int); void nv2a_d3d_repeat_frame(void);\n"
            "long nv2a_d3d_frames_done(void);\n")
        for source in (ROOT / "src/mods/timing.c", ROOT / "tests/mm3_timing_contract.c"):
            shutil.copy2(source, folder / source.name)
        exe = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [folder / "mm3_timing_contract.c", ROOT / "src/mm3_tick_clock.c"],
            exe, includes=[folder], defines=["_CRT_SECURE_NO_WARNINGS"],
            std="c11", warnings="high", werror=True), check=True, cwd=folder)
        subprocess.run([str(exe)], check=True, cwd=folder)

if __name__ == "__main__":
    main()

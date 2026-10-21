"""Exercise both backends' production presentation-rectangle getter bodies."""
from pathlib import Path
import re
import subprocess
import tempfile
import cc

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="present-rect-") as temporary:
    folder = Path(temporary)
    for backend in ("nv2a_d3d11.c", "nv2a_sdlgpu.c"):
        source = (ROOT / "tools/xboxrecomp/src/video" / backend).read_text(encoding="utf-8")
        body = re.search(r"^void nv2a_d3d_get_present_rect\(float present\[4\]\)\n\{.*?^\}", source, re.M | re.S)
        assert body, backend
        fixture = '''#include <assert.h>
#include <string.h>
static float s_present_rect[4];
static int s_present_lock, held, acquisitions;
static void take(int *lock) { assert(lock == &s_present_lock && !held); held = 1; acquisitions++; }
static void release(int *lock) { assert(lock == &s_present_lock && held); held = 0; }
#define AcquireSRWLockShared take
#define ReleaseSRWLockShared release
#define SDL_LockSpinlock take
#define SDL_UnlockSpinlock release
'''+body.group()+'''
int main(void) {
    float result[4] = {-1,-1,-1,-1};
    nv2a_d3d_get_present_rect(result);
    assert(!memcmp(result,s_present_rect,sizeof result) && !held && acquisitions == 1);
    float next[4] = {.125f,.25f,.75f,.5f};
    memcpy(s_present_rect,next,sizeof next);
    nv2a_d3d_get_present_rect(result);
    assert(!memcmp(result,next,sizeof result) && !held && acquisitions == 2);
    result[0] = 99;
    assert(s_present_rect[0] == .125f);
    return 0;
}
'''
        path = folder / "contract.c"
        path.write_text(fixture)
        executable = folder / ("contract"+cc.exe_suffix())
        subprocess.run(cc.compile_command([path], executable, obj=folder/"contract.obj", werror=True), check=True, cwd=folder)
        subprocess.run([str(executable)], check=True)
print("PASS: both production viewport getters, locking and copied results")

"""Compile the real pause callback against the title's extracted state writes.

The renderer's placement/seam contract is in test_menu_placement.py. This
contract checks its title-specific input: the real singleton address, native
open/resume writes, callback registration, and mapping/startup order.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", source, re.M | re.S)
    assert match, name
    return match.group()


def between(source, start, end):
    return source.split(start, 1)[1].split(end, 1)[0]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    names = ("sub_001E25C1", "sub_001E2CD5", "sub_001E2BDA")
    native = {}
    # Generated translation-unit boundaries change after regeneration.
    for path in sorted((ROOT / "src/recomp/gen").glob("recomp_*.c")):
        source = path.read_text()
        for name in names:
            if re.search(r"^void " + name + r"\(void\)\s*\{", source, re.M):
                assert name not in native, f"Duplicate definition: {name} in {path}"
                native[name] = function(source, name)
    assert native.keys() == set(names), f"Missing native functions: {set(names) - native.keys()}"
    singleton, opened, resumed = (native[name] for name in names)
    # Use the generated title's instructions, not copied addresses/state logic.
    singleton_return = between(singleton, "loc_001E25E6: ;", "\n}")
    open_write = between(opened, "loc_001E2E12: ;", "    g_ebp = ebp;")
    resume_write = between(resumed, "loc_001E2CC0: ;", "    PUSH32(esp, 0);")
    # Both writes target the same singleton passed in the original argument.
    assert "esi = MEM32(ebp + 8);" in opened
    assert "ebp = MEM32(esp + 8);" in resumed
    startup = (ROOT / "src/main.c").read_text()
    assert startup.count("    mm3_widescreen_init();") == 1
    assert (startup.index("if (!xbox_MemoryLayoutInit(") <
            startup.index("g_xbox_mem_offset = xbox_GetMemoryOffset();") <
            startup.index("    mm3_widescreen_init();") <
            startup.index("if (!xbox_HostHardwareInit()") <
            startup.index("if (!xbox_HostGraphicsInit("))
    assert "src/mm3_widescreen.c" in (ROOT / "CMakeLists.txt").read_text()
    harness = r'''
#include <assert.h>
#include <stdio.h>
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
static unsigned char memory[0x440000];
static int (*registered_query)(void);
static unsigned registrations;
void nv2a_d3d_set_centered_ui_query(int (*query)(void)) {
    assert(g_xbox_mem_offset == (ptrdiff_t)memory);
    registered_query = query; ++registrations;
}
float nv2a_d3d_display_aspect(void) { return 1.6f; }
void nv2a_d3d_set_edge_hud(int on) { (void)on; }
void nv2a_d3d_set_hud_start(float a, float b, float c, float d) {
    (void)a; (void)b; (void)c; (void)d;
}
void sub_0025968E_original(void) { g_esp += 24; }
void sub_001EEC53_original(void) { g_esp += 60; }
#include "production_widescreen.c"
static uint32_t eax, esi, esp;
static void native_singleton_return(void) { SINGLETON }
static void native_pause_open(uint32_t singleton) {
    esi = singleton;
    OPEN
}
static void native_pause_resume(uint32_t singleton) {
    uint32_t ebp = singleton, _fa;
    int32_t _fas;
    RESUME
    (void)_fa; (void)_fas;
}
int main(void) {
    g_xbox_mem_offset = (ptrdiff_t)memory;
    mm3_widescreen_init();
    assert(registrations == 1 && registered_query);
    esp = 0x10000;
    native_singleton_return();
    uint32_t singleton = eax;
    assert(registered_query() == 0);
    /* Neighbouring singleton flags and engine modes cannot imply pause. */
    MEM8(singleton + 1) = 0xFF;
    MEM8(singleton + 3) = 0xFF;
    MEM32(0x431610) = 7;
    assert(registered_query() == 0);
    for (unsigned i = 0; i < 100; ++i) {
        native_pause_open(singleton);
        assert(registered_query() == 1);
        /* Repeated draw queries must be observational and stay current. */
        for (unsigned j = 0; j < 10; ++j) {
            g_eax = 0x12345678; g_esp = 0x87654321;
            assert(registered_query() == 1);
            assert(g_eax == 0x12345678 && g_esp == 0x87654321);
        }
        native_pause_resume(singleton);
        assert(registered_query() == 0);
        assert(MEM8(singleton + 1) == 0xFF && MEM8(singleton + 3) == 0xFF);
    }
    puts("pause UI: native singleton/open/resume, 100 transitions, callback ABI and startup order passed");
    return 0;
}
'''.replace("SINGLETON", singleton_return).replace("OPEN", open_write).replace("RESUME", resume_write)
    with tempfile.TemporaryDirectory(prefix="mm3-pause-ui-") as folder:
        folder = Path(folder)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "src/mm3_widescreen.c", folder / "production_widescreen.c")
        (folder / "contract.c").write_text(harness)
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/O2", "/std:c11",
                        "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        "contract.c", "/Fe:contract.exe"], cwd=folder, check=True)
        subprocess.run([str(folder / "contract.exe")], cwd=folder, check=True)


if __name__ == "__main__":
    main()

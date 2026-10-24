"""Run the lifted Cruise stream copy (0x145DC) against its original contract.

The routine is generated into src/recomp/gen, so run the generator first. The
test copies that routine and its two lifted helpers (0x120B6 and 0x1F02AD) into
one host program, reads the 63-entry jump table from the XBE, and sweeps every
count up to 0x2FFF. Each run checks the copied bytes, the stream cursor, the
return registers, ESP after ret 8, and the callee-saved registers. The global
g_ebp is not checked: it is the frame published for frameless callees."""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/xboxrecomp"))
sys.path.insert(0, str(ROOT / "tools/xboxrecomp/tests"))
import cc  # noqa: E402

TABLE_VA = 0x0001548D
ENTRIES = 63
LIMIT = 0x2FFF


def extract(text, name):
    start = text.index(f"void {name}(void)\n{{")
    return text[start:text.index("\n}\n", start) + 3]


def jump_table():
    from tools.disasm.loader import load_image  # tools/xboxrecomp on sys.path
    image = load_image(str(ROOT / "game_files/default.xbe"))
    raw = image.read_bytes_at_va(TABLE_VA, ENTRIES * 4)
    return [int.from_bytes(raw[i:i + 4], "little") for i in range(0, ENTRIES * 4, 4)]


HARNESS = r"""
#define RECOMP_GENERATED_CODE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp, g_fs_base;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
RECOMP_TLS uint32_t g_recomp_resume;
RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3, g_mm4, g_mm5, g_mm6, g_mm7;

static const uint32_t cruise_table[63] = { TABLE };

void recomp_icall_fail_log(uint32_t va) { (void)va; }
recomp_func_t recomp_lookup(uint32_t xbox_va) { (void)xbox_va; return 0; }
recomp_func_t recomp_lookup_kernel(uint32_t xbox_va) { (void)xbox_va; return 0; }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { (void)xbox_va; return 0; }

/* Lifted helpers. */
HELPERS
/* Lifted routine under test. */
ROUTINE

static int failures;

static void check(int ok, const char *what, uint32_t count)
{
    if (!ok) {
        if (failures < 12)
            fprintf(stderr, "count=%u: %s\n", count, what);
        ++failures;
    }
}

int main(void)
{
    const uint32_t stream = 0x00400000u, source = 0x00500000u, dest = 0x00600000u;
    const uint32_t frame = 0x00300000u, limit = LIMIT;
    uint8_t *mem = calloc(1, 0x03000000u);
    uint32_t count;
    if (!mem) return 2;
    g_xbox_mem_offset = (ptrdiff_t)mem;
    memcpy(mem + 0x0001548Du, cruise_table, sizeof cruise_table);
    for (count = 0; count <= limit; ++count) {
        uint32_t i;
        memset(mem + dest, 0xEE, count + 16);
        for (i = 0; i < count; ++i)
            mem[source + i] = (uint8_t)(i * 31u + count);
        MEM32(stream + 0xCu) = source;
        MEM32(frame) = 0x12345678u;
        MEM32(frame + 4u) = dest;
        MEM32(frame + 8u) = count;
        g_ecx = stream; g_esp = frame; g_ebp = 0x00700000u;
        g_ebx = 0xB0B0B0B0u; g_esi = 0x51515151u; g_edi = 0xD1D1D1D1u;
        sub_000145DC();
        check(memcmp(mem + dest, mem + source, count) == 0, "copied bytes differ", count);
        check(MEM32(stream + 0xCu) == source + count, "stream cursor not advanced", count);
        check(g_eax == stream, "eax is not the stream", count);
        check(g_ecx == count, "ecx is not the count", count);
        check(g_edx == ((count & 63u) - 1u), "edx is not (count&63)-1", count);
        check(g_esp == frame + 12u, "esp is not entry+12 (ret 8)", count);
        check(g_ebx == 0xB0B0B0B0u && g_esi == 0x51515151u && g_edi == 0xD1D1D1D1u,
              "callee-saved ebx/esi/edi not restored", count);
        check(mem[dest + count] == 0xEE, "wrote past the end", count);
    }
    printf("cruise copy: %u counts, %d failures\n", limit + 1, failures);
    return failures ? 1 : 0;
}
"""


def main():
    gen = ROOT / "src/recomp/gen"
    if not (gen / "recomp_0000.c").exists() or not (gen / "recomp_0018.c").exists():
        raise SystemExit("run scripts/generate_mm3.py first: the lifted routine is generated")
    text0 = (gen / "recomp_0000.c").read_text(encoding="utf-8", errors="replace")
    text18 = (gen / "recomp_0018.c").read_text(encoding="utf-8", errors="replace")
    helpers = extract(text0, "sub_000120B6") + "\n" + extract(text18, "sub_001F02AD")
    routine = extract(text0, "sub_000145DC")
    table = ", ".join(f"0x{entry:08X}u" for entry in jump_table())
    source = (HARNESS.replace("TABLE", table).replace("HELPERS", helpers)
              .replace("ROUTINE", routine).replace("LIMIT", f"0x{LIMIT:04X}u"))
    with tempfile.TemporaryDirectory(prefix="mm3 cruise copy ") as folder:
        folder = Path(folder)
        source_file = folder / "cruise_copy.c"
        source_file.write_text(source, encoding="utf-8")
        executable = folder / f"cruise_copy{cc.exe_suffix()}"
        includes = [gen, ROOT / "src", ROOT / "tools/xboxrecomp/include"]
        subprocess.run(cc.compile_command(
            [source_file], executable, includes=includes, std="c11", optimize="speed",
            warnings="normal", defines=["_CRT_SECURE_NO_WARNINGS"],
            disable_warnings=("4102", "4101", "4189", "4100", "4996"), obj=folder / "cruise.obj"),
            check=True, cwd=folder)
        result = subprocess.run([str(executable)], capture_output=True, text=True, cwd=folder)
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr, end="")
        result.check_returncode()


if __name__ == "__main__":
    main()

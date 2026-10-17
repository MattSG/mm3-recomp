#!/usr/bin/env python3
"""Compile and run the memmove remainder-tail check against generated code.

Usage:
  python tools/scripts/check_memmove_tails.py [--generator-dir DIR]

Writes checks/check_memmove_tails.c under the generator directory, compiles it
with the four generated tails (recomp_00093b58/60/70/84_tail.c) and the shared
xboxrecomp include directory, then runs it. Windows uses cl.exe (run from a
Visual Studio developer prompt); elsewhere the first of $CC, cc, gcc, clang.

"""
import argparse
import os
import shutil
import subprocess

C_SOURCE = r'''#include "recomp_types.h"
#include <assert.h>
#include <stdio.h>
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
RECOMP_TLS uint32_t g_ebx, g_esi, g_edi, g_ebp, g_seh_ebp;
void sub_00093B58_gen(void);
void sub_00093B60_gen(void);
void sub_00093B70_gen(void);
void sub_00093B84_gen(void);
int main(void) {
    unsigned char memory[2048];
    void (*tails[])(void) = {sub_00093B58_gen, sub_00093B60_gen,
                            sub_00093B70_gen, sub_00093B84_gen};
    unsigned remainder, i;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    for (remainder = 0; remainder < 4; ++remainder) {
        memset(memory, 0, sizeof(memory));
        g_ebp = g_seh_ebp = 0x100;
        g_esp = 0xF8;
        MEM32(0xF8) = 0xABC123;
        MEM32(0xFC) = 0xDEF456;
        MEM32(0x100) = 0x180;
        MEM32(0x104) = 0x123456;
        MEM32(0x108) = 0x400;
        g_esi = 0x300; g_edi = 0x400;
        MEM8(0x301) = 11; MEM8(0x302) = 22; MEM8(0x303) = 33;
        tails[remainder]();
        assert(g_esp == 0x108 && g_eax == 0x400);
        assert(g_esi == 0xABC123 && g_edi == 0xDEF456);
        for (i = 0; i < 4; ++i)
            assert(MEM8(0x400 + i) ==
                   (i && i >= 4 - remainder ? MEM8(0x300 + i) : 0));
    }
    puts("PASS: memmove remainder tails preserve stack and registers");
    return 0;
}
'''

TAILS = ["00093b58", "00093b60", "00093b70", "00093b84"]


def die(msg):
    raise SystemExit(msg)


def compile_and_run(check_c, tails, exe, gen_dir, shared_include, cwd):
    if os.name == "nt":
        cmd = ["cl.exe", "/nologo", "/W3", "/I", gen_dir, "/I", shared_include, check_c, *tails, f"/Fe:{exe}"]
    else:
        cc = os.environ.get("CC") or next((shutil.which(c) for c in ("cc", "gcc", "clang") if shutil.which(c)), None)
        if not cc:
            die("no C compiler found (set CC, or install cc/gcc/clang)")
        cmd = [cc, "-Wall", "-I", gen_dir, "-I", shared_include, check_c, *tails, "-o", exe]
    try:
        rc = subprocess.run(cmd, cwd=cwd).returncode
    except FileNotFoundError:
        die(f"{cmd[0]} not found; on Windows run from a Visual Studio developer prompt")
    if rc:
        die("Memmove tail check compilation failed")
    if subprocess.run([exe], cwd=cwd).returncode:
        die("Memmove tail check failed")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--generator-dir", default="conformance_tmp/mm3_neg_flags_gen_20261001")
    args = parser.parse_args()

    here = os.path.dirname(os.path.abspath(__file__))
    repo_tools = os.path.join(here, "..", "..", "tools", "xboxrecomp", "include")
    gen_dir = os.path.abspath(args.generator_dir)
    if not os.path.isdir(gen_dir):
        die(f"Cannot find path '{gen_dir}' because it does not exist.")
    check_dir = os.path.join(gen_dir, "checks")
    os.makedirs(check_dir, exist_ok=True)
    check_c = os.path.join(check_dir, "check_memmove_tails.c")
    with open(check_c, "w", encoding="ascii", newline="\n") as f:
        f.write(C_SOURCE)
    tails = [os.path.join(gen_dir, f"recomp_{t}_tail.c") for t in TAILS]
    exe = os.path.join(check_dir, "check_memmove_tails.exe" if os.name == "nt" else "check_memmove_tails")
    compile_and_run(check_c, tails, exe, gen_dir, os.path.abspath(repo_tools), check_dir)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compile and run the worker bootstrap check against the project's source bodies.

Usage:
  python tools/scripts/check_worker_bootstrap.py [--source-file src/recomp_manual.c]

Extracts sub_00094FC0, sub_00083A6C and sub_001E7B8F from the source file into
conformance_tmp/worker_bootstrap_check, compiles them with
tools/scripts/check_worker_bootstrap.c, and runs the result. Windows uses
cl.exe (run from a Visual Studio developer prompt); elsewhere the first of
$CC, cc, gcc, clang. The C harness stays in tools/scripts beside this script, and the
port reads it in place.

"""
import argparse
import os
import re
import shutil
import subprocess

FUNCTIONS = ["sub_00094FC0", "sub_00083A6C", "sub_001E7B8F"]

# The harness uses MSVC's _set_error_mode; provide a no-op for other compilers.
POSIX_COMPAT = """/* POSIX stand-in for the MSVC CRT calls in check_worker_bootstrap.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _OUT_TO_STDERR 0
static int _set_error_mode(int mode) { (void)mode; return 0; }
"""


def die(msg):
    raise SystemExit(msg)


def extract_bodies(source):
    bodies = []
    for name in FUNCTIONS:
        match = re.search(r"(?ms)^void " + name + r"\(void\)\s*\{.*?^\}", source)
        if not match:
            die(f"Missing source body {name}")
        bodies.append(match.group(0))
    return "\n".join(bodies) + "\n"


def compile_and_run(harness, include, check_dir, exe):
    if os.name == "nt":
        cmd = ["cl.exe", "/nologo", "/W3", "/I", include, "/I", check_dir, harness,
               f"/Fo:{os.path.join(check_dir, 'check_worker_bootstrap.obj')}", f"/Fe:{exe}"]
    else:
        cc = os.environ.get("CC") or next((shutil.which(c) for c in ("cc", "gcc", "clang") if shutil.which(c)), None)
        if not cc:
            die("no C compiler found (set CC, or install cc/gcc/clang)")
        compat = os.path.join(check_dir, "posix_compat.h")
        with open(compat, "w", encoding="ascii") as f:
            f.write(POSIX_COMPAT)
        cmd = [cc, "-Wall", "-I", include, "-I", check_dir, "-include", compat, harness, "-o", exe]
    try:
        rc = subprocess.run(cmd).returncode
    except FileNotFoundError:
        die(f"{cmd[0]} not found; on Windows run from a Visual Studio developer prompt")
    if rc:
        die("Bootstrap check compilation failed")
    if subprocess.run([exe], cwd=check_dir).returncode:
        die("Bootstrap check failed")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--source-file", default="src/recomp_manual.c")
    args = parser.parse_args()

    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    check_dir = os.path.join(repo, "conformance_tmp", "worker_bootstrap_check")
    os.makedirs(check_dir, exist_ok=True)
    if not os.path.isfile(args.source_file):
        die(f"Cannot find path '{os.path.abspath(args.source_file)}' because it does not exist.")
    with open(args.source_file, encoding="utf-8", errors="replace") as f:
        source = f.read()
    with open(os.path.join(check_dir, "worker_bootstrap_body.inc"), "w", encoding="ascii", errors="replace") as f:
        f.write(extract_bodies(source))
    shutil.copyfile(os.path.join(repo, "src", "mm3_recomp_types.h"), os.path.join(check_dir, "recomp_types.h"))
    shutil.copyfile(os.path.join(repo, "tools", "xboxrecomp", "templates", "runtime", "recomp_types.h"), os.path.join(check_dir, "recomp_base.h"))

    harness = os.path.join(repo, "tools", "scripts", "check_worker_bootstrap.c")
    include = os.path.join(repo, "tools", "xboxrecomp", "include")
    exe = os.path.join(check_dir, "check_worker_bootstrap.exe" if os.name == "nt" else "check_worker_bootstrap")
    compile_and_run(harness, include, check_dir, exe)


if __name__ == "__main__":
    main()

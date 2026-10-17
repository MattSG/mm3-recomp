"""C compiler selection and command lines shared by the contract tests.

MSVC (cl) is used when it is on PATH on Windows. Otherwise $CC, then cc, gcc
or clang on PATH. CC_FORCE=msvc|gcc|clang|none overrides detection, which is
useful for checking the GCC/Clang translation on a Windows machine.

The MSVC command line is kept identical to the flags the tests used before.
"""

import os
import re
import shutil
import sys

NO_COMPILER_MESSAGE = (
    "No C compiler: on Windows run `python scripts/devenv.py --shell` (MSVC), "
    "elsewhere install gcc or clang."
)

# MSVC warning number -> GCC/Clang equivalent, for the warnings the tests
# disable. Unknown numbers are dropped.
_MSVC_DISABLE_TO_GCC = {
    "4100": "-Wno-unused-parameter",
    "4101": "-Wno-unused-variable",
    "4102": "-Wno-unused-label",
    "4189": "-Wno-unused-but-set-variable",
}

_DRIVER_CXX = {"gcc": "g++", "clang": "clang++", "cc": "c++"}


def _family(name):
    base = os.path.basename(name).lower()
    base = re.sub(r"\.exe$", "", base)
    if base == "cl":
        return "msvc"
    if "clang" in base:
        return "clang"
    return "gcc"


def compiler():
    """Return "msvc", "gcc", "clang" or None."""
    force = os.environ.get("CC_FORCE")
    if force:
        return None if force == "none" else force
    if os.name == "nt" and shutil.which("cl"):
        return "msvc"
    cc = os.environ.get("CC")
    if cc and shutil.which(cc):
        return _family(cc)
    for name in ("cc", "gcc", "clang"):
        if shutil.which(name):
            return _family(name)
    return None


def require_compiler():
    """Return the compiler family, or exit with the setup message."""
    family = compiler()
    if family is None:
        raise SystemExit(NO_COMPILER_MESSAGE)
    return family


def exe_suffix():
    return ".exe" if os.name == "nt" else ""


def skip_unless_windows(reason):
    """Exit 0 with a skip message when not on Windows."""
    if os.name != "nt":
        print(f"skipped: needs Windows ({reason})")
        raise SystemExit(0)


def _driver(family, cxx):
    if family == "msvc":
        return "cl"
    base = os.environ.get("CC") or ("clang" if family == "clang" else "gcc")
    base = os.path.basename(base)
    if cxx:
        base = re.sub(r"(gcc|clang|cc)$", lambda m: _DRIVER_CXX[m.group(1)], base)
    return base


def compile_command(sources, output, *, includes=(), defines=(), std="c11",
                    optimize="none", warnings="normal", werror=False,
                    disable_warnings=(), obj=None, compile_only=False,
                    libs=(), dynamic_crt=False, msvc_extra=(), gcc_extra=()):
    """Build the compile/link command for the detected compiler.

    sources: C/C++ files. output: the executable, or the object file when
    compile_only. obj: optional MSVC object path when linking (/Fo:).
    std: "c11" or "c++17". optimize: "none" (-O0 / /Od), "o1", "speed" (-O2 / /O2).
    warnings: "none" (/w), "normal" (/W3, -Wall), "high" (/W4, -Wall -Wextra).
    disable_warnings: MSVC numbers such as "4102". libs: names such as "user32",
    linked only on Windows for GCC/Clang. msvc_extra/gcc_extra: raw flags for one
    compiler only.
    """
    family = require_compiler()
    msvc = family == "msvc"
    cxx = std is not None and std.startswith("c++")
    cmd = [_driver(family, cxx)]
    sources = [str(s) for s in sources]
    includes = [str(i) for i in includes]
    output = str(output)

    if msvc:
        cmd.append("/nologo")
        cmd.append({"none": "/w", "normal": "/W3", "high": "/W4"}[warnings])
        if werror:
            cmd.append("/WX")
        cmd += [f"/wd{n}" for n in disable_warnings]
        cmd.append({"none": "/Od", "o1": "/O1", "speed": "/O2"}[optimize])
        if dynamic_crt:
            cmd.append("/MD")
        if std:
            cmd.append(f"/std:{std}")
        if cxx:
            cmd.append("/EHsc")
        cmd += [f"/D{d}" for d in defines]
        cmd += [f"/I{i}" for i in includes]
        cmd += list(msvc_extra)
        if compile_only:
            cmd.append("/c")
        cmd += sources
        if compile_only:
            cmd.append(f"/Fo:{output}")
        else:
            if obj:
                cmd.append(f"/Fo:{obj}")
            cmd.append(f"/Fe:{output}")
        cmd += [f"{lib}.lib" for lib in libs]
        return cmd

    cmd += {"none": ["-w"], "normal": ["-Wall"], "high": ["-Wall", "-Wextra"]}[warnings]
    if werror:
        cmd.append("-Werror")
    for n in disable_warnings:
        if n in _MSVC_DISABLE_TO_GCC:
            cmd.append(_MSVC_DISABLE_TO_GCC[n])
    cmd.append({"none": "-O0", "o1": "-O1", "speed": "-O2"}[optimize])
    if std:
        cmd.append(f"-std={std}")
    cmd += [f"-D{d}" for d in defines]
    cmd += [f"-I{i}" for i in includes]
    cmd += list(gcc_extra)
    if compile_only:
        cmd += ["-c", "-o", output]
        cmd += sources
    else:
        cmd += sources
        cmd += ["-o", output]
        if sys.platform == "win32":
            cmd += [f"-l{lib}" for lib in libs]
        else:
            cmd.append("-lm")  # sin/cos/floor/exp2 etc. are in libm on glibc
    return cmd

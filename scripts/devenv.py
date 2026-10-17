"""Local build environment shared by scripts/setup.py and other tools.

    python scripts/devenv.py           print the environment that would be used
    python scripts/devenv.py --shell   start a shell with it

The environment is the MSVC toolchain (Windows only: the local
tools/bin/msvc bundle, else Visual Studio's vcvars64) plus tools/venv on PATH.
"""
import argparse
import ctypes
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
IS_WIN = sys.platform == "win32"


def portable_msvc():
    """The local MSVC bundle's setup script, or None."""
    bat = REPO / "tools/bin/msvc/setup_x64.bat"
    return bat if IS_WIN and bat.exists() else None


def find_vs():
    """Install path of the newest Visual Studio with the x64 C++ tools, or None."""
    if not IS_WIN:
        return None
    pf = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(pf) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.exists():
        return None
    out = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                          "-property", "installationPath"],
                         capture_output=True, text=True)
    lines = out.stdout.strip().splitlines()
    return lines[0].strip() if out.returncode == 0 and lines else None


def msvc_env():
    """Environment after activating MSVC, as a dict; None off Windows or when no MSVC is found."""
    if not IS_WIN:
        return None
    setup = portable_msvc()
    if not setup:
        vs = find_vs()
        if not vs:
            return None
        setup = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
        if not setup.exists():
            return None
    comspec = os.environ.get("ComSpec", "cmd.exe")
    # Passed as one raw command line: subprocess would backslash-escape the
    # inner quotes, which cmd does not understand.
    cmdline = f'"{comspec}" /d /s /c "call "{setup}" >nul && set"'
    out = subprocess.run(cmdline, capture_output=True)
    if out.returncode != 0:
        raise SystemExit(f"Could not activate MSVC ({setup}).")
    text = out.stdout.decode(f"cp{ctypes.windll.kernel32.GetOEMCP()}", errors="replace")
    env = {}
    for line in text.splitlines():
        if "=" in line and not line.startswith("="):
            name, value = line.split("=", 1)
            env[name] = value
    return env


def _key(env, name):
    """The key in env for name; Windows environment names are case-insensitive."""
    if IS_WIN:
        for k in env:
            if k.upper() == name.upper():
                return k
    return name


def _get(env, name, default=""):
    return env.get(_key(env, name), default)


def _put(env, name, value):
    env[_key(env, name)] = value


def venv_python():
    return REPO / "tools/venv" / ("Scripts/python.exe" if IS_WIN else "bin/python")


def _added_dirs():
    return [d for d in (REPO / "tools/bin/powershell", venv_python().parent) if d.is_dir()]


def build_env(msvc=None):
    """os.environ with the MSVC variables (from msvc_env()) and tools/venv on PATH."""
    env = dict(os.environ)
    for name, value in (msvc or {}).items():
        _put(env, name, value)
    old = [p for p in _get(env, "PATH").split(os.pathsep) if p]
    new = [str(d) for d in _added_dirs()]
    _put(env, "PATH", os.pathsep.join(dict.fromkeys(new + old)))
    if venv_python().exists():
        env["VIRTUAL_ENV"] = str(REPO / "tools/venv")
        env.pop(_key(env, "PYTHONHOME"), None)
    env["PYTHONUTF8"] = "1"
    return env


def which(name, env=None):
    return shutil.which(name, path=_get(env, "PATH") if env else None)


def cmake_exe(env=None):
    """CMake to use: Visual Studio's copy on Windows (unless portable MSVC), else the one on PATH."""
    if IS_WIN and not portable_msvc():
        vs = find_vs()
        if vs:
            bundled = Path(vs) / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
            if bundled.exists():
                return str(bundled)
    return which("cmake", env)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--shell", action="store_true", help="start an interactive shell with this environment")
    args = ap.parse_args(argv)

    msvc = msvc_env()
    env = build_env(msvc)
    if args.shell:
        shell = os.environ.get("ComSpec", "cmd.exe") if IS_WIN else os.environ.get("SHELL", "/bin/sh")
        print(f"Entering a shell in {REPO} with this environment; exit to leave.")
        raise SystemExit(subprocess.call([shell], env=env, cwd=REPO))

    vs = find_vs()
    portable = portable_msvc()
    print(f"repo          {REPO}")
    print(f"platform      {sys.platform}")
    print(f"python        {sys.executable} ({sys.version.split()[0]})")
    if IS_WIN:
        print(f"visual studio {vs or 'not found'}")
        print(f"local msvc    {portable or 'not found'}")
        print(f"msvc env      {'%d variables' % len(msvc) if msvc else 'none (no MSVC found)'}")
    cmake = cmake_exe(env)
    print(f"cmake         {cmake or 'NOT FOUND'}")
    print(f"ninja         {which('ninja', env) or 'not found'}")
    if not IS_WIN:
        print(f"cc            {which('cc', env) or 'NOT FOUND'}")
        print(f"c++           {which('c++', env) or 'NOT FOUND'}")
    vpy = venv_python()
    print(f"venv python   {vpy} ({'exists' if vpy.exists() else 'missing'})")
    print(f"PATH adds     {', '.join(str(d) for d in _added_dirs()) or 'nothing'}")
    print(f"VIRTUAL_ENV   {_get(env, 'VIRTUAL_ENV', '(unset)')}")


if __name__ == "__main__":
    main()

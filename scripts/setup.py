"""Build a playable Midtown Madness 3 from a fresh clone and your own disc image.

    python scripts/setup.py --iso "D:/Midtown Madness 3 (USA).iso"

1. checks the toolchain (Visual Studio or local MSVC, CMake, a C/C++ compiler off Windows) and Python
2. makes tools/venv with the pinned generator dependency (capstone)
3. verifies the disc's code and extracts it to game_files/
4. generates the translated C into src/recomp/gen/ (about 6 minutes)
5. builds build/<Config>/mm3_recomp.exe (Windows) or build/mm3_recomp
6. captures the audio DSP programs from a short run of the game, then rebuilds
   so they are compiled to native code

Re-running skips finished steps; --force redoes them.
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from devenv import (IS_WIN, REPO, build_env, cmake_exe, find_vs, msvc_env,  # noqa: E402
                    portable_msvc, venv_python, which)
from generate_mm3 import generation_key

# Capstone 5.0.7 is the tested disassembler; its version affects generated output.
CAPSTONE = "capstone==5.0.7"
DSP_FILES = ("gp.p.bin", "ep.p.bin")
DSP_DIR = REPO / "game_files/dsp"


def step(text):
    print(f"\n== {text}", flush=True)


def run(cmd, what, env=None):
    rc = subprocess.run([str(c) for c in cmd], cwd=REPO, env=env).returncode
    if rc != 0:
        raise SystemExit(f"{what} failed (exit {rc})")


def base_python():
    """The interpreter behind this one, so a venv rebuild never overwrites a running Python."""
    return getattr(sys, "_base_executable", sys.executable)


def prerequisites():
    """Returns (msvc env or None, cmake path, generator kind)."""
    step("Prerequisites")
    if sys.version_info < (3, 10):
        raise SystemExit("Python 3.10 or newer is required.")
    if IS_WIN and sys.version_info < (3, 11, 4):
        raise SystemExit("Python 3.11.4 or newer is required (3.12+ recommended).")
    if not which("git"):
        raise SystemExit("git not found on PATH.")
    if not (REPO / "tools/xboxrecomp/CMakeLists.txt").exists():
        run(["git", "submodule", "update", "--init", "--recursive"], "git submodule update")

    msvc = None
    portable = None
    if IS_WIN:
        vs = find_vs()
        portable = portable_msvc()
        if not vs and not portable:
            raise SystemExit("Visual Studio 2022+ with C++ tools, or local tools/bin/msvc/setup_x64.bat, is required.")
        msvc = msvc_env()
        print(f"Local MSVC: {portable.parent}" if portable else f"Visual Studio: {vs}")

    env = build_env(msvc)
    cmake = cmake_exe(env)
    if not cmake:
        raise SystemExit("CMake not found: install CMake locally or the VS CMake component.")
    if IS_WIN:
        if portable and not which("ninja", env):
            raise SystemExit("Ninja not found: install it in tools/venv with python -m pip install ninja.")
        kind = "ninja-multi" if portable else "vs"
    else:
        for tool in ("cc", "c++"):
            if not which(tool, env):
                raise SystemExit(f"{tool} not found: install a C/C++ compiler.")
        kind = "ninja" if which("ninja", env) else "make"
    return msvc, cmake, kind


def python_env(force, msvc):
    step("Python environment (tools/venv)")
    vpy = venv_python()
    if force or not vpy.exists():
        # No --clear: on Windows the venv's own python.exe may be the running interpreter (locked).
        run([base_python(), "-m", "venv", REPO / "tools/venv"], "venv")
    run([vpy, "-m", "pip", "install", "--quiet", "--disable-pip-version-check", CAPSTONE],
        "pip install", build_env(msvc))


def game_files(iso, skip_hash, force, msvc):
    step("Game files (game_files/)")
    if force or not (REPO / "game_files/default.xbe").exists():
        if not iso:
            raise SystemExit("Pass --iso <your Midtown Madness 3 disc image>.")
        cmd = [venv_python(), "scripts/extract_disc.py", iso]
        if skip_hash:
            cmd.append("--skip-iso-hash")
        run(cmd, "disc extraction", build_env(msvc))
    else:
        print("game_files/default.xbe present (use --force to re-extract)")


def generate(force, msvc):
    step("Generate translated code (src/recomp/gen/)")
    generated = REPO / "src/recomp/gen"
    marker = generated / "generation.sha256"
    current = marker.is_file() and marker.read_text() == generation_key()
    if force or not (generated / "recomp_dispatch.c").exists() or not current:
        run([venv_python(), "scripts/generate_mm3.py"], "generation", build_env(msvc))
    else:
        print("src/recomp/gen matches the fork and MM3 recipe (use --force to regenerate)")


def configure(config, renderer, cmake, kind, msvc, build_dir="build"):
    step(f"Build ({config})")
    if kind == "vs":
        gen = ["-A", "x64"]
    elif kind == "ninja-multi":
        gen = ["-G", "Ninja Multi-Config"]
    elif kind == "ninja":
        gen = ["-G", "Ninja", f"-DCMAKE_BUILD_TYPE={config}"]
    else:
        gen = ["-G", "Unix Makefiles", f"-DCMAKE_BUILD_TYPE={config}"]
    run([cmake, "-S", ".", "-B", build_dir, *gen, f"-DXBOXRECOMP_RENDERER={renderer}"],
        "cmake configure", build_env(msvc))


def compile_game(config, cmake, msvc, build_dir="build"):
    run([cmake, "--build", build_dir, "--config", config, "--target", "mm3_recomp", "--parallel"],
        "build", build_env(msvc))


def exe_path(config, build_dir="build"):
    if IS_WIN:
        return REPO / build_dir / config / "mm3_recomp.exe"
    return REPO / build_dir / "mm3_recomp"


def all_dsp():
    return all((DSP_DIR / f).exists() for f in DSP_FILES)


def capture_dsp(exe, msvc):
    """Run the game hidden and muted until it saves the DSP programs (or 2 minutes pass)."""
    # The game's DirectSound builds these programs at run time from templates
    # in its code, so the game itself makes them.
    saves = Path(tempfile.mkdtemp(prefix="mm3-dsp-capture-"))
    env = build_env(msvc)
    env.update({
        "RECOMP_PB_EXEC": "1", "RECOMP_USB": "1", "RECOMP_AUDIO_MUTE": "1",
        "RECOMP_DSP_PROGRAM_DIR": str(DSP_DIR),
        "MM3_PERFORMANCE_PROFILE": "LowPower", "MM3_SAVE_DIR": str(saves),
    })
    game = subprocess.Popen([str(exe)], cwd=REPO, env=env, stdin=subprocess.DEVNULL,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    deadline = time.monotonic() + 120
    try:
        while game.poll() is None and time.monotonic() < deadline and not all_dsp():
            time.sleep(2)
    finally:
        if game.poll() is None:
            game.kill()
        game.wait()
        shutil.rmtree(saves, ignore_errors=True)
    return all_dsp()


def dsp_step(exe, config, cmake, msvc, build_dir="build"):
    step("Audio DSP programs (game_files/dsp/)")
    if all_dsp():
        print("Captured and compiled in (delete game_files/dsp to capture again)")
        return
    print("Running the game, hidden and muted, to capture them...")
    if capture_dsp(exe, msvc):
        compile_game(config, cmake, msvc, build_dir)
    else:
        print("WARNING: The DSP programs were not saved. Audio still works through the interpreter; "
              "the game saves them as you play and the next build compiles them.", file=sys.stderr)


def main(argv=None):
    default_renderer = "D3D11" if IS_WIN else "SDL3_GPU"
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--iso", type=Path, help="your Midtown Madness 3 disc image (needed when game_files/ is missing)")
    ap.add_argument("--config", choices=["RelWithDebInfo", "Release"], default="RelWithDebInfo")
    ap.add_argument("--skip-iso-hash", action="store_true", help="skip the (slow) whole-image hash")
    ap.add_argument("--force", action="store_true", help="redo finished steps")
    ap.add_argument("--renderer", choices=["D3D11", "SDL3_GPU"], default=default_renderer,
                    help=f"host renderer (default: {default_renderer})")
    ap.add_argument("--build-dir", default="build", help="CMake build directory (default: build)")
    args = ap.parse_args(argv)
    iso = args.iso.resolve() if args.iso else None

    msvc, cmake, kind = prerequisites()
    python_env(args.force, msvc)
    game_files(iso, args.skip_iso_hash, args.force, msvc)
    generate(args.force, msvc)
    configure(args.config, args.renderer, cmake, kind, msvc, args.build_dir)
    compile_game(args.config, cmake, msvc, args.build_dir)

    exe = exe_path(args.config, args.build_dir)
    dsp_step(exe, args.config, cmake, msvc, args.build_dir)
    print(f"\nBuilt {exe}")
    if args.build_dir == "build":
        print("Play:  python scripts/play.py    (--keyboard to drive with the keyboard)")
    else:
        print(f"Play:  {exe}")


if __name__ == "__main__":
    main()

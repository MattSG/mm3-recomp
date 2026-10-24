#!/usr/bin/env python3
"""Repeatable menu and race benchmark with an isolated save copy.

Usage:
  python tools/bench/run.py --save <save template> [--profile Balanced] [--tag name]
      [--exe PATH] [--scenario Stationary|Driving] [--hitch-trace] [--fps-limit N]
      [--secs 20] [--particle-budget 24] [--shader-cache-seed DIR]
      [--x X --z Z [--heading RAD]]

Retains settings, executable identity, frame CSV and results under
conformance_tmp/bench/<tag>. Unreached scenes and game exits fail the run.
Finishes by running tools/xboxrecomp/tools/bench/summarize.py on the evidence directory.

The harness drives menus through the game's MM3_DEBUG_TELEPORT endpoint (named
pipe on Windows, Unix socket elsewhere). CPU time comes from GetProcessTimes on
Windows and /proc/<pid>/stat elsewhere. The archive's creation time is restored
with SetFileTime on Windows only; POSIX cannot set it, so the game rebuilds its
archive cache for that run.

Teleport calls reuse tools/scripts/send_teleport.py.
"""
import argparse
import ctypes
import hashlib
import importlib.util
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time
import uuid

PROFILES = {
    "Quality": {"scale": "3.0", "msaa": "8", "distance": "2", "fps": 60},
    "LowPower": {"scale": "1.5", "msaa": "1", "distance": "1", "fps": 30},
    "Balanced": {"scale": "2.0", "msaa": "4", "distance": "2", "fps": 60},
}
TAG_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$")


def die(msg):
    raise SystemExit(msg)


def game_exe(build_dir, config):
    return os.path.join(build_dir, config, "mm3_recomp.exe" if os.name == "nt" else "mm3_recomp")


def _kernel32():
    from ctypes import wintypes

    class FILETIME(ctypes.Structure):
        _fields_ = [("lo", wintypes.DWORD), ("hi", wintypes.DWORD)]

    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.OpenProcess.restype = wintypes.HANDLE
    k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    k32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(FILETIME)] * 4
    k32.CreateFileW.restype = wintypes.HANDLE
    k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    k32.SetFileTime.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(FILETIME)] * 3
    k32.CloseHandle.argtypes = [wintypes.HANDLE]
    return k32, FILETIME


def process_cpu_ms(pid):
    """Total user+kernel CPU time of a process, in milliseconds."""
    if os.name != "nt":
        # /proc/<pid>/stat: after the ')' of the command name, field 3 is index 0,
        # so utime (field 14) and stime (field 15) are indexes 11 and 12.
        with open(f"/proc/{pid}/stat") as f:
            fields = f.read().rsplit(")", 1)[1].split()
        return (int(fields[11]) + int(fields[12])) * 1000.0 / os.sysconf("SC_CLK_TCK")
    k32, FILETIME = _kernel32()
    handle = k32.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        created, exited, kernel, user = FILETIME(), FILETIME(), FILETIME(), FILETIME()
        if not k32.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited),
                                   ctypes.byref(kernel), ctypes.byref(user)):
            raise ctypes.WinError(ctypes.get_last_error())
        ticks = lambda f: (f.hi << 32) | f.lo  # noqa: E731 - 100 ns units
        return (ticks(kernel) + ticks(user)) / 10000.0
    finally:
        k32.CloseHandle(handle)


def copy_creation_time(src, dst):
    """Give dst the creation time of src (the game keys its archive cache on it)."""
    if os.name != "nt":
        print("warning: POSIX cannot set a file's creation time; the game will rebuild "
              "its archive cache for this run", flush=True)
        return
    k32, FILETIME = _kernel32()
    ticks = int(os.path.getctime(src) * 10_000_000) + 116_444_736_000_000_000
    stamp = FILETIME(ticks & 0xFFFFFFFF, ticks >> 32)
    handle = k32.CreateFileW(dst, 0x100, 0x7, None, 3, 0x80, None)  # FILE_WRITE_ATTRIBUTES, OPEN_EXISTING
    if not handle or handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        if not k32.SetFileTime(handle, ctypes.byref(stamp), None, None):
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        k32.CloseHandle(handle)


def load_teleport():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "scripts", "send_teleport.py")
    spec = importlib.util.spec_from_file_location("send_teleport", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def last_csv_line(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    return lines[-1] if lines else ""


def hidden_startupinfo():
    if os.name != "nt":
        return None
    si = subprocess.STARTUPINFO()
    si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    si.wShowWindow = subprocess.SW_HIDE
    return si


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--exe")
    parser.add_argument("--save", required=True, help="save template directory (copied, never modified)")
    parser.add_argument("--tag", default="bench-" + uuid.uuid4().hex)
    parser.add_argument("--profile", choices=["Balanced", "Quality", "LowPower"], default="Balanced")
    parser.add_argument("--scenario", choices=["Stationary", "Driving"], default="Stationary")
    parser.add_argument("--hitch-trace", dest="hitch_trace", action="store_true")
    parser.add_argument("--fps-limit", dest="fps_limit", type=int, help="1-90 (default from profile)")
    parser.add_argument("--secs", type=int, default=20, help="measurement window, 5-300 (default 20)")
    parser.add_argument("--particle-budget", dest="particle_budget", type=int, default=24, help="1-255")
    parser.add_argument("--shader-cache-seed", dest="shader_cache_seed")
    parser.add_argument("--x", type=float)
    parser.add_argument("--z", type=float)
    parser.add_argument("--heading", type=float, default=0.0)
    args = parser.parse_args()

    if not TAG_RE.match(args.tag):
        die("--tag must start with a letter or digit and contain only letters, digits, '_' and '-' (max 80)")
    if not 5 <= args.secs <= 300:
        die("--secs must be between 5 and 300")
    if args.fps_limit is not None and not 1 <= args.fps_limit <= 90:
        die("--fps-limit must be between 1 and 90")
    if not 1 <= args.particle_budget <= 255:
        die("--particle-budget must be between 1 and 255")

    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    exe = os.path.abspath(args.exe or game_exe(os.path.join(repo, "build"), "RelWithDebInfo"))
    save = os.path.abspath(args.save)
    if not os.path.isfile(exe):
        die("Exe must be a file.")
    if not os.path.isdir(save):
        die("Save must be a directory.")
    if any(v is not None and not math.isfinite(v) for v in (args.x, args.z)):
        die("Coordinates must be finite.")
    if not math.isfinite(args.heading):
        die("Heading must be finite.")
    if (args.x is None) != (args.z is None):
        die("Supply both X and Z, or omit both to measure the saved scene spawn.")
    explicit_position = args.x is not None

    bench_root = os.path.abspath(os.path.join(repo, "conformance_tmp", "bench"))
    run_dir = os.path.abspath(os.path.join(bench_root, args.tag))
    if not os.path.normcase(run_dir).startswith(os.path.normcase(bench_root + os.sep)):
        die("Run directory must be inside conformance_tmp\\bench.")
    if os.path.normcase(run_dir).startswith(os.path.normcase(save.rstrip(os.sep) + os.sep)):
        die("The benchmark directory cannot be inside its save template.")
    seed = None
    if args.shader_cache_seed:
        seed = os.path.abspath(args.shader_cache_seed)
        if not os.path.isdir(seed):
            die("ShaderCacheSeed must be a directory.")
    if os.path.exists(run_dir):
        die(f"Run directory already exists: {run_dir}")

    os.makedirs(run_dir)
    run_save = os.path.join(run_dir, "save")
    shutil.copytree(save, run_save)
    cached_archive = os.path.join(save, "Cache", "Data", "Data_hd.zip")
    if os.path.isfile(cached_archive):
        copy_creation_time(cached_archive, os.path.join(run_save, "Cache", "Data", "Data_hd.zip"))
    shader_cache = os.path.join(run_dir, "shader-cache")
    os.makedirs(shader_cache)
    if seed:
        for entry in os.scandir(seed):
            target = os.path.join(shader_cache, entry.name)
            if entry.is_dir():
                shutil.copytree(entry.path, target)
            else:
                shutil.copy2(entry.path, target)
    pad = os.path.join(run_dir, "pad.txt")
    with open(pad, "w"):
        pass
    csv = os.path.join(run_dir, "frames.csv")
    log = os.path.join(run_dir, "stderr.log")
    profile = PROFILES[args.profile]
    fps_limit = args.fps_limit if args.fps_limit is not None else profile["fps"]
    envs = {
        "RECOMP_PB_EXEC": "1", "RECOMP_FB_WINDOW": "1", "RECOMP_USB": "1", "RECOMP_PAD_LIVE": pad,
        "RECOMP_APU_DSP_ACK": "0x80458810", "MM3_SAVE_DIR": run_save, "MM3_PERFORMANCE_PROFILE": args.profile,
        "RECOMP_ASPECT": "16:10", "RECOMP_WINDOW_SIZE": "1280x800", "RECOMP_RENDER_SCALE": profile["scale"],
        "RECOMP_MSAA": profile["msaa"], "RECOMP_DRAW_DISTANCE": profile["distance"], "RECOMP_ANISO": "16",
        "RECOMP_FPS_LIMIT": str(fps_limit), "RECOMP_PAUSE_UNFOCUSED": "0", "RECOMP_D3D_PROFILE": "1",
        "RECOMP_FRAME_CSV": csv, "MM3_DEBUG_TELEPORT": "1",
        "MM3_PARTICLE_MAX_OVERDRAW": str(args.particle_budget), "RECOMP_SHADER_CACHE": "1",
        "RECOMP_SHADER_CACHE_DIR": shader_cache, "RECOMP_PAD_PRESS": None, "RECOMP_PAD_SCRIPT": None,
        "RECOMP_HITCH_TRACE": "1" if args.hitch_trace else None,
        "RECOMP_PRESENT_CAPTURE": None, "RECOMP_CAPTURE_SURFACE": None,
    }
    child_env = dict(os.environ)
    for key, value in envs.items():
        if value is None:
            child_env.pop(key, None)
        else:
            child_env[key] = value

    teleport = load_teleport()
    windows = []
    proc = None
    gpu = None
    stdout_f = log_f = None

    def assert_running():
        if proc.poll() is not None:
            die(f"Game exited with code {proc.returncode}; see {log}")

    def wait_game(seconds):
        start = time.monotonic()
        while time.monotonic() - start < seconds:
            assert_running()
            time.sleep(0.25)
        assert_running()

    def press_pad(button, after):
        assert_running()
        with open(pad, "a") as f:
            f.write(f"{button}:150\n")
        wait_game(0.2 + after)

    def measure_window(name, seconds):
        cpu_start = process_cpu_ms(proc.pid)
        begin = time.perf_counter() * 1000.0
        wait_game(seconds)
        end = time.perf_counter() * 1000.0
        windows.append({"name": name, "start": begin, "end": end,
                        "cpu_ms": process_cpu_ms(proc.pid) - cpu_start,
                        "logical_processors": os.cpu_count()})

    def write_json(name, data):
        with open(os.path.join(run_dir, name), "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

    try:
        stdout_f = open(os.path.join(run_dir, "stdout.log"), "wb")
        log_f = open(log, "wb")
        proc = subprocess.Popen([exe], cwd=repo, env=child_env, stdout=stdout_f, stderr=log_f,
                                startupinfo=hidden_startupinfo())
        with open(exe, "rb") as f:
            digest = hashlib.sha256(f.read()).hexdigest().upper()
        write_json("run.json", {
            "executable": exe, "sha256": digest,
            "process_id": proc.pid, "profile": args.profile, "scenario": args.scenario,
            "settings": envs, "explicit_position": explicit_position,
            "coordinates": [args.x or 0.0, args.z or 0.0], "heading": args.heading,
            "save_template": save, "shader_cache_seed": seed, "logical_processors": os.cpu_count(),
        })
        print(f"Game PID {proc.pid}, evidence {run_dir}", flush=True)

        # Skip intro movies, title -> profile -> menu -> race.
        intro_start = time.monotonic()
        frontend_ready = False
        while time.monotonic() - intro_start < 120:
            wait_game(0.7)
            if os.path.isfile(csv) and os.path.getsize(csv) > 128:
                frontend_ready = True
                break
            press_pad("start", 0)
        if not frontend_ready:
            die("No frontend guest flips before the startup timeout.")
        wait_game(5)
        press_pad("start", 4)
        press_pad("a", 6)
        measure_window("frontend", 10)
        for button, after in [("down", 1), ("a", 4), ("a", 4), ("a", 5), ("a", 8), ("a", 10), ("a", 9), ("start", 6)]:
            press_pad(button, after)

        position = None
        for _ in range(5):
            assert_running()
            try:
                position = teleport.teleport(proc.pid)
            except Exception:
                position = None
            if position and position.get("available"):
                break
            press_pad("a", 9)
        if not position or not position.get("available"):
            die("No live player car; race benchmark is invalid.")
        write_json("position-start.json", position)
        if explicit_position:
            position = teleport.teleport(proc.pid, args.x, args.z, args.heading)
        write_json("position.json", position)
        wait_game(4)

        if shutil.which("nvidia-smi"):
            gpu = subprocess.Popen(
                ["nvidia-smi", "--query-gpu=utilization.gpu,power.draw", "--format=csv,noheader,nounits", "-lms", "500"],
                stdout=open(os.path.join(run_dir, "gpu.csv"), "wb"),
                stderr=open(os.path.join(run_dir, "gpu-errors.txt"), "wb"),
                startupinfo=hidden_startupinfo())

        # A fixed throttle input exercises streaming, traffic and effects. It is
        # a repeatable input, not a deterministic route: collisions can differ.
        measure_position = teleport.teleport(proc.pid)
        write_json("position-measure-start.json", measure_position)
        if args.scenario == "Driving":
            with open(pad, "a") as f:
                f.write(f"rt:{args.secs * 1000 + 500}\n")
            wait_game(0.25)
        measure_window("race", args.secs)
        end_position = teleport.teleport(proc.pid)
        write_json("position-end.json", end_position)
        if args.scenario == "Driving" and (
                not measure_position.get("available") or not end_position.get("available") or
                (end_position["x"] - measure_position["x"]) ** 2 +
                (end_position["z"] - measure_position["z"]) ** 2 < 1):
            die("Driving benchmark did not move the player car by at least one metre.")

        # The CSV flushes every 120 flips. Wait for persisted evidence covering
        # the complete window, including the 30 FPS profile, before stopping.
        flush_start = time.monotonic()
        persisted = False
        while time.monotonic() - flush_start < 15:
            wait_game(0.25)
            try:
                last_timestamp = float(last_csv_line(csv).split(",")[0])
            except (ValueError, OSError):
                last_timestamp = None
            if last_timestamp is not None and last_timestamp >= windows[-1]["end"]:
                persisted = True
                break
        if not persisted:
            die("Frame CSV did not persist the complete measurement window.")
        write_json("windows.json", windows)
    finally:
        if gpu and gpu.poll() is None:
            gpu.kill()
        if proc:
            if proc.poll() is None:
                proc.kill()
            proc.wait(timeout=10)
        for handle in (stdout_f, log_f):
            if handle:
                handle.close()
        resolved_save = os.path.abspath(run_save)
        if (resolved_save != os.path.abspath(os.path.join(run_dir, "save")) or
                not resolved_save.startswith(bench_root + os.sep)):
            die("Refusing to remove a save outside this benchmark run.")
        shutil.rmtree(resolved_save, ignore_errors=True)  # 5 GB per run

    venv_python = os.path.join(repo, "tools", "venv", "Scripts" if os.name == "nt" else "bin",
                               "python.exe" if os.name == "nt" else "python")
    python = venv_python if os.path.exists(venv_python) else sys.executable
    summary = subprocess.run([python, os.path.join(repo, "tools", "xboxrecomp", "tools", "bench", "summarize.py"),
                              run_dir])
    if summary.returncode:
        die("Benchmark summary failed; see retained evidence.")


if __name__ == "__main__":
    main()

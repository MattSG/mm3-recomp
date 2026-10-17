"""Run the recompiled Midtown Madness 3 built by scripts/setup.py.

  python scripts/play.py [--keyboard] [--fullscreen] [--profile Balanced|Quality|LowPower]
                         [--fps-limit N] [--fov DEG] [--trace-buildings | --no-trace-buildings]
                         [--allow-multiple-instances] [--exe PATH]

Any RECOMP_*/MM3_* variable already set in the
environment overrides these defaults. Every launch saves combined, timestamped
output and launch metadata under logs/. Only the child process gets the
defaults; the caller's environment is never modified.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import os
import re
import subprocess
import sys
import tempfile
import threading
import uuid
from contextlib import ExitStack

if os.name == "nt":
    import msvcrt
else:
    import fcntl

IS_WINDOWS = os.name == "nt"
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
CONSOLE_KEY = re.compile(r"^RECOMP_XBOX_(HDKEY|SIGKEY|LANKEY)$")
CMAKE_OPTION = re.compile(r"^((?:MM3_|RECOMP_|CMAKE_(?:C_COMPILER|C_FLAGS|BUILD_TYPE|CONFIGURATION_TYPES|GENERATOR|PROJECT_VERSION|EXE_LINKER_FLAGS))[^:]*):[^=]+=(.*)$")
GAME_PROCESS = "mm3_recomp"
TRACE_KEYS = ("MM3_BUILDING_TRACE", "RECOMP_TEXTURE_STREAM_TRACE", "RECOMP_ASYNC_WAIT_TRACE")
CONFIG_FILES = [
    "game_files/default.xbe", "game_files/Data/mm3_factory_physics.ini",
    "game_files/Data/mm3_speedometers.ini", "game_files/.mm3-car-mods/state.json",
    "tools/car-tools/car-mods/cars.local.json", "tools/car-tools/car-mods/cars.json",
    "build/CMakeCache.txt",
]
SOURCE_FILES = [
    "CMakeLists.txt", "src/mods/draw_distance.c", "src/mods/widescreen.c",
    "src/mm3_performance_profile.h", "src/mm3_building_diagnostics.c", "src/main.c",
    "src/recomp_manual.c", "tools/xboxrecomp/src/kernel/kernel_bridge.c",
    "tools/xboxrecomp/src/video/nv2a_d3d11.c",
]
PROFILE_DEFAULTS = {
    "quality": {"render_scale": "3.0", "msaa": "8", "draw_distance": "2", "aniso": "16", "fps_limit": "60"},
    "lowpower": {"render_scale": "1.5", "msaa": "1", "draw_distance": "1", "aniso": "16", "fps_limit": "30"},
}
DEFAULT_PROFILE_DEFAULTS = {"render_scale": "2.0", "msaa": "4", "draw_distance": "2", "aniso": "16", "fps_limit": "60"}


class LaunchError(Exception):
    pass


def repo_path(relative):
    return os.path.join(REPO, *relative.split("/"))


def utc_text(when):
    return when.strftime("%Y-%m-%dT%H:%M:%S.%fZ")


def format_fov(value):
    # Invariant decimal point regardless of locale: 60 -> "60", 100.5 -> "100.5".
    return str(int(value)) if float(value).is_integer() else repr(float(value))


def in_range(convert, low, high):
    def parse(text):
        try:
            value = convert(text)
        except ValueError:
            raise argparse.ArgumentTypeError(f"invalid value: {text!r}") from None
        if not (math.isfinite(value) and low <= value <= high):
            raise argparse.ArgumentTypeError(f"must be between {low} and {high}")
        return value
    return parse


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Run the recompiled Midtown Madness 3 built by scripts/setup.py.")
    parser.add_argument("--keyboard", action="store_true")
    parser.add_argument("--fullscreen", action="store_true")
    parser.add_argument("--profile", choices=["Balanced", "Quality", "LowPower"], default="Balanced")
    parser.add_argument("--fps-limit", type=in_range(int, 0, 1000), metavar="N")
    parser.add_argument("--fov", type=in_range(float, 60, 110), metavar="DEG")
    parser.add_argument("--trace-buildings", action="store_true")
    parser.add_argument("--no-trace-buildings", action="store_true")
    parser.add_argument("--allow-multiple-instances", action="store_true")
    parser.add_argument("--exe", metavar="PATH")
    return parser.parse_args(argv)


def find_exe(requested):
    if requested is None:
        suffix = ".exe" if IS_WINDOWS else ""
        candidates = [repo_path("build/%s/mm3_recomp%s" % (configuration, suffix))
                      for configuration in ("RelWithDebInfo", "Release")]
        # Single-config generators (setup.py on Linux/macOS) put the game directly in build/.
        candidates.append(repo_path("build/mm3_recomp%s" % suffix))
        requested = next((path for path in candidates if os.path.isfile(path)), None)
    if requested is None or not os.path.isfile(requested):
        raise LaunchError("No build found: run scripts/setup.py first.")
    return os.path.abspath(requested)


def launch_file(path):
    if not os.path.isfile(path):
        return None
    stat = os.stat(path)
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return {
        "path": os.path.abspath(path), "size": stat.st_size,
        "last_write_utc": utc_text(dt.datetime.fromtimestamp(stat.st_mtime, dt.timezone.utc)),
        "sha256": digest.hexdigest(),
    }


def cmake_cache_path(exe):
    for candidate in (os.path.join(os.path.dirname(exe), "CMakeCache.txt"),
                      os.path.join(os.path.dirname(os.path.dirname(exe)), "CMakeCache.txt")):
        if os.path.isfile(candidate):
            return candidate
    return repo_path("build/CMakeCache.txt")


def read_cmake_options(path):
    options = {}
    if os.path.isfile(path):
        with open(path, encoding="utf-8", errors="replace") as handle:
            for line in handle:
                match = CMAKE_OPTION.match(line.rstrip("\r\n"))
                if match:
                    options[match.group(1)] = match.group(2)
    return options


def installed_car_configuration():
    path = repo_path("game_files/.mm3-car-mods/state.json")
    if not os.path.isfile(path):
        return None
    try:
        with open(path, encoding="utf-8-sig") as handle:
            return json.load(handle)
    except (OSError, ValueError) as error:
        return {"parse_error": str(error)}


def save_metadata(path, metadata):
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")


def save_metadata_or_warn(path, metadata):
    try:
        save_metadata(path, metadata)
    except OSError as error:
        print(f"Warning: Cannot update launch metadata: {error}", file=sys.stderr)


def running_game_pids():
    if IS_WINDOWS:
        # Localized "INFO:" text can be in a code page the locale decoder rejects.
        result = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], capture_output=True, text=True,
                                errors="replace")
        rows = csv.reader(result.stdout.splitlines())
        pids = [row[1] for row in rows if len(row) > 1 and row[0].lower() == "mm3_recomp.exe"]
    else:
        try:
            result = subprocess.run(["pgrep", "-x", GAME_PROCESS], capture_output=True, text=True)
            pids = result.stdout.split()
        except FileNotFoundError:
            pids = []
            for entry in os.listdir("/proc") if os.path.isdir("/proc") else []:
                if not entry.isdigit():
                    continue
                try:
                    with open(f"/proc/{entry}/comm", encoding="utf-8") as handle:
                        if handle.read().strip() == GAME_PROCESS:
                            pids.append(entry)
                except OSError:
                    continue
    return sorted(int(pid) for pid in pids)


def try_lock(fd):
    try:
        if IS_WINDOWS:
            os.lseek(fd, 0, os.SEEK_SET)
            msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
        else:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except OSError:
        return False


def acquire_launcher_lock():
    """Return a locked fd, or None when another launcher holds it. Closing the fd releases it."""
    fd = os.open(os.path.join(tempfile.gettempdir(), "mm3-recomp-launcher.lock"), os.O_RDWR | os.O_CREAT, 0o644)
    if try_lock(fd):
        return fd
    os.close(fd)
    return None


def lock_save_root(root):
    os.makedirs(root, exist_ok=True)
    fd = os.open(os.path.join(root, ".mm3-launch.lock"), os.O_RDWR | os.O_CREAT, 0o644)
    if not try_lock(fd):
        os.close(fd)
        raise LaunchError(
            f"Another Midtown Madness 3 launcher is using save directory '{root}'. "
            "Use a distinct MM3_SAVE_DIR so writable cache files are not shared.")
    return fd


def echo(text):
    try:
        sys.stdout.write(text + "\n")
        sys.stdout.flush()
    except (OSError, ValueError):
        pass


class RunLog:
    """Serialises lines from both pipes into one log, flushing each line."""

    def __init__(self, path):
        self.handle = open(path, "x", encoding="utf-8", newline="\n")
        self.lock = threading.Lock()
        self.capture_error = None

    def write(self, source, raw):
        text = raw.decode("utf-8", "replace").rstrip("\r\n")
        stamp = dt.datetime.now(dt.timezone.utc).isoformat(timespec="microseconds")
        with self.lock:
            try:
                self.handle.write(f"{stamp} [{source}] {text}\n")
                self.handle.flush()
            except (OSError, ValueError) as error:
                if self.capture_error is None:
                    self.capture_error = repr(error)
            echo(text)


def pump(stream, source, log):
    try:
        for raw in iter(stream.readline, b""):
            log.write(source, raw)
    finally:
        stream.close()


def launch(args):
    exe = find_exe(args.exe)
    if not os.path.isfile(repo_path("game_files/default.xbe")):
        raise LaunchError("game_files/ missing: run scripts/setup.py --iso <disc>.")
    trace = args.trace_buildings and not args.no_trace_buildings
    if args.allow_multiple_instances and not os.environ.get("MM3_SAVE_DIR", "").strip():
        raise LaunchError("--allow-multiple-instances requires an explicit MM3_SAVE_DIR; "
                          "use a different save directory for each game instance.")

    defaults = {
        "RECOMP_PB_EXEC": "1", "RECOMP_FB_WINDOW": "1", "RECOMP_USB": "1",
        "RECOMP_APU_DSP_ACK": "0x80458810", "MM3_PERFORMANCE_PROFILE": args.profile,
        "RECOMP_ASPECT": "16:10",
    }
    if args.fps_limit is not None:
        defaults["RECOMP_FPS_LIMIT"] = str(args.fps_limit)
    if args.fov is not None:
        defaults["MM3_FOV_DEGREES"] = format_fov(args.fov)
    if args.keyboard:
        defaults["RECOMP_KEYBOARD"] = "1"
    if args.fullscreen:
        defaults["RECOMP_FULLSCREEN"] = "1"
    if trace:
        defaults.update({key: "1" for key in TRACE_KEYS})

    env = dict(os.environ)
    is_game = os.path.basename(exe).lower() in ("mm3_recomp", "mm3_recomp.exe")
    with ExitStack() as stack:
        if is_game:
            if not args.allow_multiple_instances:
                lock_fd = acquire_launcher_lock()
                if lock_fd is not None:
                    stack.callback(os.close, lock_fd)
                running = running_game_pids()
                if running:
                    pids = ", ".join(str(pid) for pid in running)
                    raise LaunchError(
                        f"Midtown Madness 3 is already running (PID {pids}). Close all existing game "
                        "windows before launching again, or use --allow-multiple-instances with a "
                        "distinct MM3_SAVE_DIR to isolate its cache.")
                if lock_fd is None:
                    raise LaunchError(
                        "Another Midtown Madness 3 launcher is active. Close it before launching again, "
                        "or use --allow-multiple-instances with a distinct MM3_SAVE_DIR.")
            # Writable cache and save files need separate roots; the lock file
            # recognizes aliases that resolve to the same directory.
            saved = os.environ.get("MM3_SAVE_DIR", "")
            if saved:
                save_root = os.path.abspath(saved)
                env["MM3_SAVE_DIR"] = save_root
            else:
                # Must match kernel_path.c: LocalAppData on Windows (else SaveData
                # under the cwd, which is the repo here), XDG_DATA_HOME or ~/.local/share elsewhere.
                if IS_WINDOWS:
                    base = os.environ.get("LOCALAPPDATA")
                    save_root = os.path.join(base, "xboxrecomp") if base else repo_path("SaveData")
                else:
                    data_home = os.environ.get("XDG_DATA_HOME") or os.path.join(
                        os.path.expanduser("~"), ".local", "share")
                    save_root = os.path.join(data_home, "xboxrecomp")
            stack.callback(os.close, lock_save_root(save_root))

        for key, value in defaults.items():
            if not env.get(key):
                env[key] = value

        logs_dir = repo_path("logs")
        os.makedirs(logs_dir, exist_ok=True)
        now = dt.datetime.now(dt.timezone.utc)
        run_name = f"play-{now:%Y%m%d-%H%M%S}-{now.microsecond // 1000:03d}-{uuid.uuid4().hex[:8]}"
        log_path = os.path.join(logs_dir, run_name + ".log")
        metadata_path = os.path.join(logs_dir, run_name + ".json")

        settings = {}
        for key in sorted(env):
            if re.match(r"^(MM3_|RECOMP_)", key):
                settings[key] = "[REDACTED]" if CONSOLE_KEY.match(key) else env[key]

        if os.path.splitext(exe)[1].lower() == ".py":
            command, arguments = sys.executable, [exe]
        else:
            command, arguments = exe, []

        cache = cmake_cache_path(exe)
        cmake_options = read_cmake_options(cache)
        metadata = {
            "schema_version": 1, "started_utc": utc_text(now), "state": "starting",
            "log_path": log_path, "working_directory": REPO,
            "executable": launch_file(exe),
            "build_configuration": os.path.basename(os.path.dirname(exe)),
            "build_context_at_launch": {
                "cmake_cache": launch_file(cache), "cmake_options": cmake_options,
                "source_files": [f for f in (launch_file(repo_path(p)) for p in SOURCE_FILES) if f],
            },
            "command": command, "arguments": arguments,
            "launch_options": {
                "profile": args.profile, "keyboard": args.keyboard, "fullscreen": args.fullscreen,
                "trace_buildings": trace, "allow_multiple_instances": args.allow_multiple_instances,
                "fps_limit": args.fps_limit, "fov": args.fov,
            },
            "environment": settings,
            "profile_defaults_for_unset_settings": PROFILE_DEFAULTS.get(
                str(settings.get("MM3_PERFORMANCE_PROFILE")).lower(), DEFAULT_PROFILE_DEFAULTS),
            "configuration_files": [f for f in (launch_file(repo_path(p)) for p in CONFIG_FILES) if f],
            "installed_car_configuration": installed_car_configuration(),
        }
        save_metadata(metadata_path, metadata)
        print(f"Game log: {log_path}")
        print(f"Launch details: {metadata_path}")
        if trace:
            print("Building diagnostics: " + " ".join(f"{key}={settings.get(key)}" for key in TRACE_KEYS))

        log = RunLog(log_path)
        stack.callback(log.handle.close)
        try:
            process = subprocess.Popen([command, *arguments], cwd=REPO, env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            metadata["state"] = "running"
            metadata["process_id"] = process.pid
            save_metadata_or_warn(metadata_path, metadata)
            readers = [threading.Thread(target=pump, args=(process.stdout, "stdout", log), daemon=True),
                       threading.Thread(target=pump, args=(process.stderr, "stderr", log), daemon=True)]
            for reader in readers:
                reader.start()
            exit_code = process.wait()
            for reader in readers:
                reader.join()
        except Exception as error:
            metadata.update(state="launch_failed", launch_error=str(error), finished_utc=utc_text(
                dt.datetime.now(dt.timezone.utc)))
            save_metadata_or_warn(metadata_path, metadata)
            raise

        metadata.update(state="exited", exit_code=exit_code,
                        finished_utc=utc_text(dt.datetime.now(dt.timezone.utc)))
        if log.capture_error:
            metadata["capture_error"] = log.capture_error
            print(f"Warning: Game output could not be fully saved: {log.capture_error}", file=sys.stderr)
        save_metadata_or_warn(metadata_path, metadata)
        return exit_code


def main(argv=None):
    args = parse_args(argv)
    for stream in (sys.stdout, sys.stderr):
        if stream is not None:
            stream.reconfigure(errors="replace")
    try:
        return launch(args)
    except (LaunchError, OSError) as error:
        print(f"play.py: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

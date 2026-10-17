#!/usr/bin/env python3
"""Launch the retail debug-features experiment (isolated save and run directory).

Usage:
  python tools/debug-features/run.py [--save-template DIR] [--dev-hud] [--debug-menu]
      [--extended-cameras] [--e3-preset] [--debug-overlay] [--config Release|RelWithDebInfo]
      [--capture-display] [--graphics-preset Retail|Default|Max]
      [--graphics RECOMP_MSAA=1 ...]

Starts build/<config>/mm3_recomp.exe (Windows) or build/mm3_recomp (elsewhere)
with out/debug-features/save as the save directory and a new
out/debug-features/run-<stamp> for settings.json, pad.txt, request.txt, captures
and logs. The PID and run path are written to out/debug-features/current-run.json.
Run prepare.py first. Only one instance of this experiment may run.

--graphics accepts only RECOMP_MSAA, RECOMP_ANISO, RECOMP_DRAW_DISTANCE,
RECOMP_RENDER_SCALE, RECOMP_ASPECT, RECOMP_VSYNC, RECOMP_PS_SPEC and
RECOMP_D3D_DEBUG.

"""
import argparse
import datetime
import json
import os
import re
import shutil
import subprocess

GRAPHICS_KEY = re.compile(r"^(RECOMP_(MSAA|ANISO|DRAW_DISTANCE|RENDER_SCALE|ASPECT|VSYNC|PS_SPEC|D3D_DEBUG))$")


def die(msg):
    raise SystemExit(msg)


def game_exe(build_dir, config):
    if os.name == "nt":
        return os.path.join(build_dir, config, "mm3_recomp.exe")
    return os.path.join(build_dir, "mm3_recomp")


def _windows_is_running(exe):
    """True when a mm3_recomp.exe at this path is running (Toolhelp snapshot + QueryFullProcessImageNameW)."""
    import ctypes
    from ctypes import wintypes

    class PROCESSENTRY32W(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
                    ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
                    ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
                    ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
                    ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260)]

    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    k32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    k32.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    k32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    k32.OpenProcess.restype = wintypes.HANDLE
    k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    k32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
                                               ctypes.POINTER(wintypes.DWORD)]
    k32.CloseHandle.argtypes = [wintypes.HANDLE]

    snap = k32.CreateToolhelp32Snapshot(0x2, 0)  # TH32CS_SNAPPROCESS
    if not snap or snap == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    want = os.path.normcase(os.path.abspath(exe))
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = k32.Process32FirstW(snap, ctypes.byref(entry))
        while ok:
            if entry.szExeFile.lower() == "mm3_recomp.exe":
                handle = k32.OpenProcess(0x1000, False, entry.th32ProcessID)  # PROCESS_QUERY_LIMITED_INFORMATION
                if handle:
                    try:
                        size = wintypes.DWORD(32768)
                        buf = ctypes.create_unicode_buffer(size.value)
                        if (k32.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size)) and
                                os.path.normcase(buf.value) == want):
                            return True
                    finally:
                        k32.CloseHandle(handle)
            ok = k32.Process32NextW(snap, ctypes.byref(entry))
        return False
    finally:
        k32.CloseHandle(snap)


def game_is_running(exe):
    if os.name == "nt":
        return _windows_is_running(exe)
    out = subprocess.run(["ps", "-axo", "args="], capture_output=True, text=True).stdout
    return any(line.strip().startswith(exe) for line in out.splitlines())


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--save-template", dest="save_template")
    parser.add_argument("--dev-hud", dest="dev_hud", action="store_true")
    parser.add_argument("--debug-menu", dest="debug_menu", action="store_true")
    parser.add_argument("--extended-cameras", dest="extended_cameras", action="store_true")
    parser.add_argument("--e3-preset", dest="e3_preset", action="store_true")
    parser.add_argument("--debug-overlay", dest="debug_overlay", action="store_true")
    parser.add_argument("--config", choices=["Release", "RelWithDebInfo"], default="Release")
    parser.add_argument("--capture-display", dest="capture_display", action="store_true")
    parser.add_argument("--graphics-preset", dest="graphics_preset", choices=["Retail", "Default", "Max"],
                        default="Default")
    parser.add_argument("--graphics", action="append", default=[], metavar="KEY=VALUE",
                        help="override one graphics setting (repeatable)")
    args = parser.parse_args()

    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    exe = game_exe(os.path.join(repo, "build"), args.config)
    if not os.path.exists(exe):
        die(f"Build the {args.config} target first.")
    if game_is_running(exe):
        die("This experiment is already running.")

    graphics = {}
    for item in args.graphics:
        key, sep, value = item.partition("=")
        if not sep:
            die(f"Graphics override must be KEY=VALUE: {item}")
        graphics[key] = value

    out_dir = os.path.join(repo, "out", "debug-features")
    save = os.path.join(out_dir, "save")
    if args.save_template:
        if os.path.exists(save):
            die("Private save already exists; omit SaveTemplate.")
        shutil.copytree(args.save_template, save)
    os.makedirs(save, exist_ok=True)
    run = os.path.join(out_dir, "run-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
    os.makedirs(run)

    settings = {
        "RECOMP_PB_EXEC": "1", "RECOMP_FB_WINDOW": "1", "RECOMP_USB": "1",
        "RECOMP_KEYBOARD": "1", "RECOMP_MENU_KEYS": "1",
        "RECOMP_APU_DSP_ACK": "0x80458810", "RECOMP_FPS_LIMIT": "60",
        "RECOMP_ASPECT": "16:9", "RECOMP_RENDER_SCALE": "2", "MM3_LOG_UNBUFFERED": "1",
        "RECOMP_PAD_LIVE": os.path.join(run, "pad.txt"), "RECOMP_PAD_SCRIPT": None,
        "RECOMP_PAD_PRESS": None, "MM3_SAVE_DIR": save,
        "MM3_DEBUG_FEATURE_REQUEST": os.path.join(run, "request.txt"),
        "RECOMP_PRESENT_CAPTURE": os.path.join(run, "capture"), "RECOMP_CAPTURE_SURFACE": "1",
        "MM3_DEV_HUD": int(args.dev_hud), "MM3_DEBUG_MENU": int(args.debug_menu),
        "MM3_EXTENDED_CAMERAS": int(args.extended_cameras), "MM3_E3_PRESET": int(args.e3_preset),
        "MM3_DEBUG_OVERLAY": int(args.debug_overlay), "MM3_MOD_DEV_TOOLS": "1",
        "MM3_DEBUG_TELEPORT": "1", "RECOMP_VSYNC": "0", "RECOMP_MSAA": "4", "RECOMP_ANISO": "16",
        "RECOMP_DRAW_DISTANCE": "2", "RECOMP_PS_SPEC": "1",
        "RECOMP_WINDOW_SIZE": "1280x720", "RECOMP_D3D_DEBUG": None,
    }
    if args.capture_display:
        settings["RECOMP_CAPTURE_SURFACE"] = None
    if args.graphics_preset == "Retail":
        settings.update(RECOMP_MSAA="1", RECOMP_ANISO="1", RECOMP_DRAW_DISTANCE="1",
                        RECOMP_RENDER_SCALE="1", RECOMP_ASPECT="4:3")
    elif args.graphics_preset == "Max":
        settings.update(RECOMP_MSAA="8", RECOMP_DRAW_DISTANCE="4", RECOMP_RENDER_SCALE="4")
    for key, value in graphics.items():
        if key not in settings or not GRAPHICS_KEY.match(key):
            die(f"Unsupported graphics override: {key}")
        settings[key] = value

    with open(settings["RECOMP_PAD_LIVE"], "w"):
        pass
    with open(os.path.join(run, "settings.json"), "w", encoding="utf-8") as f:
        json.dump(settings, f, indent=2)

    env = dict(os.environ)
    for key, value in settings.items():
        if value is None:
            env.pop(key, None)
        else:
            env[key] = str(value)
    with open(os.path.join(run, "stdout.log"), "wb") as stdout, \
            open(os.path.join(run, "stderr.log"), "wb") as stderr:
        process = subprocess.Popen([exe], cwd=repo, env=env, stdout=stdout, stderr=stderr)

    with open(os.path.join(out_dir, "current-run.json"), "w", encoding="utf-8") as f:
        json.dump({"pid": process.pid, "executable": exe, "run": run, "save": save}, f)
    print(f"PID={process.pid} Run={run}")


if __name__ == "__main__":
    main()

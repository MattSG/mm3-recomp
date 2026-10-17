#!/usr/bin/env python3
"""Post a key press to a running MM3 recomp framebuffer window.

Usage:
  python tools/scripts/send_framebuffer_key.py --process-id PID --button Start
      [--hold-milliseconds 120]

Buttons: Start, Back, A, B, X, Y, Up, Down, Left, Right. Mapping follows the
keyboard bridge: Enter=Start, Backspace=Back, Z=A, X=B, A=X, S=Y, arrow keys
for the D-pad. Sends WM_KEYDOWN, waits, then WM_KEYUP with PostMessageW to the
process's main top-level window.

Win32 only (ctypes on user32). Elsewhere it exits with "not supported on this OS".

"""
import argparse
import ctypes
import os
import time

VIRTUAL_KEYS = {"Start": 0x0D, "Back": 0x08, "A": 0x5A, "B": 0x58, "X": 0x41, "Y": 0x53,
                "Up": 0x26, "Down": 0x28, "Left": 0x25, "Right": 0x27}
WM_KEYDOWN, WM_KEYUP = 0x0100, 0x0101
GW_OWNER = 4


def die(msg):
    raise SystemExit(msg)


def ranged(lo, hi):
    def parse(text):
        value = int(text)
        if not lo <= value <= hi:
            raise argparse.ArgumentTypeError(f"must be between {lo} and {hi}")
        return value
    return parse


def find_main_window(pid):
    """First visible, unowned top-level window of the process (what Get-Process MainWindowHandle reports)."""
    from ctypes import wintypes
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    proc_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.GetWindowThreadProcessId.restype = wintypes.DWORD
    user32.IsWindowVisible.argtypes = [wintypes.HWND]
    user32.GetWindow.argtypes = [wintypes.HWND, wintypes.UINT]
    user32.GetWindow.restype = wintypes.HWND
    user32.EnumWindows.argtypes = [proc_type, wintypes.LPARAM]
    found = []

    def visit(hwnd, _):
        owner_pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner_pid))
        if (owner_pid.value == pid and user32.IsWindowVisible(hwnd) and
                not user32.GetWindow(hwnd, GW_OWNER)):
            found.append(hwnd)
            return False
        return True

    user32.EnumWindows(proc_type(visit), 0)
    return found[0] if found else None


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--process-id", dest="process_id", type=int, required=True)
    parser.add_argument("--button", choices=list(VIRTUAL_KEYS), required=True)
    parser.add_argument("--hold-milliseconds", dest="hold", type=ranged(30, 2000), default=120,
                        help="30-2000 (default 120)")
    args = parser.parse_args()
    if os.name != "nt":
        die("not supported on this OS: keyboard injection uses Win32 PostMessage")

    vk = VIRTUAL_KEYS[args.button]
    window = find_main_window(args.process_id)
    if not window:
        die(f"Process {args.process_id} has no framebuffer window")
    post = ctypes.WinDLL("user32", use_last_error=True).PostMessageW
    post.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_size_t, ctypes.c_ssize_t]
    post.restype = ctypes.c_int
    if not post(window, WM_KEYDOWN, vk, 0):
        die(f"Could not post {args.button} key-down to process {args.process_id}")
    time.sleep(args.hold / 1000.0)
    if not post(window, WM_KEYUP, vk, 0):
        die(f"Could not post {args.button} key-up to process {args.process_id}")
    print(f"Sent {args.button} to PID {args.process_id} (VK 0x{vk:02X})")


if __name__ == "__main__":
    main()

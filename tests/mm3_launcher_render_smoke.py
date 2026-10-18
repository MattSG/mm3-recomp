"""Exercise the compiled launcher, render a frame, then close it.

Windows: the test checks the launcher's own native frame and visibility,
then sends WM_CLOSE. D3D11 can render hidden; use --visible for SDL3, whose
launcher suspends GPU rendering while hidden.

Other platforms (SDL3 launcher, RmlUi SDL GPU backend): the launcher opens
normally; the test waits for the first-frame log line, then sends SIGTERM,
which the game's console-close handler turns into a normal exit.
"""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import time

FIRST_FRAME = '[launcher] first frame rendered:'
RENDER_TIMEOUT = 15


def run_windows(exe, root, output_dir, visible=False):
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user32.IsWindowVisible.argtypes = [wintypes.HWND]
    user32.GetWindowLongW.argtypes = [wintypes.HWND, ctypes.c_int]
    user32.GetWindowLongW.restype = wintypes.LONG
    env = os.environ.copy()
    env.pop('RECOMP_PB_EXEC', None)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 1 if visible else subprocess.SW_HIDE  # SW_SHOWNORMAL
    stderr_path = output_dir / 'stderr.log'
    with (output_dir / 'stdout.log').open('wb') as stdout, stderr_path.open('wb') as stderr:
        process = subprocess.Popen([str(exe.resolve())], cwd=root, env=env,
                                   startupinfo=startup, stdout=stdout, stderr=stderr)
        try:
            deadline = time.monotonic() + RENDER_TIMEOUT
            window = None
            while time.monotonic() < deadline:
                owned = []

                @callback_type
                def inspect(hwnd, _):
                    pid = wintypes.DWORD()
                    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
                    if pid.value == process.pid:
                        rect = wintypes.RECT()
                        if user32.GetClientRect(hwnd, ctypes.byref(rect)):
                            if rect.right > rect.left and rect.bottom > rect.top:
                                owned.append((hwnd, rect.right - rect.left, rect.bottom - rect.top))
                    return True

                user32.EnumWindows(inspect, 0)
                log = stderr_path.read_text(encoding='utf-8', errors='replace')
                if owned and FIRST_FRAME in log:
                    window = owned[0]
                    break
                if process.poll() is not None:
                    raise AssertionError(f'Launcher exited before rendering: {process.returncode}\n{log}')
                time.sleep(0.05)
            assert window is not None, f'Launcher did not render a frame within 15 s:\n{log}'
            hwnd, width, height = window
            style = user32.GetWindowLongW(hwnd, -16) & 0xffffffff  # GWL_STYLE
            assert style & 0x00c00000 == 0x00c00000, f'Launcher needs a native caption: {style:#010x}'
            assert style & 0x00080000, f'Launcher needs a native system menu: {style:#010x}'
            assert not style & 0x80000000, f'Launcher must not use a popup frame: {style:#010x}'
            assert bool(user32.IsWindowVisible(hwnd)) == visible, 'Launcher visibility differs from requested startup state.'
            assert user32.PostMessageW(hwnd, 0x0010, 0, 0), 'Could not send WM_CLOSE to the owned launcher.'
            assert process.wait(timeout=5) == 0, 'Normal launcher close must exit successfully.'
            log = stderr_path.read_text(encoding='utf-8', errors='replace')
            assert 'CreateTexture2D (' not in log, log
            assert '[CRASH]' not in log and '[UNIMPL]' not in log, log
            print(f'PASS: {"visible" if visible else "hidden"} launcher rendered at {width}x{height} with native style {style:#010x}, then exited normally on WM_CLOSE.')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)


def run_sdl(exe, root, output_dir):
    env = os.environ.copy()
    env.pop('RECOMP_PB_EXEC', None)
    stderr_path = output_dir / 'stderr.log'
    with (output_dir / 'stdout.log').open('wb') as stdout, stderr_path.open('wb') as stderr:
        process = subprocess.Popen([str(exe.resolve())], cwd=root, env=env,
                                   stdout=stdout, stderr=stderr)
        try:
            deadline = time.monotonic() + RENDER_TIMEOUT
            while True:
                log = stderr_path.read_text(encoding='utf-8', errors='replace')
                if FIRST_FRAME in log:
                    break
                if process.poll() is not None:
                    raise AssertionError(f'Launcher exited before rendering: {process.returncode}\n{log}')
                if time.monotonic() > deadline:
                    process.kill()
                    raise AssertionError(f'Launcher did not render a frame within {RENDER_TIMEOUT} s:\n{log}')
                time.sleep(0.05)
            process.send_signal(signal.SIGTERM)
            assert process.wait(timeout=10) == 0, 'SIGTERM (console close) must exit the launcher successfully.'
            log = stderr_path.read_text(encoding='utf-8', errors='replace')
            assert '[CRASH]' not in log and '[UNIMPL]' not in log, log
            size = [line for line in log.splitlines() if FIRST_FRAME in line][0].split(FIRST_FRAME)[1].strip()
            print(f'PASS: launcher rendered a frame at {size}, then exited normally on SIGTERM.')
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    default_exe = 'build/RelWithDebInfo/mm3_recomp.exe' if os.name == 'nt' else 'build/mm3_recomp'
    parser.add_argument('--exe', type=Path, default=root / default_exe)
    parser.add_argument('--output-dir', type=Path, default=root / 'conformance_tmp/launcher-render-smoke')
    parser.add_argument('--visible', action='store_true', help='Render the Windows SDL3 launcher in its visible state.')
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    if os.name == 'nt':
        run_windows(args.exe, root, args.output_dir, visible=args.visible)
    else:
        run_sdl(args.exe, root, args.output_dir)


if __name__ == '__main__':
    main()

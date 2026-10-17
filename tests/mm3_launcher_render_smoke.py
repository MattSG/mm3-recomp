"""Exercise the compiled launcher, render a frame, then close it.

Windows (D3D11 launcher): the window starts hidden; the test finds the
launcher's own window, checks it stayed hidden, then sends WM_CLOSE.

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


def run_windows(exe, root, output_dir):
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user32.IsWindowVisible.argtypes = [wintypes.HWND]
    env = os.environ.copy()
    env.pop('RECOMP_PB_EXEC', None)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
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
            assert not user32.IsWindowVisible(hwnd), 'Hidden-start regression must keep the test window hidden.'
            assert user32.PostMessageW(hwnd, 0x0010, 0, 0), 'Could not send WM_CLOSE to the owned launcher.'
            assert process.wait(timeout=5) == 0, 'Normal launcher close must exit successfully.'
            log = stderr_path.read_text(encoding='utf-8', errors='replace')
            assert 'CreateTexture2D (' not in log, log
            assert '[CRASH]' not in log and '[UNIMPL]' not in log, log
            print(f'PASS: hidden launcher rendered at {width}x{height}, then exited normally on WM_CLOSE.')
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
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    if os.name == 'nt':
        run_windows(args.exe, root, args.output_dir)
    else:
        run_sdl(args.exe, root, args.output_dir)


if __name__ == '__main__':
    main()

"""Exercise the compiled launcher hidden, render a frame, then close its own window."""
import argparse
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument('--exe', type=Path, default=root / 'build/RelWithDebInfo/mm3_recomp.exe')
    parser.add_argument('--output-dir', type=Path, default=root / 'conformance_tmp/launcher-render-smoke')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('The launcher uses the Windows D3D11 backend.')
    args.output_dir.mkdir(parents=True, exist_ok=True)
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
    startup.wShowWindow = subprocess.SW_HIDE
    stderr_path = args.output_dir / 'stderr.log'
    with (args.output_dir / 'stdout.log').open('wb') as stdout, stderr_path.open('wb') as stderr:
        process = subprocess.Popen([str(args.exe.resolve())], cwd=root, env=env,
                                   startupinfo=startup, stdout=stdout, stderr=stderr)
        try:
            deadline = time.monotonic() + 15
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
                if owned and '[launcher] first frame rendered:' in log:
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


if __name__ == '__main__':
    main()

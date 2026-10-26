"""Build Midtown Madness 3 from your own disc in a small desktop window."""
import os
import platform
import queue
import shutil
import subprocess
import sys
import threading
from pathlib import Path

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, scrolledtext, ttk
except ImportError:
    tk = None

from extract_disc import CODE_SHA256, TESTED_IMAGES, Xiso, code_sha256, file_sha1
from setup import REPO as SOURCE_REPO


def graphics_options(system):
    if system == 'Windows':
        return {'Direct3D 11': ('D3D11', None),
                'Direct3D 12': ('SDL3_GPU', 'direct3d12'),
                'Vulkan': ('SDL3_GPU', 'vulkan')}
    if system == 'Linux':
        return {'Vulkan': ('SDL3_GPU', 'vulkan')}
    return {}


def launch_env(driver):
    env = os.environ.copy()
    if getattr(sys, 'frozen', False) and os.name != 'nt':
        if 'LD_LIBRARY_PATH_ORIG' in env:
            env['LD_LIBRARY_PATH'] = env.pop('LD_LIBRARY_PATH_ORIG')
        else:
            env.pop('LD_LIBRARY_PATH', None)
    if driver:
        env['RECOMP_SDL_GPU_DRIVER'] = driver
    return env


def run_command(cmd, emit, cwd):
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    with subprocess.Popen([str(c) for c in cmd], cwd=cwd, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, errors='replace',
                          env=launch_env(None), creationflags=flags) as process:
        for line in process.stdout:
            emit(line)
        if process.wait():
            raise RuntimeError(f'Command failed (exit {process.returncode}). See the build log.')


def check_disc(iso, emit):
    emit('Checking game-code SHA-256 and image SHA-1. This can take a while...\n')
    with Xiso(str(iso)) as disc:
        entry = disc.find('default.xbe')
        if entry is None:
            raise RuntimeError('Disc has no default.xbe in the game partition.')
        digest = code_sha256(disc.read(entry))
        if digest != CODE_SHA256:
            raise RuntimeError(f'Unsupported game code SHA-256: {digest}')
    sha1 = file_sha1(iso)
    emit(f'default.xbe code matches ({digest})\n')
    emit(f'image sha1 {sha1}: ' + TESTED_IMAGES.get(
        sha1, 'not a dump this project was tested with, but its code matches') + '\n')


def run_build(iso, renderer, emit, repo=SOURCE_REPO):
    check_disc(iso, emit)
    existing = repo / 'game_files/default.xbe'
    if existing.is_file():
        if code_sha256(existing.read_bytes()) != CODE_SHA256:
            raise RuntimeError('Existing game files are incompatible. Move game_files aside and retry.')
        emit('Existing game files will be reused.\n')
    host = 'windows' if os.name == 'nt' else 'linux'
    build_dir = f'build/gui-{host}-{renderer.lower()}'
    emit(f'Building in {build_dir}...\n')
    python = (shutil.which('python') if os.name == 'nt' else shutil.which('python3')) if getattr(sys, 'frozen', False) else sys.executable
    if not python:
        raise RuntimeError('Python 3 is required to build the game. Install it and retry.')
    run_command([python, '-u', repo / 'scripts/setup.py', '--iso', iso,
                 '--renderer', renderer, '--build-dir', build_dir], emit, repo)
    return repo / build_dir / ('RelWithDebInfo/mm3_recomp.exe' if os.name == 'nt' else 'mm3_recomp')


class BuildWindow:
    def __init__(self, root):
        self.root = root
        self.options = graphics_options(platform.system())
        self.events = queue.Queue()
        self.busy = False
        self.exe = None
        self.driver = None
        self.unrecognized_image = False
        self.image_sha1 = ''
        self.repo = SOURCE_REPO if not getattr(sys, 'frozen', False) else Path.cwd()
        root.title('Midtown Madness 3')
        root.geometry('620x365')
        root.minsize(580, 365)
        root.protocol('WM_DELETE_WINDOW', self.close)
        style = ttk.Style(root)
        if 'vista' in style.theme_names():
            style.theme_use('vista')
        style.configure('Title.TLabel', font=('Segoe UI', 16, 'bold'))
        style.configure('Hint.TLabel', foreground='#606960')
        frame = ttk.Frame(root, padding=24)
        frame.pack(fill='both', expand=True)
        ttk.Label(frame, text='Midtown Madness 3', style='Title.TLabel').pack(anchor='w')
        ttk.Label(frame, text='Build from your own Xbox disc image', style='Hint.TLabel').pack(anchor='w', pady=(2, 18))
        ttk.Label(frame, text='Disc image').pack(anchor='w')
        row = ttk.Frame(frame)
        row.pack(fill='x', pady=(5, 14))
        self.iso = tk.StringVar()
        self.entry = ttk.Entry(row, textvariable=self.iso)
        self.entry.pack(side='left', fill='x', expand=True, ipady=3)
        self.browse = ttk.Button(row, text='Browse...', command=self.pick)
        self.browse.pack(side='left', padx=(8, 0))
        ttk.Label(frame, text='Graphics').pack(anchor='w')
        self.graphics = tk.StringVar(value=next(iter(self.options), 'Unsupported'))
        self.selector = ttk.Combobox(frame, textvariable=self.graphics, values=list(self.options), state='readonly')
        self.selector.pack(fill='x', pady=(5, 17))
        actions = ttk.Frame(frame)
        actions.pack(fill='x')
        self.check = ttk.Button(actions, text='Check SHA', command=lambda: self.start(check_only=True))
        self.check.pack(side='left', padx=(0, 8))
        self.build = ttk.Button(actions, text='Build', command=self.start)
        self.build.pack(side='left')
        self.play = ttk.Button(actions, text='Launch game', command=self.launch, state='disabled')
        self.play.pack(side='left', padx=8)
        self.details = ttk.Button(actions, text='Show log', command=self.toggle_log)
        self.details.pack(side='right')
        self.status = tk.StringVar(value=f'Ready - {platform.system()} / {platform.machine()}')
        ttk.Label(frame, textvariable=self.status, style='Hint.TLabel', wraplength=480).pack(fill='x', pady=(15, 0))
        self.log = scrolledtext.ScrolledText(frame, height=12, wrap='word', font=('Consolas', 9))
        self.log.configure(state='disabled')
        self.write('Select your own Xbox disc image. The image SHA-1 and game code will be checked before building.\n')
        if not self.options or platform.machine().lower() not in ('amd64', 'x86_64'):
            self.check.configure(state='disabled')
            self.build.configure(state='disabled')
            self.status.set('This computer is not supported. Windows or Linux x86-64 is required.')
        root.after(100, self.poll)

    def write(self, text):
        self.log.configure(state='normal')
        self.log.insert('end', text)
        self.log.see('end')
        self.log.configure(state='disabled')

    def toggle_log(self):
        if self.log.winfo_ismapped():
            self.log.pack_forget()
            self.details.configure(text='Show log')
            self.root.geometry('620x365')
        else:
            self.log.pack(fill='both', expand=True, pady=(12, 0))
            self.details.configure(text='Hide log')
            self.root.geometry('620x600')

    def pick(self):
        path = filedialog.askopenfilename(title='Select your Midtown Madness 3 disc image',
                                          filetypes=[('Disc images', '*.iso *.xiso'), ('All files', '*')])
        if path:
            self.iso.set(path)

    def start(self, check_only=False):
        iso = Path(self.iso.get().strip()).expanduser()
        if not self.iso.get().strip() or not iso.is_file():
            messagebox.showerror('Disc image', 'Choose an existing disc image first.', parent=self.root)
            return
        renderer, driver = self.options[self.graphics.get()]
        if not check_only and not (self.repo / 'scripts/setup.py').is_file():
            chosen = filedialog.askdirectory(title='Select your mm3-recomp checkout')
            if not chosen:
                return
            self.repo = Path(chosen)
            if not (self.repo / 'scripts/setup.py').is_file():
                messagebox.showerror('Checkout', 'Select the mm3-recomp repository folder.', parent=self.root)
                return
        self.busy = True
        self.exe = None
        self.unrecognized_image = False
        self.image_sha1 = ''
        self.check.configure(state='disabled')
        self.build.configure(state='disabled')
        self.play.configure(state='disabled')
        self.browse.configure(state='disabled')
        self.entry.configure(state='disabled')
        self.selector.configure(state='disabled')
        self.status.set('Checking disc...' if check_only else 'Checking disc and building...')
        def worker():
            try:
                emit = lambda line: self.events.put(('log', line))
                if check_only:
                    check_disc(iso.resolve(), emit)
                    self.events.put(('checked', None))
                else:
                    exe = run_build(iso.resolve(), renderer, emit, self.repo)
                    self.events.put(('done', (exe, driver)))
            except Exception as error:
                self.events.put(('error', str(error)))
        threading.Thread(target=worker, daemon=True).start()

    def poll(self):
        while True:
            try:
                kind, value = self.events.get_nowait()
            except queue.Empty:
                break
            if kind == 'log':
                if value.startswith('image sha1 '):
                    self.image_sha1 = value.split()[2].rstrip(':')
                    self.unrecognized_image = 'not a dump this project was tested with' in value
                self.write(value)
                continue
            self.busy = False
            self.check.configure(state='normal')
            self.build.configure(state='normal')
            self.browse.configure(state='normal')
            self.entry.configure(state='normal')
            self.selector.configure(state='readonly')
            if kind == 'checked':
                result = 'not in tested list' if self.unrecognized_image else 'matches tested image'
                self.status.set(f'SHA-1: {self.image_sha1} ({result}). Game code matches.')
            elif kind == 'done':
                self.exe, self.driver = value
                self.write(f'\nBuild complete: {self.exe}\n')
                self.status.set('Build complete. Disc code matches; image SHA-1 is not in the tested list.'
                                if self.unrecognized_image else 'Build complete. Ready to launch.')
                self.play.configure(state='normal')
            else:
                self.write(f'\nBuild stopped: {value}\n')
                self.status.set('Check or build failed. See the log for details.')
        self.root.after(100, self.poll)

    def launch(self):
        try:
            subprocess.Popen([str(self.exe)], cwd=self.repo, env=launch_env(self.driver))
        except OSError as error:
            messagebox.showerror('Could not launch', str(error), parent=self.root)

    def close(self):
        if self.busy:
            messagebox.showinfo('Build in progress', 'Please wait for the build to finish before closing.', parent=self.root)
        else:
            self.root.destroy()


def main():
    if getattr(sys, 'frozen', False) and os.name == 'nt':
        import ctypes
        ctypes.windll.kernel32.SetDllDirectoryW(None)
    if tk is None:
        sys.exit('Tkinter is required. On Ubuntu, install python3-tk.')
    root = tk.Tk()
    BuildWindow(root)
    root.mainloop()


if __name__ == '__main__':
    if '--self-test' in sys.argv:
        if tk is None or Xiso is None:
            sys.exit(1)
        if os.environ.get('MM3_TEST_ISO'):
            check_disc(Path(os.environ['MM3_TEST_ISO']), lambda line: None)
        sys.exit(0)
    main()

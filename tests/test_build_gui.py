import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import build_gui
import extract_disc


class BuilderTests(unittest.TestCase):
    def test_graphics_choices_match_host_and_launch_driver(self):
        windows = build_gui.graphics_options('Windows')
        self.assertEqual(windows['Direct3D 11'], ('D3D11', None))
        self.assertEqual(windows['Direct3D 12'], ('SDL3_GPU', 'direct3d12'))
        self.assertEqual(windows['Vulkan'], ('SDL3_GPU', 'vulkan'))
        self.assertEqual(build_gui.graphics_options('Linux'), {'Vulkan': ('SDL3_GPU', 'vulkan')})
        self.assertEqual(build_gui.launch_env('vulkan')['RECOMP_SDL_GPU_DRIVER'], 'vulkan')

    def test_packaged_builder_uses_checkout_and_external_python(self):
        with tempfile.TemporaryDirectory() as temp:
            repo = Path(temp)
            iso = repo / 'disc.iso'
            with patch.object(build_gui.sys, 'frozen', True, create=True), patch.object(build_gui.shutil, 'which', return_value='external-python'), patch.object(build_gui, 'check_disc'), patch.object(build_gui, 'run_command') as run:
                exe = build_gui.run_build(iso, 'SDL3_GPU', lambda line: None, repo)
            args = run.call_args.args
            self.assertEqual(args[0][0], 'external-python')
            self.assertEqual(args[0][2], repo / 'scripts/setup.py')
            self.assertEqual(args[2], repo)
            self.assertTrue(str(exe).startswith(str(repo)))

    def test_window_handles_build_success_and_failure(self):
        if build_gui.tk is None:
            self.skipTest('Tkinter is unavailable')
        try:
            root = build_gui.tk.Tk()
        except build_gui.tk.TclError:
            self.skipTest('No display available')
        root.withdraw()
        self.addCleanup(root.destroy)
        with tempfile.TemporaryDirectory() as temp:
            iso = Path(temp) / 'disc.iso'
            iso.touch()
            window = build_gui.BuildWindow(root)
            window.iso.set(str(iso))
            def valid_disc(path, emit):
                emit('image sha1 8ed003adaf504883afc980d2888df6125496d044: tested image\n')
            with patch.object(build_gui, 'check_disc', side_effect=valid_disc), patch.object(build_gui, 'run_build') as build:
                window.start(check_only=True)
                deadline = time.monotonic() + 3
                while window.busy and time.monotonic() < deadline:
                    root.update()
                    time.sleep(0.01)
                self.assertFalse(window.busy)
                self.assertIn('8ed003adaf504883afc980d2888df6125496d044', window.status.get())
                self.assertIn('matches tested image', window.status.get())
                build.assert_not_called()
            for outcome in [RuntimeError('bad disc'), Path(temp) / 'game.exe']:
                with patch.object(build_gui, 'run_build', side_effect=outcome if isinstance(outcome, Exception) else None, return_value=outcome):
                    window.start()
                    deadline = time.monotonic() + 3
                    while window.busy and time.monotonic() < deadline:
                        root.update()
                        time.sleep(0.01)
                    self.assertFalse(window.busy)
                    self.assertEqual(str(window.play['state']), 'disabled' if isinstance(outcome, Exception) else 'normal')
                    self.assertIn('failed' if isinstance(outcome, Exception) else 'complete', window.status.get().lower())
            window.events.put(('log', 'image sha1 abc: not a dump this project was tested with, but its code matches'))
            window.events.put(('done', (Path(temp) / 'game.exe', None)))
            window.poll()
            self.assertIn('not in the tested list', window.status.get())

    def test_extraction_rejects_escape_before_writing(self):
        class Entry:
            name = '../escape'
        class Disc:
            def __init__(self, path): pass
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def find(self, name): return Entry()
            def read(self, entry): return b'xbe'
            def walk(self): return [('', Entry())]
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / 'output'
            with patch.object(extract_disc, 'Xiso', Disc), patch.object(extract_disc, 'code_sha256', return_value=extract_disc.CODE_SHA256), patch.object(sys, 'argv', ['extract', 'dummy', '--out', str(out), '--skip-iso-hash']):
                with self.assertRaisesRegex(SystemExit, 'unsafe disc path'):
                    extract_disc.main()
            self.assertFalse(out.exists())

    def test_existing_symlink_cannot_escape_output(self):
        class Entry:
            name = 'file'
        class Disc:
            def __init__(self, path): pass
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def find(self, name): return Entry()
            def read(self, entry): return b'xbe'
            def walk(self): return [('link', Entry())]
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / 'output'
            out.mkdir()
            try:
                (out / 'link').symlink_to(Path(temp), target_is_directory=True)
            except OSError:
                self.skipTest('creating symlinks requires privileges')
            with patch.object(extract_disc, 'Xiso', Disc), patch.object(extract_disc, 'code_sha256', return_value=extract_disc.CODE_SHA256), patch.object(sys, 'argv', ['extract', 'dummy', '--out', str(out), '--skip-iso-hash']):
                with self.assertRaisesRegex(SystemExit, 'escapes output'):
                    extract_disc.main()


if __name__ == '__main__':
    unittest.main()

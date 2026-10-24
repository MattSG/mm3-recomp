import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]


class BenchmarkRunnerTests(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt', 'Windows benchmark runner')
    def test_runner_launch_failure_isolates_save(self):
        tag = 'contract-' + uuid.uuid4().hex
        run_dir = ROOT / 'conformance_tmp/bench' / tag
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            save = folder / 'save'
            save.mkdir()
            (save / 'keep.txt').write_text('original save')
            # A missing executable makes the launch fail before the game runs.
            result = subprocess.run([sys.executable, str(ROOT / 'tools/bench/run.py'), '--exe',
                                     str(folder / 'missing.exe'), '--save', str(save), '--tag', tag],
                                    text=True, capture_output=True)
            try:
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((run_dir / 'save').exists(), 'disposable save remains')
                self.assertEqual((save / 'keep.txt').read_text(), 'original save')
            finally:
                shutil.rmtree(run_dir, ignore_errors=True)

    @unittest.skipUnless(os.name == 'nt', 'Windows benchmark runner')
    def test_runner_rejects_recursive_save_copy(self):
        tag = 'contract-' + uuid.uuid4().hex
        result = subprocess.run([sys.executable, str(ROOT / 'tools/bench/run.py'), '--exe',
                                 os.environ['ComSpec'], '--save', str(ROOT), '--tag', tag],
                                text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('cannot be inside its save template', result.stderr)
        self.assertFalse((ROOT / 'conformance_tmp/bench' / tag).exists())


if __name__ == '__main__':
    unittest.main()

import contextlib
import csv
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('bench_summary', ROOT / 'tools/bench/summarize.py')
BENCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCH)


class BenchmarkSummaryTests(unittest.TestCase):
    def make_run(self, folder, present=True):
        (folder / 'windows.json').write_text(json.dumps([
            {'name': 'race', 'start': 100, 'end': 500, 'cpu_ms': 320, 'logical_processors': 16}
        ]), encoding='utf-8-sig')
        with (folder / 'frames.csv').open('w', newline='') as stream:
            writer = csv.writer(stream)
            writer.writerow(['qpc_ms', 'frame_ms', 'present_qpc_ms', 'present_ms'])
            # This straddling interval must not count as a hitch within the window.
            writer.writerow([110, 100, 110 if present else '', 100 if present else ''])
            for timestamp in range(130, 531, 20):
                writer.writerow([timestamp, 20, timestamp - 2 if present else '', 20 if present else ''])

    def test_separate_host_metrics_and_interval_boundaries(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            self.make_run(folder)
            with contextlib.redirect_stdout(io.StringIO()):
                results = BENCH.summarize_run(folder)
            self.assertEqual(results[0]['frame_ms']['frames'], 19)
            self.assertEqual(results[0]['present_ms']['frames'], 19)
            self.assertEqual(results[0]['frame_ms']['fps'], 50)
            self.assertEqual(results[0]['frame_ms']['hitch'], 0)
            self.assertEqual(results[0]['process_cpu_percent'], 5)
            self.assertEqual(json.loads((folder / 'summary.json').read_text()), results)

    def test_missing_host_samples_rejects_run(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            self.make_run(folder, present=False)
            with contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(ValueError, 'insufficient present_ms'):
                BENCH.summarize_run(folder)
            self.assertFalse((folder / 'summary.json').exists())

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

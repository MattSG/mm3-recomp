import contextlib
import csv
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
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
    def test_runner_launch_failure_restores_environment_and_isolates_save(self):
        pwsh = ROOT / 'tools/bin/powershell/pwsh.exe'
        if not pwsh.exists():
            self.skipTest('Checkout PowerShell is unavailable')
        tag = 'contract-' + uuid.uuid4().hex
        run_dir = ROOT / 'conformance_tmp/bench' / tag
        def quote(value):
            return "'" + str(value).replace("'", "''") + "'"
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            save = folder / 'save'
            save.mkdir()
            (save / 'keep.txt').write_text('original save')
            script = folder / 'test.ps1'
            script.write_text(f'''
$ErrorActionPreference = 'Stop'
$env:RECOMP_MSAA = '7'
$env:RECOMP_PAD_SCRIPT = 'inherited-control'
function Start-Process {{ throw 'injected launch failure' }}
try {{
    try {{
        & {quote(ROOT / 'tools/bench/run.ps1')} -Exe $env:ComSpec -Save {quote(save)} -Tag {quote(tag)}
        throw 'runner unexpectedly succeeded'
    }} catch {{
        if ($_.Exception.Message -ne 'injected launch failure') {{ throw }}
    }}
    if ($env:RECOMP_MSAA -ne '7' -or $env:RECOMP_PAD_SCRIPT -ne 'inherited-control') {{ throw 'environment not restored' }}
    if (Test-Path -LiteralPath {quote(run_dir / 'save')}) {{ throw 'disposable save remains' }}
    if ((Get-Content -LiteralPath {quote(save / 'keep.txt')} -Raw) -ne 'original save') {{ throw 'original save changed' }}
}} finally {{
    $target = [IO.Path]::GetFullPath({quote(run_dir)})
    $boundary = [IO.Path]::GetFullPath({quote(ROOT / 'conformance_tmp/bench')}) + [IO.Path]::DirectorySeparatorChar
    if (-not $target.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)) {{ throw 'cleanup escaped boundary' }}
    Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue
}}
''', encoding='utf-8')
            subprocess.run([str(pwsh), '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script)], check=True)
            self.assertEqual((save / 'keep.txt').read_text(), 'original save')

    @unittest.skipUnless(os.name == 'nt', 'Windows benchmark runner')
    def test_runner_rejects_recursive_save_copy(self):
        pwsh = ROOT / 'tools/bin/powershell/pwsh.exe'
        if not pwsh.exists():
            self.skipTest('Checkout PowerShell is unavailable')
        tag = 'contract-' + uuid.uuid4().hex
        result = subprocess.run([str(pwsh), '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                 str(ROOT / 'tools/bench/run.ps1'), '-Exe', os.environ['ComSpec'],
                                 '-Save', str(ROOT), '-Tag', tag], text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('cannot be inside its save template', result.stderr)
        self.assertFalse((ROOT / 'conformance_tmp/bench' / tag).exists())


if __name__ == '__main__':
    unittest.main()

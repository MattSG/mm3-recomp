"""Exercise the real launcher with a harmless temporary script in place of MM3."""
import json
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
SHELL = shutil.which("pwsh") or shutil.which("powershell")
KEYS = ("MM3_PERFORMANCE_PROFILE", "RECOMP_RENDER_SCALE", "RECOMP_MSAA",
        "RECOMP_DRAW_DISTANCE", "RECOMP_ANISO", "RECOMP_FPS_LIMIT",
        "RECOMP_PB_EXEC", "RECOMP_FB_WINDOW", "RECOMP_USB", "RECOMP_APU_DSP_ACK",
        "MM3_FOV_DEGREES", "MM3_BUILDING_TRACE", "RECOMP_TEXTURE_STREAM_TRACE",
        "RECOMP_ASYNC_WAIT_TRACE")


def quoted(path):
    return "'" + str(path).replace("'", "''") + "'"


@unittest.skipUnless(SHELL, "PowerShell is unavailable")
class LauncherProfileTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="mm3 launch profile ")
        self.folder = Path(self.temporary.name)
        (self.folder / "scripts").mkdir()
        (self.folder / "game_files").mkdir()
        (self.folder / "game_files/default.xbe").write_bytes(b"fixture marker only")
        self.launcher = self.folder / "scripts/play.ps1"
        self.launcher.write_bytes((ROOT / "scripts/play.ps1").read_bytes())
        self.collector = self.folder / "collect settings.ps1"
        self.collected = self.folder / "settings.jsonl"
        names = ",".join(quoted(key) for key in KEYS)
        self.snapshot = ("$settings = [ordered]@{}; foreach ($key in @(" + names + ")) { "
                         "$settings[$key] = [Environment]::GetEnvironmentVariable($key, 'Process') }; ")
        self.collector.write_text(self.snapshot + "$json = $settings | ConvertTo-Json -Compress; "
                                  "Add-Content -LiteralPath " + quoted(self.collected) +
                                  " -Value $json; [Console]::Out.WriteLine($json)", encoding="utf-8")
        self.environment = {key: value for key, value in os.environ.items()
                            if not key.startswith(("MM3_", "RECOMP_"))}

    def tearDown(self):
        self.temporary.cleanup()

    def run_launcher(self, launches, overrides=None, expect_failure=False, culture=None):
        self.environment.update(overrides or {})
        script = "$ErrorActionPreference = 'Stop'; $initial = (Get-Location).Path; $during = @(); "
        if culture:
            script += "[Threading.Thread]::CurrentThread.CurrentCulture = " + quoted(culture) + "; "
        for arguments in launches:
            invoke = "& " + quoted(self.launcher) + " -Exe " + quoted(self.collector) + " " + arguments
            script += "try { " + invoke + " } catch {} ; " if expect_failure else invoke + "; "
        script += ("if (Test-Path -LiteralPath " + quoted(self.collected) + ") { "
                   "$during = @(Get-Content -LiteralPath " + quoted(self.collected) +
                   " | ForEach-Object { $_ | ConvertFrom-Json }) }; ")
        script += self.snapshot
        script += ("[ordered]@{ during = $during; after = $settings; "
                   "location_restored = ((Get-Location).Path -eq $initial) } | ConvertTo-Json -Depth 5 -Compress")
        result = subprocess.run([SHELL, "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script],
                                text=True, capture_output=True, env=self.environment, cwd=self.folder)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout.splitlines()[-1])

    def test_profile_selection_explicit_fps_and_repeated_launches_restore_environment(self):
        result = self.run_launcher(["-Profile Quality", "-Profile LowPower", "", "-FpsLimit 45"])
        self.assertEqual([row["MM3_PERFORMANCE_PROFILE"] for row in result["during"]],
                         ["Quality", "LowPower", "Balanced", "Balanced"])
        self.assertEqual([row["RECOMP_FPS_LIMIT"] for row in result["during"]], [None, None, None, "45"])
        for row in result["during"]:
            # Graphics/FPS profile values are chosen by the shared C helper.
            self.assertIsNone(row["RECOMP_RENDER_SCALE"])
            self.assertEqual(row["RECOMP_PB_EXEC"], "1")
            self.assertEqual(row["RECOMP_APU_DSP_ACK"], "0x80458810")
            self.assertEqual(row["MM3_BUILDING_TRACE"], "1")
            self.assertEqual(row["RECOMP_TEXTURE_STREAM_TRACE"], "1")
            self.assertEqual(row["RECOMP_ASYNC_WAIT_TRACE"], "1")
        self.assertTrue(result["location_restored"])
        self.assertTrue(all(value is None for value in result["after"].values()))

    def test_individual_environment_overrides_take_priority_and_survive_launch(self):
        overrides = {"MM3_PERFORMANCE_PROFILE": "Quality", "RECOMP_RENDER_SCALE": "2.25",
                     "RECOMP_MSAA": "8", "RECOMP_DRAW_DISTANCE": "1.25", "RECOMP_ANISO": "4",
                     "RECOMP_FPS_LIMIT": "47", "RECOMP_USB": "0", "MM3_FOV_DEGREES": "97"}
        result = self.run_launcher(["-Profile LowPower -FpsLimit 30 -Fov 100"], overrides)
        for key, value in overrides.items():
            self.assertEqual(result["during"][0][key], value)
            self.assertEqual(result["after"][key], value)
        self.assertTrue(result["location_restored"])

    def test_failed_entry_point_still_restores_added_settings_and_working_directory(self):
        self.collector.write_text("throw 'intentional fixture failure'", encoding="utf-8")
        result = self.run_launcher(["-Profile LowPower -FpsLimit 42 -Fov 105"], expect_failure=True)
        self.assertTrue(result["location_restored"])
        self.assertTrue(all(value is None for value in result["after"].values()))

    def test_fov_boundaries_decimal_and_unset_restore_environment(self):
        result = self.run_launcher(["-Fov 60", "-Fov 110", "-Fov 100.5", ""], culture="fr-FR")
        self.assertEqual([row["MM3_FOV_DEGREES"] for row in result["during"]],
                         ["60", "110", "100.5", None])
        self.assertTrue(result["location_restored"])
        self.assertTrue(all(value is None for value in result["after"].values()))

    def test_out_of_range_fov_never_runs_entry_point(self):
        marker = self.folder / "entry-point-was-run"
        self.collector.write_text("Set-Content -LiteralPath " + quoted(marker) + " -Value 'unexpected'",
                                  encoding="utf-8")
        result = self.run_launcher(["-Fov 59", "-Fov 111"], expect_failure=True)
        self.assertFalse(marker.exists())
        self.assertTrue(result["location_restored"])
        self.assertTrue(all(value is None for value in result["after"].values()))

    def test_trace_switch_records_launch_configuration_and_respects_overrides(self):
        overrides = {"MM3_BUILDING_TRACE": "0", "RECOMP_DRAW_DISTANCE": "1.25",
                     "MM3_FOV_DEGREES": "97"}
        result = self.run_launcher(["-TraceBuildings -Profile LowPower -Fov 100", "-NoTraceBuildings"], overrides)
        first, second = result["during"]
        self.assertEqual(first["MM3_BUILDING_TRACE"], "0")
        self.assertEqual(first["RECOMP_TEXTURE_STREAM_TRACE"], "1")
        self.assertEqual(first["RECOMP_ASYNC_WAIT_TRACE"], "1")
        self.assertIsNone(second["RECOMP_TEXTURE_STREAM_TRACE"])
        self.assertIsNone(second["RECOMP_ASYNC_WAIT_TRACE"])
        self.assertEqual(result["after"]["MM3_BUILDING_TRACE"], "0")
        manifests = sorted((self.folder / "logs").glob("play-*.json"))
        self.assertEqual(len(manifests), 2)
        metadata = json.loads(manifests[0].read_text(encoding="utf-8"))
        self.assertEqual(metadata["state"], "exited")
        self.assertEqual(metadata["exit_code"], 0)
        self.assertEqual(metadata["environment"]["RECOMP_DRAW_DISTANCE"], "1.25")
        self.assertEqual(metadata["environment"]["MM3_FOV_DEGREES"], "97")
        self.assertEqual(metadata["profile_defaults_for_unset_settings"]["draw_distance"], "1")
        self.assertTrue(metadata["launch_options"]["trace_buildings"])
        self.assertEqual(metadata["executable"]["sha256"], hashlib.sha256(self.collector.read_bytes()).hexdigest())
        self.assertEqual(metadata["arguments"][-1], str(self.collector))
        files = {entry["path"]: entry for entry in metadata["configuration_files"]}
        xbe = self.folder / "game_files/default.xbe"
        self.assertEqual(files[str(xbe)]["sha256"], hashlib.sha256(xbe.read_bytes()).hexdigest())
        for manifest in manifests:
            saved = json.loads(manifest.read_text(encoding="utf-8"))
            log = Path(saved["log_path"]).read_text(encoding="utf-8")
            self.assertRegex(log, r"^\d{4}-\d\d-\d\dT.*\[stdout\] ")
            self.assertIn('"MM3_PERFORMANCE_PROFILE"', log)

    @unittest.skipUnless(os.name == "nt", "requires a harmless Windows native process fixture")
    def test_second_game_is_refused_before_launch_or_log_creation(self):
        # A renamed native ping process exercises the real process-name guard
        # independently of any game the user happens to be playing. Keep its
        # Popen handle so cleanup can only terminate this test-owned process.
        fixture = self.folder / "mm3_recomp.exe"
        ping = Path(os.environ["SystemRoot"]) / "System32/ping.exe"
        shutil.copyfile(ping, fixture)
        process = subprocess.Popen([str(fixture), "-t", "127.0.0.1"],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            script = ("$ErrorActionPreference = 'Stop'; $initial = (Get-Location).Path; "
                      "$fixture = Get-Process -Id " + str(process.pid) + "; "
                      "if ($fixture.ProcessName -ne 'mm3_recomp') { throw 'invalid native fixture' }; "
                      "$refusal = $null; try { & " + quoted(self.launcher) +
                      " -Exe " + quoted(fixture) +
                      " } catch { $refusal = $_.Exception.Message }; ")
            script += self.snapshot
            script += ("[ordered]@{ refusal = $refusal; after = $settings; "
                       "location_restored = ((Get-Location).Path -eq $initial) } "
                       "| ConvertTo-Json -Depth 5 -Compress")
            result = subprocess.run([SHELL, "-NoProfile", "-ExecutionPolicy", "Bypass",
                                     "-Command", script], text=True, capture_output=True,
                                    env=self.environment, cwd=self.folder, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            response = json.loads(result.stdout.splitlines()[-1])
            self.assertIn("already running", response["refusal"])
            self.assertIn(str(process.pid), response["refusal"])
            self.assertIn("Close all existing game windows", response["refusal"])
            self.assertIn("cache", response["refusal"])
            self.assertFalse((self.folder / "logs").exists())
            self.assertTrue(response["location_restored"])
            self.assertTrue(all(value is None for value in response["after"].values()))
            self.assertIsNone(process.poll(), "preflight disturbed the existing native process")
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=10)

    def test_both_streams_are_saved_before_exit_and_nonzero_exit_code_survives(self):
        release = self.folder / "release child"
        self.collector.write_text(
            "[Console]::Out.WriteLine('stdout before hold'); "
            "[Console]::Error.WriteLine('stderr before hold'); "
            "while (-not (Test-Path -LiteralPath " + quoted(release) + ")) { Start-Sleep -Milliseconds 20 }; "
            "for ($i = 0; $i -lt 2500; ++$i) { [Console]::Out.WriteLine(('output-{0} ' -f $i) + ('x' * 128)) }; "
            "for ($i = 0; $i -lt 2500; ++$i) { [Console]::Error.WriteLine(('error-{0} ' -f $i) + ('y' * 128)) }; "
            "[Console]::Error.Write('final stderr without newline'); exit 37", encoding="utf-8")
        # Redirect the outer shell to files as well, so its console echo cannot
        # fill a pipe while the test deliberately leaves the child running.
        with (self.folder / "outer.out").open("w", encoding="utf-8") as output, \
                (self.folder / "outer.err").open("w", encoding="utf-8") as error:
            process = subprocess.Popen([SHELL, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                                        str(self.launcher), "-Exe", str(self.collector)],
                                       stdout=output, stderr=error, env=self.environment, cwd=self.folder)
            try:
                deadline = time.monotonic() + 20
                observed = ""
                while time.monotonic() < deadline:
                    logs = list((self.folder / "logs").glob("play-*.log"))
                    if logs:
                        observed = logs[0].read_text(encoding="utf-8")
                    if "stdout before hold" in observed and "stderr before hold" in observed:
                        break
                    if process.poll() is not None:
                        self.fail("launcher exited before the live capture: " +
                                  (self.folder / "outer.err").read_text(encoding="utf-8"))
                    time.sleep(.05)
                self.assertIn("[stdout] stdout before hold", observed)
                self.assertIn("[stderr] stderr before hold", observed)
                self.assertIsNone(process.poll())
                release.touch()
                self.assertEqual(process.wait(timeout=30), 37)
            finally:
                release.touch()
                if process.poll() is None:
                    process.wait(timeout=30)
        metadata = json.loads(next((self.folder / "logs").glob("play-*.json")).read_text(encoding="utf-8"))
        self.assertEqual(metadata["exit_code"], 37)
        self.assertEqual(metadata["state"], "exited")
        self.assertNotIn("capture_error", metadata)
        log = Path(metadata["log_path"]).read_text(encoding="utf-8")
        self.assertEqual(log.count("[stdout] output-"), 2500)
        self.assertEqual(log.count("[stderr] error-"), 2500)
        self.assertTrue(log.endswith("[stderr] final stderr without newline\n"))


if __name__ == "__main__":
    unittest.main()

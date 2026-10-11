"""Exercise the real launcher with a harmless temporary script in place of MM3."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SHELL = shutil.which("pwsh") or shutil.which("powershell")
KEYS = ("MM3_PERFORMANCE_PROFILE", "RECOMP_RENDER_SCALE", "RECOMP_MSAA",
        "RECOMP_DRAW_DISTANCE", "RECOMP_ANISO", "RECOMP_FPS_LIMIT",
        "RECOMP_PB_EXEC", "RECOMP_FB_WINDOW", "RECOMP_USB", "RECOMP_APU_DSP_ACK",
        "MM3_FOV_DEGREES")


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
        names = ",".join(quoted(key) for key in KEYS)
        self.snapshot = ("$settings = [ordered]@{}; foreach ($key in @(" + names + ")) { "
                         "$settings[$key] = [Environment]::GetEnvironmentVariable($key, 'Process') }; ")
        self.collector.write_text(self.snapshot + "$settings | ConvertTo-Json -Compress", encoding="utf-8")
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
            script += ("try { " + invoke + " } catch {} ; " if expect_failure else
                       "$during += (" + invoke + " | ConvertFrom-Json); ")
        script += self.snapshot
        script += ("[ordered]@{ during = $during; after = $settings; "
                   "location_restored = ((Get-Location).Path -eq $initial) } | ConvertTo-Json -Depth 5 -Compress")
        result = subprocess.run([SHELL, "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script],
                                text=True, capture_output=True, env=self.environment, cwd=self.folder)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

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


if __name__ == "__main__":
    unittest.main()

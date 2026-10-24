"""Exercise scripts/play.py with harmless Python fixtures standing in for the game."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME_NAME = "mm3_recomp.exe" if os.name == "nt" else "mm3_recomp"

# Records every MM3_/RECOMP_ variable the fixture sees. Console keys go to the
# private jsonl file only, never to stdout.
COLLECTOR = '''
import json, os
seen = {k: v for k, v in os.environ.items() if k.startswith(("MM3_", "RECOMP_"))}
with open(os.environ["PLAY_TEST_COLLECTED"], "a", encoding="utf-8") as handle:
    handle.write(json.dumps(seen) + "\\n")
print(json.dumps({k: v for k, v in seen.items() if "XBOX" not in k}), flush=True)
'''

# Stands in for a running game: signals readiness, then waits for a release file.
GAME_SCRIPT = '''
import os, time
open(os.environ["MM3_FIXTURE_READY"], "w").close()
while not os.path.exists(os.environ["MM3_FIXTURE_RELEASE"]):
    time.sleep(0.02)
'''

HOLD_COLLECTOR = '''
import os, sys, time
print("stdout before hold", flush=True)
print("stderr before hold", file=sys.stderr, flush=True)
while not os.path.exists(os.environ["PLAY_TEST_RELEASE"]):
    time.sleep(0.02)
for i in range(2500):
    print("output-%d " % i + "x" * 128)
for i in range(2500):
    print("error-%d " % i + "y" * 128, file=sys.stderr)
sys.stderr.write("final stderr without newline")
sys.stderr.flush()
sys.exit(37)
'''


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class PlayLauncherTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="mm3 launch profile ")
        self.folder = Path(self.temporary.name)
        (self.folder / "scripts").mkdir()
        (self.folder / "game_files").mkdir()
        (self.folder / "game_files/default.xbe").write_bytes(b"fixture marker only")
        self.launcher = self.folder / "scripts/play.py"
        shutil.copyfile(ROOT / "scripts/play.py", self.launcher)
        support = self.folder / "tools/xboxrecomp/tools/launch"
        support.mkdir(parents=True)
        shutil.copyfile(ROOT / "tools/xboxrecomp/tools/launch/process_support.py", support / "process_support.py")
        self.collector = self.folder / "collect settings.py"
        self.collector.write_text(COLLECTOR, encoding="utf-8")
        self.collected = self.folder / "settings.jsonl"
        self.environment = {key: value for key, value in os.environ.items()
                            if not key.startswith(("MM3_", "RECOMP_"))}
        self.environment["PLAY_TEST_COLLECTED"] = str(self.collected)
        self.parent_environment = dict(os.environ)

    def tearDown(self):
        self.temporary.cleanup()

    def launch(self, arguments=(), overrides=None, exe=None):
        environment = dict(self.environment, **(overrides or {}))
        command = [sys.executable, str(self.launcher), "--exe", str(exe or self.collector), *arguments]
        result = subprocess.run(command, text=True, encoding="utf-8", errors="replace",
                                capture_output=True, env=environment, cwd=self.folder, timeout=120)
        # The launcher is a subprocess: the caller's own environment must be untouched.
        self.assertEqual(dict(os.environ), self.parent_environment)
        return result

    def launch_ok(self, arguments=(), overrides=None):
        result = self.launch(arguments, overrides)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result

    def records(self):
        if not self.collected.exists():
            return []
        return [json.loads(line) for line in self.collected.read_text(encoding="utf-8").splitlines()]

    def manifests(self):
        return sorted((self.folder / "logs").glob("play-*.json"))

    @staticmethod
    def metadata(path):
        return json.loads(Path(path).read_text(encoding="utf-8"))

    def test_profile_selection_explicit_fps_and_repeated_launches(self):
        for arguments in (["--profile", "Quality"], ["--profile", "LowPower"], [], ["--fps-limit", "45"]):
            self.launch_ok(arguments)
        during = self.records()
        self.assertEqual([row.get("MM3_PERFORMANCE_PROFILE") for row in during],
                         ["Quality", "LowPower", "Balanced", "Balanced"])
        self.assertEqual([row.get("RECOMP_FPS_LIMIT") for row in during], [None, None, None, "45"])
        for row in during:
            # Graphics/FPS profile values are chosen by the shared C helper.
            self.assertIsNone(row.get("RECOMP_RENDER_SCALE"))
            self.assertEqual(row["RECOMP_PB_EXEC"], "1")
            self.assertEqual(row["RECOMP_APU_DSP_ACK"], "0x80458810")
            self.assertIsNone(row.get("MM3_BUILDING_TRACE"))
            self.assertIsNone(row.get("RECOMP_TEXTURE_STREAM_TRACE"))
            self.assertIsNone(row.get("RECOMP_ASYNC_WAIT_TRACE"))
        metadata = [self.metadata(path) for path in self.manifests()]
        self.assertEqual([row["profile_defaults_for_unset_settings"]["msaa"] for row in metadata],
                         ["8", "1", "4", "4"])

    def test_individual_environment_overrides_take_priority_and_survive_launch(self):
        overrides = {"MM3_PERFORMANCE_PROFILE": "Quality", "RECOMP_RENDER_SCALE": "2.25",
                     "RECOMP_MSAA": "8", "RECOMP_DRAW_DISTANCE": "1.25", "RECOMP_ANISO": "4",
                     "RECOMP_FPS_LIMIT": "47", "RECOMP_USB": "0", "MM3_FOV_DEGREES": "97"}
        self.launch_ok(["--profile", "LowPower", "--fps-limit", "30", "--fov", "100"], overrides)
        for key, value in overrides.items():
            self.assertEqual(self.records()[0][key], value)

    def test_failing_entry_point_is_reported_and_recorded(self):
        self.collector.write_text("raise RuntimeError('intentional fixture failure')", encoding="utf-8")
        result = self.launch(["--profile", "LowPower", "--fps-limit", "42", "--fov", "105"])
        self.assertNotEqual(result.returncode, 0)
        metadata = self.metadata(self.manifests()[0])
        self.assertEqual(metadata["state"], "exited")
        self.assertEqual(metadata["exit_code"], 1)
        log = Path(metadata["log_path"]).read_text(encoding="utf-8")
        self.assertIn("[stderr] ", log)
        self.assertIn("intentional fixture failure", log)

    def test_fov_boundaries_and_decimal_formatting(self):
        for arguments in (["--fov", "60"], ["--fov", "110"], ["--fov", "100.5"], []):
            self.launch_ok(arguments)
        self.assertEqual([row.get("MM3_FOV_DEGREES") for row in self.records()],
                         ["60", "110", "100.5", None])

    def test_out_of_range_fov_never_runs_entry_point(self):
        marker = self.folder / "entry-point-was-run"
        self.collector.write_text("open(%r, 'w').write('unexpected')" % str(marker), encoding="utf-8")
        for arguments in (["--fov", "59"], ["--fov", "111"], ["--fov", "nan"]):
            result = self.launch(arguments)
            self.assertNotEqual(result.returncode, 0)
        self.assertFalse(marker.exists())
        self.assertFalse((self.folder / "logs").exists())

    def test_trace_switch_records_launch_configuration_and_respects_overrides(self):
        overrides = {"MM3_BUILDING_TRACE": "0", "RECOMP_DRAW_DISTANCE": "1.25", "MM3_FOV_DEGREES": "97"}
        self.launch_ok(["--trace-buildings", "--profile", "LowPower", "--fov", "100"], overrides)
        self.launch_ok(["--no-trace-buildings"], overrides)
        first, second = self.records()
        self.assertEqual(first["MM3_BUILDING_TRACE"], "0")
        self.assertEqual(first["RECOMP_TEXTURE_STREAM_TRACE"], "1")
        self.assertEqual(first["RECOMP_ASYNC_WAIT_TRACE"], "1")
        self.assertIsNone(second.get("RECOMP_TEXTURE_STREAM_TRACE"))
        self.assertIsNone(second.get("RECOMP_ASYNC_WAIT_TRACE"))
        manifests = self.manifests()
        self.assertEqual(len(manifests), 2)
        metadata = self.metadata(manifests[0])
        self.assertEqual(metadata["state"], "exited")
        self.assertEqual(metadata["exit_code"], 0)
        self.assertEqual(metadata["environment"]["RECOMP_DRAW_DISTANCE"], "1.25")
        self.assertEqual(metadata["environment"]["MM3_FOV_DEGREES"], "97")
        self.assertEqual(metadata["profile_defaults_for_unset_settings"]["draw_distance"], "1")
        self.assertTrue(metadata["launch_options"]["trace_buildings"])
        self.assertEqual(metadata["executable"]["sha256"], sha256(self.collector))
        self.assertEqual(metadata["arguments"][-1], str(self.collector))
        files = {entry["path"]: entry for entry in metadata["configuration_files"]}
        xbe = self.folder / "game_files/default.xbe"
        self.assertEqual(files[str(xbe)]["sha256"], sha256(xbe))
        for manifest in manifests:
            saved = self.metadata(manifest)
            log = Path(saved["log_path"]).read_text(encoding="utf-8")
            self.assertRegex(log, r"(?m)^\d{4}-\d\d-\d\dT.*\[stdout\] ")
            self.assertIn('"MM3_PERFORMANCE_PROFILE"', log)

    def test_console_keys_are_redacted_from_metadata_and_preserved_for_child(self):
        # Synthetic values only. The child stores them privately; stdout never shows them.
        overrides = {"RECOMP_XBOX_HDKEY": "11" * 16, "RECOMP_XBOX_SIGKEY": "22" * 16,
                     "RECOMP_XBOX_LANKEY": "33" * 16}
        self.launch_ok([], overrides)
        for key, value in overrides.items():
            self.assertEqual(self.records()[0][key], value)
        manifest = self.manifests()[0]
        metadata_text = manifest.read_text(encoding="utf-8")
        metadata = json.loads(metadata_text)
        log = Path(metadata["log_path"]).read_text(encoding="utf-8")
        for key, value in overrides.items():
            self.assertEqual(metadata["environment"][key], "[REDACTED]")
            self.assertNotIn(value, metadata_text)
            self.assertNotIn(value, log)

    def test_multiple_instances_requires_explicit_save_root_before_launch(self):
        result = self.launch(["--allow-multiple-instances"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("MM3_SAVE_DIR", result.stderr)
        self.assertEqual(self.records(), [])
        self.assertFalse((self.folder / "logs").exists())

    def start_game(self, save_root, tag, allow=True):
        """Start a launcher whose game fixture is named like the real executable."""
        ready, release = self.folder / (tag + " ready"), self.folder / (tag + " release")
        environment = dict(self.environment, MM3_SAVE_DIR=str(save_root),
                           MM3_FIXTURE_READY=str(ready), MM3_FIXTURE_RELEASE=str(release))
        command = [sys.executable, str(self.launcher), "--exe", str(self.game_fixture)]
        if allow:
            command.append("--allow-multiple-instances")
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, encoding="utf-8",
                                   errors="replace", env=environment, cwd=self.folder)
        # The fixture game is a Python interpreter reading its script from stdin.
        process.stdin.write(GAME_SCRIPT)
        process.stdin.close()
        process.stdin = None  # communicate() must not try to flush the closed pipe
        return process, ready, release

    def make_game_fixture(self):
        self.game_fixture = self.folder / GAME_NAME
        if os.name == "nt":
            shutil.copy2(sys.executable, self.game_fixture)
        else:
            os.symlink(sys.executable, self.game_fixture)

    @staticmethod
    def wait_ready(process, ready):
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline and not ready.exists():
            if process.poll() is not None:
                raise AssertionError("fixture launch failed: " + process.communicate()[1])
            time.sleep(.05)
        if not ready.exists():
            raise AssertionError("fixture did not start")
        if process.poll() is not None:
            raise AssertionError("fixture exited early")

    def test_multiple_instances_allow_distinct_roots_block_aliases_and_keep_default_guard(self):
        self.make_game_fixture()
        active = []
        try:
            root_one, root_two = self.folder / "save one", self.folder / "save two"
            first, first_ready, first_release = self.start_game(root_one, "first")
            active.append((first, first_release))
            self.wait_ready(first, first_ready)
            second, second_ready, second_release = self.start_game(root_two, "second")
            active.append((second, second_release))
            self.wait_ready(second, second_ready)
            self.assertIsNone(first.poll(), "starting a distinct-root instance disturbed the first")
            # A lexical alias must resolve to the same save root and be refused.
            alias, alias_ready, alias_release = self.start_game("save one/./unused/..", "alias")
            active.append((alias, alias_release))
            output, error = alias.communicate(timeout=30)
            self.assertNotEqual(alias.returncode, 0, output)
            self.assertIn("using save directory", error)
            self.assertFalse(alias_ready.exists())
            # Without --allow-multiple-instances the process guard refuses it.
            normal, normal_ready, normal_release = self.start_game(self.folder / "save three", "normal", allow=False)
            active.append((normal, normal_release))
            output, error = normal.communicate(timeout=30)
            self.assertNotEqual(normal.returncode, 0, output)
            self.assertIn("already running", error)
            self.assertFalse(normal_ready.exists())
            manifests = self.manifests()
            self.assertEqual(len(manifests), 2, "refused launches created log metadata")
            for manifest in manifests:
                metadata = self.metadata(manifest)
                self.assertTrue(metadata["launch_options"]["allow_multiple_instances"])
                self.assertIn(metadata["environment"]["MM3_SAVE_DIR"], {str(root_one), str(root_two)})
            self.assertIsNone(first.poll())
            self.assertIsNone(second.poll())
        finally:
            for process, release in active:
                release.touch()
            for process, release in active:
                _, error = process.communicate(timeout=30)
                if release.name.startswith(("first ", "second ")):
                    self.assertEqual(process.returncode, 0, error)

    def test_second_game_is_refused_before_launch_or_log_creation(self):
        self.make_game_fixture()
        release = self.folder / "game release"
        game = subprocess.Popen([str(self.game_fixture)], stdin=subprocess.PIPE,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True,
                                env=dict(self.environment, MM3_FIXTURE_READY=str(self.folder / "game ready"),
                                         MM3_FIXTURE_RELEASE=str(release)), cwd=self.folder)
        game.stdin.write(GAME_SCRIPT)
        game.stdin.close()
        game.stdin = None  # communicate() must not try to flush the closed pipe
        try:
            self.wait_ready(game, self.folder / "game ready")
            result = self.launch(exe=self.game_fixture)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("already running", result.stderr)
            self.assertIn(str(game.pid), result.stderr)
            self.assertIn("Close all existing game windows", result.stderr)
            self.assertIn("cache", result.stderr)
            self.assertFalse((self.folder / "logs").exists())
            self.assertIsNone(game.poll(), "preflight disturbed the existing game process")
        finally:
            release.touch()
            game.communicate(timeout=30)

    def test_both_streams_are_saved_before_exit_and_nonzero_exit_code_survives(self):
        release = self.folder / "release child"
        self.collector.write_text(HOLD_COLLECTOR, encoding="utf-8")
        environment = dict(self.environment, PLAY_TEST_RELEASE=str(release))
        # Redirect the outer launcher to files so its console echo cannot fill a pipe.
        with (self.folder / "outer.out").open("w", encoding="utf-8") as output, \
                (self.folder / "outer.err").open("w", encoding="utf-8") as error:
            process = subprocess.Popen([sys.executable, str(self.launcher), "--exe", str(self.collector)],
                                       stdout=output, stderr=error, env=environment, cwd=self.folder)
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
        metadata = self.metadata(self.manifests()[0])
        self.assertEqual(metadata["exit_code"], 37)
        self.assertEqual(metadata["state"], "exited")
        self.assertNotIn("capture_error", metadata)
        log = Path(metadata["log_path"]).read_text(encoding="utf-8")
        self.assertEqual(log.count("[stdout] output-"), 2500)
        self.assertEqual(log.count("[stderr] error-"), 2500)
        # Two pipes read concurrently: lines land in arrival order, so the
        # unterminated last line is checked for, not its position.
        self.assertIn("[stderr] final stderr without newline\n", log)


if __name__ == "__main__":
    unittest.main()

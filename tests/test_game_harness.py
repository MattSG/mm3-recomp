"""Fast stdlib regressions, including a real Windows pipe/child-process fixture.

python -m unittest discover -s tests -p test_game_harness.py -v
The fixture is synthetic; these checks never claim gameplay acceptance.
"""
import ctypes
from ctypes import wintypes
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("game_harness", ROOT / "tools/game_test/run.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def car(x, *, player=True, ident=1):
    return dict(id=ident, player=player, x=x, z=0, fx=1, fz=0)


def tracks(xs, ai=False):
    return [dict(t=i + .1, cars=[car(x)] + ([car(x, player=False, ident=2)] if ai else [])) for i, x in enumerate(xs)]


class OracleTests(unittest.TestCase):
    def test_motion_and_ai_have_independent_requirements(self):
        self.assertEqual(len(harness.movement(tracks([0, 2, 4], ai=True), require_ai=True)), 2)
        for samples, options in (([], {}), (tracks([0, 0, 0]), {}), (tracks([4, 2, 0]), {}),
                                 (tracks([0, 2, 4]), {"require_ai": True})):
            with self.subTest(samples=samples, options=options), self.assertRaises(AssertionError):
                harness.movement(samples, **options)
        samples = tracks([0, 2, 4], ai=True)
        for i, row in enumerate(samples):
            row["cars"][1]["x"] = -i * 8
        with self.assertRaises(AssertionError):
            harness.movement(samples, require_ai=True)

    def test_invalid_missing_and_disappearing_telemetry_fail(self):
        for response in ({}, {"cars": [car(float("nan"))]}, {"cars": [car(1), car(2)]},
                         {"cars": [dict(car(1), fx=0, fz=0)]}):
            with self.subTest(response=response), self.assertRaises(ValueError):
                harness.validate_cars(response)
        samples = tracks([0, 2, 4], ai=True)
        samples[1]["cars"].pop()
        with self.assertRaises(AssertionError):
            harness.movement(samples)

    def test_scenario_validation_before_launch(self):
        invalid = [{}, {"steps": []}, {"steps": [{"action": "wait", "seconds": 1}]},
                   {"steps": [{"action": "press", "buttons": "a\nrt"}]},
                   {"steps": [{"action": "teleport", "x": float("inf"), "z": 0}]},
                   {"steps": [{"action": "sample", "seconds": .1, "interval": 1}]},
                   {"steps": [{"action": "wait_player", "typo": 1}]},
                   {"steps": [{"action": "peek", "address": 0, "size": 4}]}]
        for scenario in invalid:
            with self.subTest(scenario=scenario), self.assertRaises(ValueError):
                harness.validate_scenario(scenario)
        for path in (ROOT / "tools/game_test").glob("*.json"):
            harness.validate_scenario(json.loads(path.read_text()))
        self.assertEqual(harness.pad_line("p2-rt+lleft", 1000), "p2-rt+lleft:1000\n")

    def test_reject_template_links_and_managed_overrides(self):
        for key in ("MM3_SAVE_DIR", "RECOMP_PAD_LIVE", "PATH"):
            with self.assertRaises(ValueError):
                harness.Session("missing", ".", env={key: "elsewhere"})
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            (source / "profile").write_bytes(b"untouched")
            harness.copy_save(source, root / "copied")
            (root / "copied/profile").write_bytes(b"changed")
            self.assertEqual((source / "profile").read_bytes(), b"untouched")
            harness.link_assets(source, root / "link")
            try:
                with self.assertRaises(ValueError):
                    harness.copy_save(root / "link", root / "bad")
            finally:
                (root / "link").rmdir() if os.name == "nt" else (root / "link").unlink()


@unittest.skipUnless(os.name == "nt", "Windows named-pipe runtime")
class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.assets = self.root / "assets"
        self.assets.mkdir()
        (self.assets / "default.xbe").write_bytes(b"fixture-not-a-game")
        self.template = self.root / "template"
        self.template.mkdir()
        (self.template / "profile").write_bytes(b"original")
        real_popen = subprocess.Popen

        def launch(command, **kwargs):
            if command == [str(Path(sys.executable).resolve())]:
                command = [sys.executable, str(Path(__file__).resolve()), "--fixture"]
            return real_popen(command, **kwargs)

        self.patcher = patch.object(harness.subprocess, "Popen", side_effect=launch)
        self.patcher.start()
        self.addCleanup(self.patcher.stop)

    def session(self, **options):
        return harness.Session(sys.executable, self.assets, save_template=self.template,
                               output_root=self.root / "runs", timeout=12, **options)

    def test_sparse_partition_clone_keeps_holes_and_contents(self):
        import msvcrt
        source, destination = self.root / "sparse.img", self.root / "copy.img"
        k = ctypes.WinDLL("kernel32", use_last_error=True)
        k.DeviceIoControl.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
                                     ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
        k.GetCompressedFileSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
        k.GetCompressedFileSizeW.restype = wintypes.DWORD
        with source.open("wb") as stream:
            returned = wintypes.DWORD()
            self.assertTrue(k.DeviceIoControl(msvcrt.get_osfhandle(stream.fileno()), 0x900C4,
                                             None, 0, None, 0, ctypes.byref(returned), None))
            stream.write(b"first")
            stream.seek(2**32)
            stream.write(b"last")
        harness.copy_save_file(source, destination)
        self.assertEqual(destination.stat().st_size, 2**32 + 4)
        with destination.open("rb") as stream:
            self.assertEqual(stream.read(5), b"first")
            stream.seek(2**31)
            self.assertEqual(stream.read(8), bytes(8))
            stream.seek(2**32)
            self.assertEqual(stream.read(), b"last")
        high = wintypes.DWORD()
        low = k.GetCompressedFileSizeW(str(destination), ctypes.byref(high))
        self.assertLess((high.value << 32) | low, 1024 * 1024)

    def test_full_scenario_real_pipe_capture_and_owned_cleanup(self):
        # A separately running sentinel must survive success and failed tests.
        sentinel = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
        self.addCleanup(lambda: sentinel.poll() is None and sentinel.terminate())
        scenario = {"steps": [
            {"action": "wait_log", "text": "[FIXTURE_READY]", "timeout": 3},
            {"action": "wait_player", "timeout": 3},
            {"action": "press", "buttons": "rt", "hold_ms": 1000, "wait_release": False},
            {"action": "wait_motion", "timeout": 3},
            {"action": "sample", "seconds": .15, "interval": .03, "require_ai": True, "min_distance": .1},
            {"action": "teleport", "x": 100, "z": 0, "heading": 0}]}
        try:
            with self.session(env={"RECOMP_XBOX_HDKEY": "secret-fixture"}) as session:
                owned_pid = session.process.pid
                with self.assertRaises(RuntimeError):
                    session.__enter__()
                self.assertEqual(session.process.pid, owned_pid)
                session.run(scenario)
                session.run({"steps": [{"action": "peek", "address": 65536, "size": 4},
                                       {"action": "capture", "timeout": 3}]})
            self.assertIsNone(sentinel.poll())
            self.assertIsNotNone(session.process.poll())
            result = json.loads((session.run_dir / "result.json").read_text())
            self.assertEqual(result["status"], "passed")
            self.assertEqual([s["index"] for s in result["steps"]], list(range(8)))
            self.assertEqual(len(result["scenarios"]), 2)
            self.assertEqual(result["environment"]["RECOMP_XBOX_HDKEY"], "[REDACTED]")
            self.assertFalse((session.run_dir / "save").exists())
            self.assertFalse((session.run_dir / "game_files").exists())
            self.assertEqual((self.template / "profile").read_bytes(), b"original")
            self.assertEqual((self.assets / "default.xbe").read_bytes(), b"fixture-not-a-game")
        finally:
            sentinel.terminate()
            sentinel.wait(timeout=5)

    def test_pipe_reply_and_run_deadlines_cannot_hang(self):
        with self.session(env={"MM3_FIXTURE_MODE": "hang"}) as session:
            session.wait_for(lambda: "[FIXTURE_READY]" in session.log_text, 3, "fixture startup")
            began = time.monotonic()
            with self.assertRaises(TimeoutError):
                harness.pipe_request(session.process.pid, "cars", .1)
            self.assertLess(time.monotonic() - began, 1)
        short = harness.Session(sys.executable, self.assets, output_root=self.root / "short", timeout=2)
        with self.assertRaises(TimeoutError):
            with short:
                short.run({"steps": [{"action": "wait_log", "text": "never", "timeout": 20}]})
        self.assertIsNotNone(short.process.poll())
        self.assertEqual(json.loads((short.run_dir / "result.json").read_text())["status"], "failed")

    def test_process_exit_and_crash_are_failures_and_keep_private_save(self):
        for mode in ("exit", "crash"):
            with self.subTest(mode=mode):
                session = self.session(env={"MM3_FIXTURE_MODE": mode})
                with self.assertRaises((RuntimeError, AssertionError)):
                    with session:
                        session.run({"steps": [{"action": "wait_log", "text": "never", "timeout": 3}]})
                self.assertEqual(session.result["status"], "failed")
                self.assertTrue((session.run_dir / "save/profile").exists())
                self.assertIsNotNone(session.process.poll())

    def test_caught_failed_check_never_becomes_a_pass(self):
        with self.session() as session:
            with self.assertRaises(TimeoutError):
                session.run({"steps": [{"action": "wait_log", "text": "missing", "timeout": .1}]})
            with self.assertRaises(RuntimeError):
                session.run({"steps": [{"action": "wait_log", "text": "[FIXTURE_READY]"}]})
        self.assertEqual(session.result["status"], "failed")

    def test_failed_motion_keeps_save_and_records_capture_attempt(self):
        session = self.session(env={"MM3_FIXTURE_MODE": "stationary"})
        with self.assertRaises(AssertionError):
            with session:
                session.run({"steps": [{"action": "wait_player", "timeout": 3},
                                       {"action": "sample", "seconds": .1, "interval": .02}]})
        self.assertEqual(session.result["status"], "failed")
        # Failure capture has a one-second best-effort budget, unlike the
        # required capture action exercised by the full scenario test.
        if "failure_capture" in session.result:
            self.assertTrue(Path(session.result["failure_capture"]["path"]).is_file())
        else:
            self.assertTrue(session.result["failure_capture_error"])
        self.assertTrue((session.run_dir / "save/profile").is_file())


def fixture():
    """Real local pipe server; intentionally simple synthetic game replacement."""
    mode = os.getenv("MM3_FIXTURE_MODE", "normal")
    if mode == "exit":
        return 7
    if mode == "crash":
        print("[CRASH] synthetic", flush=True)
        time.sleep(10)
        return 0
    import threading
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.CreateNamedPipeW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
                                  wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    k.CreateNamedPipeW.restype = wintypes.HANDLE
    k.ConnectNamedPipe.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
    k.ReadFile.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
    k.WriteFile.argtypes = k.ReadFile.argtypes
    k.FlushFileBuffers.argtypes = [wintypes.HANDLE]
    k.DisconnectNamedPipe.argtypes = [wintypes.HANDLE]
    k.CloseHandle.argtypes = [wintypes.HANDLE]
    position = [0.0]

    def server():
        while True:
            handle = k.CreateNamedPipeW(rf"\\.\pipe\MM3Teleport-{os.getpid()}", 3, 6, 1, 4096, 4096, 0, None)
            if handle == ctypes.c_void_p(-1).value:
                raise ctypes.WinError(ctypes.get_last_error())
            k.ConnectNamedPipe(handle, None)
            data, size = ctypes.create_string_buffer(128), wintypes.DWORD()
            if k.ReadFile(handle, data, 128, ctypes.byref(size), None):
                command = data.raw[:size.value].decode("ascii")
                if mode == "hang":
                    time.sleep(10)
                if command == "cars":
                    if mode != "stationary":
                        position[0] += 2
                    reply = json.dumps(dict(cars=[car(position[0]), car(position[0], player=False, ident=2)]))
                elif command.startswith("teleport "):
                    position[0] = float(command.split()[1])
                    reply = json.dumps(dict(result="applied", x=position[0], y=0, z=0))
                elif command.startswith("peek "):
                    reply = "00" * int(command.split()[2], 16)
                else:
                    reply = json.dumps(dict(available=True, x=position[0], y=0, z=0))
                encoded = reply.encode("ascii")
                if k.WriteFile(handle, encoded, len(encoded), ctypes.byref(size), None):
                    k.FlushFileBuffers(handle)
            k.DisconnectNamedPipe(handle)
            k.CloseHandle(handle)

    threading.Thread(target=server, daemon=True).start()
    print("[FIXTURE_READY]", flush=True)
    flag = Path(os.environ["RECOMP_PRESENT_CAPTURE"] + ".flag")
    pad_offset = 0
    while True:
        with Path(os.environ["RECOMP_PAD_LIVE"]).open() as pad:
            pad.seek(pad_offset)
            for line in pad:
                print(f'  PAD: live "{line.strip()}" at t=0 ms', flush=True)
            pad_offset = pad.tell()
        if flag.exists():
            flag.unlink()
            header = bytearray(54)
            header[:2] = b"BM"
            struct.pack_into("<I", header, 2, 70)
            struct.pack_into("<I", header, 10, 54)
            struct.pack_into("<IiiHH", header, 14, 40, 2, 2, 1, 24)
            Path(os.environ["RECOMP_PRESENT_CAPTURE"] + "-000.bmp").write_bytes(header + bytes(16))
        time.sleep(.01)


if __name__ == "__main__":
    if "--fixture" in sys.argv:
        raise SystemExit(fixture())
    unittest.main()

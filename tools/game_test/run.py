"""Bounded MM3 runtime scenarios; Python 3.11+, Windows, standard library only."""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time


_UTIL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "xboxrecomp", "tools", "game_test", "harness_util.py")
_util_spec = importlib.util.spec_from_file_location("harness_util", _UTIL)
_util = importlib.util.module_from_spec(_util_spec)
_util_spec.loader.exec_module(_util)
number = _util.number
sha256 = _util.sha256
linked = _util.linked
copy_save_file = _util.copy_save_file
copy_save = _util.copy_save


ROOT = Path(__file__).resolve().parents[2]
BUTTONS = set("up down left right start back lthumb rthumb a b x y black white lt rt "
              "lleft lright lup ldown rleft rright rup rdown".split())
MANAGED = {"MM3_SAVE_DIR", "RECOMP_PAD_LIVE", "RECOMP_PAD_SCRIPT", "RECOMP_PAD_PRESS",
           "MM3_DEBUG_TELEPORT", "RECOMP_PRESENT_CAPTURE", "RECOMP_FRAME_CSV", "RECOMP_PB_EXEC"}




def pad_line(buttons, hold_ms=150):
    if not isinstance(buttons, str):
        raise ValueError("buttons must be a string")
    plain = buttons[3:] if buttons.startswith(tuple(f"p{i}-" for i in range(1, 5))) else buttons
    if not plain or any(button not in BUTTONS for button in plain.split("+")):
        raise ValueError(f"Unknown pad buttons: {buttons!r}")
    number(hold_ms, "hold_ms", 1, 60000)
    if int(hold_ms) != hold_ms:
        raise ValueError("hold_ms must be an integer")
    return f"{buttons}:{int(hold_ms)}\n"




def invalid_json_constant(value):
    raise ValueError(f"Nonfinite pipe JSON: {value}")








def link_assets(source, destination):
    # A directory junction needs no administrator/developer-mode privilege.
    if os.name == "nt":
        env = os.environ.copy()
        env.update(MM3_TEST_ASSETS=str(source), MM3_TEST_LINK=str(destination))
        subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
                        "$ErrorActionPreference='Stop'; New-Item -ItemType Junction "
                        "-Path $env:MM3_TEST_LINK -Target $env:MM3_TEST_ASSETS | Out-Null"],
                       env=env, check=True, capture_output=True, timeout=15)
    else:
        destination.symlink_to(source, target_is_directory=True)


def pipe_request(pid, command, timeout=1.0):
    """One overlapped transaction: connection AND reply share a real deadline."""
    if os.name != "nt":
        raise OSError("MM3 named pipes require Windows")
    number(timeout, "pipe timeout", .001, 600)
    payload = command.encode("ascii")
    if not payload or len(payload) > 127 or b"\0" in payload:
        raise ValueError("Invalid MM3 pipe command")
    k = ctypes.WinDLL("kernel32", use_last_error=True)

    class Overlapped(ctypes.Structure):
        _fields_ = [("Internal", ctypes.c_size_t), ("InternalHigh", ctypes.c_size_t),
                    ("Offset", wintypes.DWORD), ("OffsetHigh", wintypes.DWORD), ("hEvent", wintypes.HANDLE)]

    k.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                             wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    k.CreateFileW.restype = wintypes.HANDLE
    k.CreateEventW.argtypes = [ctypes.c_void_p, wintypes.BOOL, wintypes.BOOL, wintypes.LPCWSTR]
    k.CreateEventW.restype = wintypes.HANDLE
    k.WaitNamedPipeW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
    k.SetNamedPipeHandleState.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p, ctypes.c_void_p]
    k.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    k.TransactNamedPipe.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
                                   ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
                                   ctypes.POINTER(Overlapped)]
    k.GetOverlappedResult.argtypes = [wintypes.HANDLE, ctypes.POINTER(Overlapped),
                                    ctypes.POINTER(wintypes.DWORD), wintypes.BOOL]
    k.CancelIoEx.argtypes = [wintypes.HANDLE, ctypes.POINTER(Overlapped)]
    k.CloseHandle.argtypes = [wintypes.HANDLE]
    deadline = time.monotonic() + timeout
    name = rf"\\.\pipe\MM3Teleport-{pid}"
    handle = ctypes.c_void_p(-1).value
    while time.monotonic() < deadline:
        handle = k.CreateFileW(name, 0xC0000000, 0, None, 3, 0x40000000, None)
        if handle != ctypes.c_void_p(-1).value:
            break
        error = ctypes.get_last_error()
        if error not in (2, 231):
            raise ctypes.WinError(error)
        k.WaitNamedPipeW(name, max(1, min(50, int((deadline - time.monotonic()) * 1000))))
        time.sleep(min(.01, max(0, deadline - time.monotonic())))
    if handle == ctypes.c_void_p(-1).value:
        raise TimeoutError(f"MM3 pipe {pid} not available")
    event = None
    pending = False
    operation = Overlapped()
    size = wintypes.DWORD()
    try:
        mode = wintypes.DWORD(2)  # PIPE_READMODE_MESSAGE, required by TransactNamedPipe.
        if not k.SetNamedPipeHandleState(handle, ctypes.byref(mode), None, None):
            raise ctypes.WinError(ctypes.get_last_error())
        event = k.CreateEventW(None, True, False, None)
        if not event:
            raise ctypes.WinError(ctypes.get_last_error())
        operation.hEvent = event
        output = ctypes.create_string_buffer(4096)
        source = ctypes.create_string_buffer(payload)
        if not k.TransactNamedPipe(handle, source, len(payload), output, len(output), ctypes.byref(size), ctypes.byref(operation)):
            error = ctypes.get_last_error()
            if error != 997:  # ERROR_IO_PENDING
                raise ctypes.WinError(error)
            pending = True
            remaining = max(0, int((deadline - time.monotonic()) * 1000))
            if k.WaitForSingleObject(event, remaining) != 0:
                raise TimeoutError(f"MM3 pipe reply timed out for {command!r}")
            if not k.GetOverlappedResult(handle, ctypes.byref(operation), ctypes.byref(size), False):
                raise ctypes.WinError(ctypes.get_last_error())
            pending = False
        return output.raw[:size.value].decode("ascii")
    finally:
        if pending:
            k.CancelIoEx(handle, ctypes.byref(operation))
            k.GetOverlappedResult(handle, ctypes.byref(operation), ctypes.byref(size), True)
        k.CloseHandle(handle)
        if event:
            k.CloseHandle(event)


def validate_cars(response):
    cars = response.get("cars") if isinstance(response, dict) else None
    if not isinstance(cars, list):
        raise ValueError("Pipe response has no cars array")
    ids = set()
    for car in cars:
        if not isinstance(car, dict) or type(car.get("id")) is not int or not 0 < car["id"] <= 0xFFFFFFFF:
            raise ValueError("Invalid car identity")
        if car["id"] in ids or type(car.get("player")) is not bool:
            raise ValueError("Duplicate car identity or invalid player flag")
        ids.add(car["id"])
        for key in ("x", "z", "fx", "fz"):
            number(car.get(key), f"car {key}", -1e7, 1e7)
        if not .1 <= math.hypot(car["fx"], car["fz"]) <= 2:
            raise ValueError("Invalid car forward vector")
    if sum(car["player"] for car in cars) > 1:
        raise ValueError("Multiple primary players in car response")
    return cars


def movement(samples, min_distance=1.0, require_ai=False, max_backward=10.0):
    """Independent movement oracle; no telemetry, no motion or disappearing cars fail."""
    if len(samples) < 3:
        raise AssertionError("Need at least three telemetry samples")
    tracks = {}
    for index, sample in enumerate(samples):
        if index and sample["t"] <= samples[index - 1]["t"]:
            raise AssertionError("Telemetry timestamps must increase")
        for car in validate_cars(sample):
            tracks.setdefault(car["id"], []).append(car)
    rows = []
    for ident, track in tracks.items():
        if len(track) != len(samples) or any(c["player"] != track[0]["player"] for c in track):
            raise AssertionError(f"Car {ident} disappeared or changed identity during sampling")
        forward = backward = distance = 0.0
        for a, b in zip(track, track[1:]):
            dx, dz = b["x"] - a["x"], b["z"] - a["z"]
            along = (dx * a["fx"] + dz * a["fz"]) / math.hypot(a["fx"], a["fz"])
            forward += max(0, along)
            backward += max(0, -along)
            distance += math.hypot(dx, dz)
        a, b = track[0], track[-1]
        net = ((b["x"] - a["x"]) * a["fx"] + (b["z"] - a["z"]) * a["fz"]) / math.hypot(a["fx"], a["fz"])
        rows.append(dict(id=ident, player=a["player"], samples=len(track), distance=distance,
                         forward=forward, backward=backward, net=net))
    players = [row for row in rows if row["player"]]
    ais = [row for row in rows if not row["player"]]
    if len(players) != 1 or players[0]["distance"] < min_distance or players[0]["forward"] <= players[0]["backward"]:
        raise AssertionError("Primary player did not drive forwards under throttle")
    if require_ai and not ais:
        raise AssertionError("No AI opponents observed; AI test is inconclusive")
    if require_ai and any(row["distance"] < min_distance or row["backward"] > max_backward or row["net"] < -1 for row in ais):
        raise AssertionError("An AI opponent stalled or reversed off the start line")
    return rows


def validate_scenario(scenario):
    if not isinstance(scenario, dict) or not isinstance(scenario.get("steps"), list) or not scenario["steps"]:
        raise ValueError("Scenario must contain a nonempty steps array")
    fields = {
        "press": {"buttons", "hold_ms", "after", "wait_release"}, "wait": {"seconds"},
        "wait_log": {"text", "timeout"}, "wait_player": {"timeout", "advance"},
        "wait_motion": {"timeout", "min_distance"},
        "sample": {"seconds", "interval", "min_distance", "require_ai", "max_backward"},
        "teleport": {"x", "z", "heading", "tolerance"}, "capture": {"timeout"},
        "peek": {"address", "size"},
    }
    for step in scenario["steps"]:
        if not isinstance(step, dict) or step.get("action") not in fields:
            raise ValueError(f"Unknown scenario action: {step!r}")
        action = step["action"]
        if set(step) - fields[action] - {"action"}:
            raise ValueError(f"Unknown fields for {action}: {set(step) - fields[action] - {'action'}}")
        for key in ("timeout", "seconds", "after", "interval"):
            if key in step:
                number(step[key], key, .01 if key in ("timeout", "seconds", "interval") else 0, 600)
        if action == "press":
            pad_line(step.get("buttons"), step.get("hold_ms", 150))
            if type(step.get("wait_release", True)) is not bool:
                raise ValueError("wait_release must be boolean")
        if action == "wait" and "seconds" not in step:
            raise ValueError("wait requires seconds")
        if action == "wait_log" and (not isinstance(step.get("text"), str) or not step["text"]):
            raise ValueError("wait_log requires a nonempty literal text")
        if action == "wait_player" and "advance" in step:
            pad_line(step["advance"])
        if action == "wait_motion":
            number(step.get("min_distance", 1), "min_distance", .01, 1e7)
        if action == "sample":
            number(step.get("min_distance", 1), "min_distance", .01, 1e7)
            number(step.get("max_backward", 10), "max_backward", 0, 1e7)
            if type(step.get("require_ai", False)) is not bool:
                raise ValueError("require_ai must be boolean")
            if step.get("seconds", 5) < 2 * step.get("interval", .1):
                raise ValueError("sample duration must allow at least three samples")
        if action == "teleport":
            for key in ("x", "z"):
                number(step.get(key), key, -1e7, 1e7)
            if "heading" in step:
                number(step["heading"], "heading", -1e6, 1e6)
            number(step.get("tolerance", 2), "tolerance", .001, 100)
        if action == "peek":
            address, size = step.get("address"), step.get("size")
            if type(address) is not int or type(size) is not int or not 0x10000 <= address < 0x8000000 or not 1 <= size <= 512 or address + size > 0x8000000:
                raise ValueError("peek requires a guest RAM address and size 1..512")
    if not any(s["action"] in ("wait_log", "wait_player", "wait_motion", "sample", "teleport", "capture", "peek") for s in scenario["steps"]):
        raise ValueError("Scenario has no observable check")
    return scenario


class Session:
    """Own exactly one process. Use the context manager for guaranteed cleanup."""
    def __init__(self, exe, game_dir, *, save_template=None, output_root=None, timeout=180, env=None, keep_save=False):
        number(timeout, "timeout", .1, 3600)
        self.exe, self.game_dir = Path(exe).resolve(), Path(game_dir).resolve()
        self.save_template = Path(save_template).absolute() if save_template else None
        self.output_root = Path(output_root or ROOT / "conformance_tmp/game_test").resolve()
        self.timeout, self.overrides, self.keep_save = timeout, dict(env or {}), keep_save
        for key, value in self.overrides.items():
            if not key.startswith(("MM3_", "RECOMP_")) or key in MANAGED or not isinstance(value, str) or "\0" in value:
                raise ValueError(f"Unsupported or harness-owned environment setting: {key}")
        self.process = None
        self.run_dir = None
        self.files = []
        self.log_offsets = {}
        self.log_pending = {}
        self.pad_ack = {}
        self.log_text = ""
        self.result = {"schema_version": 1, "status": "running", "steps": [], "error": None}

    def emit(self, event, **data):
        row = dict(event=event, run_elapsed=round(time.monotonic() - self.started, 4), **data)
        with (self.run_dir / "events.jsonl").open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(row, allow_nan=False) + "\n")

    def check(self):
        if time.monotonic() >= self.deadline:
            raise TimeoutError(f"Run exceeded {self.timeout}s deadline")
        self.read_logs()
        if self.process.poll() is not None:
            raise RuntimeError(f"Game exited unexpectedly: {self.process.returncode}")

    def read_logs(self):
        for name in ("stderr.log", "stdout.log"):
            with (self.run_dir / name).open("rb") as stream:
                stream.seek(self.log_offsets.get(name, 0))
                data = stream.read()
                self.log_offsets[name] = stream.tell()
            text = data.decode("utf-8", errors="replace")
            self.log_text = (self.log_text + text)[-262144:]
            lines = (self.log_pending.get(name, "") + text).split("\n")
            self.log_pending[name] = lines.pop()
            for line in lines:
                _, found, tail = line.partition('PAD: live "')
                command, closed, _ = tail.partition('"')
                if found and closed:
                    self.pad_ack[command] = self.pad_ack.get(command, 0) + 1
        if any(marker in self.log_text for marker in ("[CRASH]", "[ABI_FAIL]", "[ABI_VIOLATION]")):
            raise AssertionError("Game reported a crash or ABI violation; inspect stderr.log")

    def remaining(self, cap):
        self.check()
        return max(.001, min(cap, self.deadline - time.monotonic()))

    def wait(self, seconds):
        number(seconds, "wait", 0, 600)
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.check()
            time.sleep(max(0, min(.05, end - time.monotonic())))
        self.check()

    def wait_for(self, predicate, timeout, description):
        end = min(self.deadline, time.monotonic() + timeout)
        while time.monotonic() < end:
            self.check()
            value = predicate()
            if value:
                return value
            self.wait(min(.05, max(0, end - time.monotonic())))
        raise TimeoutError(f"Timed out waiting for {description}")

    def request(self, command, timeout=1):
        reply = pipe_request(self.process.pid, command, self.remaining(timeout))
        self.check()
        return reply

    def query(self, command, timeout=1):
        reply = json.loads(self.request(command, timeout), parse_constant=invalid_json_constant)
        if not isinstance(reply, dict) or reply.get("error"):
            raise ValueError(f"Invalid MM3 response: {reply}")
        return reply

    def press(self, buttons, hold_ms=150, after=0, wait_release=True):
        self.check()
        line = pad_line(buttons, hold_ms)
        command = line.strip()
        previous = self.pad_ack.get(command, 0)
        with (self.run_dir / "pad.txt").open("a", encoding="ascii", newline="\n") as stream:
            stream.write(line)
        self.emit("press", buttons=buttons, hold_ms=hold_ms)
        # The original network readiness path can delay input polling during
        # startup. Require an actual acknowledgement, within the run deadline.
        self.wait_for(lambda: self.pad_ack.get(command, 0) > previous, 30, f"USB input acknowledgement: {command}")
        self.wait((hold_ms / 1000 + .25 if wait_release else 0) + after)

    def wait_player(self, timeout=30, advance=None):
        next_press = 0
        stable_id, stable_since = None, 0
        def ready():
            nonlocal next_press, stable_id, stable_since
            try:
                cars = validate_cars(self.query("cars", .2))
            except (OSError, TimeoutError):
                cars = []
            player = next((car for car in cars if car["player"]), None)
            if player:
                if player["id"] != stable_id:
                    stable_id, stable_since = player["id"], time.monotonic()
                # The native pipe retains samples for 500 ms. Require longer
                # continuous availability to reject a stale loading/menu car.
                return player if time.monotonic() - stable_since >= 1 else None
            stable_id = None
            if not player and advance and time.monotonic() >= next_press:
                self.press(advance)
                next_press = time.monotonic() + 1
            return None
        return self.wait_for(ready, timeout, "fresh primary-player telemetry")

    def wait_motion(self, timeout=10, min_distance=1):
        baseline = None
        def moving():
            nonlocal baseline
            cars = validate_cars(self.query("cars", .25))
            player = next((car for car in cars if car["player"]), None)
            if not player:
                return None
            if baseline is None:
                baseline = player
            if baseline["id"] != player["id"]:
                raise AssertionError("Primary player changed while waiting for motion")
            distance = math.hypot(player["x"] - baseline["x"], player["z"] - baseline["z"])
            return dict(player=player, distance=distance) if distance >= min_distance else None
        return self.wait_for(moving, timeout, "primary-player motion after countdown/loading")

    def sample(self, seconds=5, interval=.1, min_distance=1, require_ai=False, max_backward=10):
        samples = []
        start = time.monotonic()
        end = start + seconds
        while True:
            cars = validate_cars(self.query("cars", min(1, interval + .2)))
            if not any(car["player"] for car in cars):
                raise AssertionError("Primary-player telemetry missing during gameplay")
            row = dict(t=time.monotonic() - start, cars=cars)
            samples.append(row)
            self.emit("cars", **row)
            if time.monotonic() >= end:
                break
            self.wait(min(interval, max(0, end - time.monotonic())))
        rows = movement(samples, min_distance, require_ai, max_backward)
        return dict(duration=samples[-1]["t"], cars=rows)

    def teleport(self, x, z, heading=None, tolerance=2):
        command = f"teleport {x:.9g} {z:.9g}" + (f" {heading:.9g}" if heading is not None else "")
        reply = self.query(command, 3)
        if reply.get("result") != "applied":
            raise AssertionError(f"Native teleport rejected: {reply}")
        def placed():
            reply = self.query("position", .25)
            if reply.get("available") is not True:
                return None
            for key in ("x", "y", "z"):
                number(reply.get(key), key, -1e7, 1e7)
            return reply if math.hypot(reply["x"] - x, reply["z"] - z) <= tolerance else None
        return self.wait_for(placed, 3, "verified teleport position")

    def capture(self, timeout=10):
        previous = set(self.run_dir.glob("capture-*.bmp"))
        (self.run_dir / "capture.flag").write_text("1", encoding="ascii")
        def completed():
            for path in set(self.run_dir.glob("capture-*.bmp")) - previous:
                with path.open("rb") as stream:
                    header = stream.read(54)
                if len(header) == 54 and header[:2] == b"BM":
                    size = struct.unpack_from("<I", header, 2)[0]
                    if size >= 54 and path.stat().st_size == size:
                        width, height = struct.unpack_from("<ii", header, 18)
                        if width > 0 and height != 0:
                            return dict(path=str(path), width=width, height=abs(height))
            return None
        return self.wait_for(completed, timeout, "complete presented BMP (pixels need visual review)")

    def perform(self, step):
        action = step["action"]
        options = {key: value for key, value in step.items() if key != "action"}
        if action == "wait_log":
            text = options["text"]
            return self.wait_for(lambda: text in self.log_text, options.get("timeout", 30), repr(text))
        if action == "peek":
            reply = self.request(f"peek {options['address']:x} {options['size']:x}")
            if len(reply) != 2 * options["size"]:
                raise AssertionError("Guest peek returned no complete mapped range")
            return dict(hex=bytes.fromhex(reply).hex(), **options)
        return getattr(self, action)(**options)

    def run(self, scenario):
        validate_scenario(scenario)
        if self.result["status"] == "failed":
            raise RuntimeError("A previous check failed; this session cannot become a pass")
        self.result["status"] = "running"
        self.result.setdefault("scenarios", []).append(scenario)
        for index, step in enumerate(scenario["steps"], start=len(self.result["steps"])):
            self.emit("step_begin", index=index, step=step)
            began = time.monotonic()
            row = dict(index=index, action=step["action"], status="running")
            self.result["steps"].append(row)
            try:
                row["observed"] = self.perform(step)
                self.check()
                row["status"] = "passed"
            except Exception as error:
                row.update(status="failed", error=str(error))
                self.result.update(status="failed", error=str(error))
                raise
            finally:
                row["elapsed"] = round(time.monotonic() - began, 4)
                self.emit("step_end", **row)
        self.result["status"] = "passed"
        return self.result

    def __enter__(self):
        if self.run_dir is not None:
            raise RuntimeError("Session already started; create a new Session for another process")
        self.started = time.monotonic()
        self.deadline = self.started + self.timeout
        self.output_root.mkdir(parents=True, exist_ok=True)
        self.run_dir = Path(tempfile.mkdtemp(prefix="run-", dir=self.output_root))
        self.result.update(run_dir=str(self.run_dir), executable=str(self.exe), game_dir=str(self.game_dir))
        try:
            if not self.exe.is_file() or not (self.game_dir / "default.xbe").is_file():
                raise FileNotFoundError("Provide a compiled --exe and --game-dir containing default.xbe")
            if self.save_template:
                if not self.save_template.is_dir() or self.output_root.is_relative_to(self.save_template.resolve()):
                    raise ValueError("Save template missing or contains output root")
                copy_save(self.save_template, self.run_dir / "save")
            else:
                (self.run_dir / "save").mkdir()
            self.result.update(executable_sha256=sha256(self.exe), xbe_sha256=sha256(self.game_dir / "default.xbe"),
                               save_template=str(self.save_template) if self.save_template else None)
            link_assets(self.game_dir, self.run_dir / "game_files")
            (self.run_dir / "pad.txt").touch()
            # Do not inherit unrelated diagnostics, input, save paths or render experiments.
            env = {k: v for k, v in os.environ.items() if not k.upper().startswith(("MM3_", "RECOMP_"))}
            settings = {"RECOMP_FB_WINDOW": "1", "RECOMP_USB": "1", "RECOMP_APU_DSP_ACK": "0x80458810",
                        "RECOMP_FPS_LIMIT": "60", "RECOMP_VSYNC": "0", "RECOMP_RENDER_SCALE": "1",
                        "RECOMP_ASPECT": "16:9", "RECOMP_WINDOW_SIZE": "960x540", "RECOMP_MSAA": "1",
                        "MM3_MOD_SKIP_INTRO": "1", "MM3_LOG_UNBUFFERED": "1", "RECOMP_PAUSE_UNFOCUSED": "0"}
            settings.update(self.overrides)
            settings.update(MM3_SAVE_DIR=str(self.run_dir / "save"), RECOMP_PAD_LIVE=str(self.run_dir / "pad.txt"),
                            MM3_DEBUG_TELEPORT="1", RECOMP_PB_EXEC="1", RECOMP_FRAME_CSV=str(self.run_dir / "frames.csv"),
                            RECOMP_PRESENT_CAPTURE=str(self.run_dir / "capture"))
            env.update(settings)
            self.result["environment"] = {k: "[REDACTED]" if any(word in k.upper() for word in ("KEY", "TOKEN", "PASSWORD", "SECRET")) else v for k, v in settings.items()}
            self.files = [(self.run_dir / name).open("wb") for name in ("stdout.log", "stderr.log")]
            if time.monotonic() >= self.deadline:
                raise TimeoutError("Preparation exceeded run deadline")
            self.process = subprocess.Popen([str(self.exe)], cwd=self.run_dir, env=env, stdin=subprocess.DEVNULL,
                                            stdout=self.files[0], stderr=self.files[1])
            self.result["pid"] = self.process.pid
            self.emit("launched", pid=self.process.pid)
            return self
        except BaseException as error:
            self.__exit__(type(error), error, error.__traceback__)
            raise

    def __exit__(self, kind, error, traceback):
        if error:
            self.result.update(status="failed", error=f"{type(error).__name__}: {error}")
        elif self.result["status"] == "running":
            self.result.update(status="inconclusive", error="Session ended without a completed scenario")
        if self.process:
            if error and self.process.poll() is None and time.monotonic() + 1 < self.deadline:
                try:
                    self.result["failure_capture"] = self.capture(1)
                except Exception as capture_error:
                    self.result["failure_capture_error"] = str(capture_error)
            if not error:
                try:
                    self.check()
                except Exception as last_error:
                    self.result.update(status="failed", error=f"{type(last_error).__name__}: {last_error}")
            self.result["exit_before_cleanup"] = self.process.poll()
            if self.process.poll() is None:
                self.process.terminate()  # Only the child handle owned by this Session.
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=5)
                self.result["cleanup"] = "terminated_owned_process"
            self.result["exit_code"] = self.process.returncode
        for stream in self.files:
            stream.close()
        self.result["elapsed"] = round(time.monotonic() - self.started, 4)
        if self.run_dir:
            self.result["log_tail"] = self.log_text[-4096:]
            try:
                # Remove the link itself, never recurse into the user's game assets.
                link = self.run_dir / "game_files"
                if os.path.lexists(link):
                    if os.name == "nt":
                        link.rmdir()
                    else:
                        link.unlink()
                save = self.run_dir / "save"
                if (self.result["status"] == "passed" or self.process is None) and not self.keep_save and save.exists():
                    if save.resolve().parent != self.run_dir or linked(save):
                        raise RuntimeError("Refusing to clean an unexpected save path")
                    shutil.rmtree(save)
            except Exception as cleanup_error:
                self.result.update(status="failed", cleanup_error=str(cleanup_error))
                if not self.result["error"]:
                    self.result["error"] = f"Cleanup failed: {cleanup_error}"
            finally:
                (self.run_dir / "result.json").write_text(json.dumps(self.result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        return False


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--game-dir", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, default=Path(__file__).with_name("driving.json"))
    parser.add_argument("--save-template", type=Path)
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--env", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--keep-save", action="store_true")
    args = parser.parse_args(argv)
    session = None
    try:
        scenario = validate_scenario(json.loads(args.scenario.read_text(encoding="utf-8")))
        overrides = {}
        for item in args.env:
            key, separator, value = item.partition("=")
            if not separator:
                raise ValueError("--env requires NAME=VALUE")
            overrides[key] = value
        session = Session(args.exe, args.game_dir, save_template=args.save_template, output_root=args.output_root,
                          timeout=args.timeout, env=overrides, keep_save=args.keep_save)
        with session:
            session.run(scenario)
        print(json.dumps({k: session.result[k] for k in ("status", "run_dir", "elapsed", "error")}, allow_nan=False))
        return 0 if session.result["status"] == "passed" else 1
    except (Exception, KeyboardInterrupt) as error:
        print(json.dumps(dict(status="failed", error=f"{type(error).__name__}: {error}",
                              run_dir=str(session.run_dir) if session and session.run_dir else None)))
        return 130 if isinstance(error, KeyboardInterrupt) else 1


if __name__ == "__main__":
    raise SystemExit(main())

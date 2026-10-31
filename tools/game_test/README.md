# Fast game checks for agents

Python 3.11+ and the Windows standard library; no pip install, rebuild, desktop
focus or global keyboard automation. Point it at the executable you want to
test. One launch can perform several checks through the existing USB input,
teleport/debug pipe and renderer capture interfaces.

From the repository root:

```powershell
python tools/game_test/run.py --exe build/RelWithDebInfo/mm3_recomp.exe --game-dir game_files --save-template C:/path/to/save-template
```

This defaults to `driving.json`: skip boot movies, select an existing profile,
enter Single Race -> Cruise -> Paris, select the current car and automatic
transmission, hold throttle, wait for actual motion after loading/countdown,
and require measured forward motion. It saves a
presented frame for the agent to inspect. Menu transitions have short calibrated
delays; loading completion polls for a continuously live player rather than
sleeping for an assumed loading duration. USB presses must be acknowledged by
the runtime before the next action proceeds. Input acknowledgement allows up to
30 seconds, bounded by the session deadline, for the original offline network
initialization to finish; the harness does not force guest readiness.

The menu path expects a saved profile and the default initial selections. It
was exercised with Player1/Stocker on the PAL game. Different profile/menu
defaults may need a local scenario. A missing profile or a changed menu path
must fail the check, not silently count as gameplay success.

For a quick startup check (no profile needed):

```powershell
python tools/game_test/run.py --exe build/RelWithDebInfo/mm3_recomp.exe --game-dir game_files --scenario tools/game_test/boot.json --timeout 90
```

`boot.json` proves that the intro-skip marker was reached and a complete frame
was captured. It does **not** prove menus, loading completion or gameplay.
`ai-start.json` selects Checkpoint rather than Cruise and additionally requires
opponents to move without substantial reversing. No opponents is a failure,
never a passing AI test.

Use `--output-root C:/path/with/space` to place evidence/private saves elsewhere.
Use `--env RECOMP_FPS_LIMIT=0 --env RECOMP_VSYNC=0` for an uncapped diagnostic
run, or override graphics/simulation settings explicitly. Defaults use a small
960x540 window, 1x render scale and 1x MSAA for fast functional checks. The
runtime's existing DSP acknowledgement is enabled, matching the old bench.
These defaults do not establish audio fidelity or graphics-quality parity.

The CLI prints one JSON summary to stdout, with exit code 0 only for a passed
scenario, 1 for validation/runtime/check failures and 130 for interruption.
`result.json` contains each action's result, durations, child PID, executable
and XBE SHA-256, sanitized settings, cleanup method and errors. `events.jsonl`
is an incremental trace; `stdout.log`, `stderr.log`, `frames.csv` and BMPs stay
beside it. Parse the JSON and inspect the capture; do not infer success from a
process being alive, log file existing or a nonempty frame CSV.
On failure the harness also attempts a one-second frame capture if the process
is alive and the run deadline permits it; crash/deadline failures can prevent
that capture, and the reason is recorded.

## Isolation and bounds

- Every launch creates a new working directory and **copies** the supplied
  save template, including partition images and caches. Windows sparse images
  retain their holes. Save templates containing links/junctions are rejected.
  Use a quiescent template; copying an actively written save is not a snapshot.
- Only extracted game assets are referenced through a directory junction. The
  harness does not install mods or mutate assets. Use a separate asset copy
  for experiments that intentionally modify them.
- Runtime logs, captures, pad input, settings and save/cache writes live under
  the private run directory. Parent `MM3_*`/`RECOMP_*` settings are cleared;
  explicit overrides are recorded with key/token/password/secret values
  redacted. The harness owns save/input/capture paths and rejects overrides.
- The harness stops only the child process it launched. It never attaches to,
  kills or sends input to an existing game. Cleanup deliberately terminates the
  child; this does not test graceful game shutdown.
- `--timeout` bounds the run, including preparation. Pipe connection and reply
  have bounded waits with cancellation. Missing telemetry, crash/ABI markers,
premature exits and timed-out checks fail the run. Disk copies are synchronous;
  the deadline is checked before launch, not an asynchronous I/O abort.
- Successful private saves are removed unless `--keep-save` is set. Failed
  launched runs retain saves for diagnosis; failed preparations remove partial
  saves unless requested otherwise. Evidence remains in either case.

## Custom checks without another boot

Copy a JSON scenario or use the session API. Multiple `run()` calls share one
owned game process; the result contains all their steps. Any failed check must
propagate out of the context so cleanup records the failure.

```python
import json
from pathlib import Path
from tools.game_test.run import Session

scenario = json.loads(Path("tools/game_test/driving.json").read_text())
with Session("build/RelWithDebInfo/mm3_recomp.exe", "game_files",
             save_template="C:/path/to/save-template", timeout=180) as game:
    game.run(scenario)
    position = game.query("position")
    game.run({"steps": [
    {"action": "teleport", "x": position["x"], "z": position["z"]},
    {"action": "wait", "seconds": 2},
        {"action": "capture"}
    ]})
print(game.run_dir / "result.json")
```

| Action | Fields and evidence |
| --- | --- |
| `wait_log` | `text` (literal), `timeout` (default 30 s); marker observed |
| `press` | `buttons`, `hold_ms` (150), `after` (0); USB acknowledgement required |
| `wait` | `seconds`; bounded delay that still checks crashes/exits |
| `wait_player` | `timeout` (30), optional `advance` button (e.g. `start`); one primary identity continuously fresh for 1 s |
| `wait_motion` | `timeout` (10), `min_distance` (1); actual primary-player movement after loading/countdown |
| `sample` | `seconds` (5), `interval` (0.1), `min_distance` (1), `require_ai` (false), `max_backward` (10); per-car movement oracle |
| `teleport` | finite `x`, `z`, optional `heading` in radians, `tolerance` (2); native acknowledgement plus fresh position verification |
| `capture` | `timeout` (10); complete BMP, path and dimensions |
| `peek` | integer guest `address`, integer `size` (1..512); exact mapped bytes as hex |

For throttle while sampling, use `"wait_release": false` on the press step;
otherwise `press` waits through the hold and release. The runtime serializes
each pad's commands with 200 ms releases; a second press does not cancel a long
hold. Do not queue a long hold before a menu action or teleport placement test.

Movement checks require at least three samples, finite positions/heading,
stable identities and a forward-moving primary player. AI checks additionally
require opponents, motion, at most the configured backward travel and no net
reversal beyond 1 unit. Raw samples remain available even on a failed verdict.
This is a short functional regression oracle, not a handling-parity benchmark.

## Harness regression checks

```powershell
python -m unittest discover -s tests -p test_game_harness.py -v
```

These fast tests cover malformed scenarios/telemetry, missing/stalled/reversing
cars, save isolation, sparse cloning, real Windows pipes, reply/run timeouts,
crashes/exits, capture completion, secret redaction and owned-process cleanup
while an unrelated process remains alive. Their pipe server is synthetic;
passing them does not establish real gameplay. No framework or compiler needed.

Frame CSVs are diagnostic artifacts. Use populated PresentMon/Tracy traces
and matched state/captures for performance claims. BMP completeness proves
capture transport, not correct pixels; an agent must visually inspect it.
Likewise, a successful teleport verifies native placement and fresh coordinates,
not complete streamed geometry at an arbitrary destination. Allow settling and
inspect the frame for tests that depend on world rendering there.

## Local verification (2026-10-14)

The harness was tested against the existing PAL executable with SHA-256
`4bf04ffaae125d2a72fec216d2fe22963b988047b06cef0a49e68386e1a9d883`
and XBE `2b04b66c43e7f37bbcebbfb5b72ccb96a2aa99cc2c60c53eb7d3530bcc2a3d79`.
The save template was copied from the local XboxRecomp save directory; its
Player1 profile and original files were not modified. Evidence directories:

- Startup/capture: `conformance_tmp/game_test/run-odi41iyb` (8.3 s, startup only).
- Driving plus native same-road teleport in one session: `%TEMP%/mm3-game-harness-runs/run-l_3q29bp`
  (48.1 s, includes menu/loading, motion readiness, 5 s sampling and two captures).
- Checkpoint AI start: `%TEMP%/mm3-game-harness-runs/run-2o9uvn0l`
  (48.9 s, 10 s sampling, four AI opponents moved forward without reversing).

Gameplay captures were visually inspected. A teleport to a separate fixed
coordinate also returned the requested native position, but its immediate
capture had missing city geometry; that is not rendering acceptance for that
destination. The same-road test above allows two seconds to settle and retains
the rendered city. Failed calibration runs correctly reported timeouts or
insufficient motion rather than passing; `wait_motion` was added to handle the
variable initial countdown. The unrelated pre-existing game remained running.

## Winning missions (`mission_win.py`)

`MM3_MISSION=<name> MM3_SAVE_TEMPLATE=<copied save> python tools/game_test/mission_win.py` boots with all mods
on (smoothing off) and holds throttle. Names: `checkpoint` and `checkpoint-washington`, `blitz` (Washington) and
`blitz-paris`, `undercover`.

- Races read their remaining checkpoints with the `route` debug-pipe command and teleport through them; the
  script exits 0 once every checkpoint is passed and the game refuses further teleports.
- `undercover` is Paris career 00/00 "Delivery by dawn". It has no checkpoint list, so the harness parses the
  career script's zones (`ZN-0..9` in `Scripts/Paris/Career/00/00Setup.lua`, inside `Data_hd.zip`) and teleports
  into them in order.
- Inspect the captures: "CONGRATULATIONS!" for races, Mathilda's end scene and the post-mission menu
  (RESTART RACE / NEXT RACE) for undercover.

Verified wins: Paris Checkpoint (8/8), Washington Checkpoint (7/7), Washington Blitz (7/7), Paris Blitz (5/5),
Paris Delivery by dawn (10/10 zones).

Limits: the zone parser only fits delivery-style career scripts. Measured over the 54 career setups, 14 give as many
`ZN` zones as their `total_targets`; the rest use passengers, pursuits or `CP`/`WP` checkpoint races, and none of
them is reachable on a save that has not unlocked it (the Work Undercover menu lists them locked).

## Profiling the game thread (`profile_thread.py`)

`MM3_SAVE_TEMPLATE=<copied save> python tools/game_test/profile_thread.py [seconds] [fps_limit]` samples the
game thread through the debug pipe's `profile` command (every millisecond, no elevation needed). It prints the
share of time in translated code, in ntdll (blocked), the hottest exe code, the exe functions the thread is blocked
under (the first exe frame of its stack, named through the pdb), and the game process's CPU use in cores.

First readings, uncapped Checkpoint race at the harness settings: about 30% of the game thread's samples are in the
exe and 70% in ntdll waits, with no exe address above 1.2%, so translated guest code has no hot spot. The waits were
`nv2a_d3d_repeat_frame` (47%, a `SwitchToThread` spin while the presenter shows the repeated frame),
`xbox_Nv2aWbcFlush` (15%, a real wait for the GPU executor) and `xbox_GuestMapped`/`xbox_GuestRead` (5%, memory
probes). Making the frame-request wait sleep on an event and caching `xbox_GuestMapped` took the process from
2.72 to 2.20 CPU cores for the same run.


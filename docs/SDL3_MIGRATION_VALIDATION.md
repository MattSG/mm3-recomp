# SDL3 migration validation

Validated on Windows on 2026-11-01, followed by a manual Cruise check.

The full SDL3 migration from `archive/stale-base-sdl3-20261031` (`2487d718`)
was ported onto the newer `259c3164` runtime. It retains the DSP
interpreter/recompiler, native NV2A executor, kernel and current input support.
SDL3 supplies host graphics, input and audio; FFmpeg supplies video/WMA decoding.
Published runtime commit: `0acefa1b642d6362e4a8b0c3b5e7e8c72d9aced4`.

## Automated checks

| Suite | Result |
|---|---|
| Native toolkit/MM3 aggregate | 490 passed |
| Additional SDL GPU helpers | 4 passed |
| Separate D3D8 fixtures | 5 passed |
| Vulkan reruns of GPU helpers and D3D8 fixtures | 9 passed |
| Toolkit Python | 966 passed, 23 skipped, 160 subtests |
| Root Python | 147 passed, 4 skipped, 122 subtests |
| MM3 regression scripts | 37 passed |
| Instruction conformance | 6368 vectors, zero mismatches |
| Function conformance | 211 vectors, zero mismatches |
| Harness selftests | 12 passed |
| Builder unit tests | 5 passed |

The 499 distinct native checks ran with assertions enabled. Shader-helper CMake
configurations with and without a caller override passed, as did the launcher
settings write/read contract. Python compilation checks used the repository venv
and MSVC environment. Subsequent README/builder cleanup reran the five builder
tests and the builder self-test.

## Gameplay

Harnesses used explicit extracted assets and private copies of an isolated save.
All runs used the same executable, SHA256
`7a6cb55d859bf9f7066d5f8776977f1005e4c0dc8e0a52f75ebd78a87eebdbe4`.
At 960x540, render scale 1, MSAA 1 and a 60 FPS cap:

- Cruise travelled 139.85 units in five seconds; world/HUD capture inspected.
- Four AI opponents travelled 379.75-401.39 units in ten seconds; the player
  travelled 321.84 units. The early race capture was inspected.
- Paris Checkpoint completed 8/8 and Washington Checkpoint 7/7.
- Paris Blitz completed 5/5 and Washington Blitz 7/7.
- Paris Undercover completed all ten delivery zones and reached Next Race.

Mission completion used native teleport commands to traverse checkpoints or
zones. Congratulations screens and the Undercover results transition were
inspected. The city-selection harness was corrected to select Paris explicitly
instead of relying on the copied save's Washington default. A separate manual
Cruise session was left running and the user confirmed that it looked good.

## Reproducing and interpreting the checks

Use `tools/game_test/run.py` with `driving.json` and `ai-start.json`, supplying
`--exe`, `--game-dir` and `--save-template`. `mission_win.py` documents the five
mission variants. Harness setup, telemetry and captures are described in
[the harness README](../tools/game_test/README.md).

Toolkit tests live under `tools/xboxrecomp/tests` and its Python `tools` tree;
root fixtures and Python regressions live under `tests`. Local aggregate build
scripts, logs, executable hashes and captures were retained under
`conformance_tmp`; these machine-specific artifacts are not shipped.

No regressions were found in these Windows checks. Linux/ARM/POSIX execution,
physical audio fidelity, live two-machine System Link, unbounded play, graceful
guest shutdown and broad graphics-setting/hardware parity remain unproven.
These paced gameplay runs are not performance benchmarks. Dummy audio and media
contracts establish API/decoder behavior, not perceived sound quality.

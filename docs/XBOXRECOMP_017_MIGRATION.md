# XboxRecomp 0.17 migration

Upstream: [v0.17.0, Empty Promises](https://github.com/sp00nznet/xboxrecomp/releases/tag/v0.17.0),
commit `343b6d8901723b37ce568b2418c5df4e1fe1d223`.

The migration is isolated at `I:/repos/mm3-v017-migration` on
`migration/xboxrecomp-v017`. Its toolkit uses `migration/v017-rebase`.
The original checkout and its ongoing local changes were preserved.
Local work present when the migration snapshot was taken has separate preservation commits.
Later concurrent edits in the original checkout are outside that snapshot.

The toolkit's effective fork changes were consolidated onto the upstream release.
Replaying only its final SDL3 commit would omit earlier runtime changes, because
its history contains a history-only merge and a later tree-restoration commit.
The old fork branches remain available; no remote history was rewritten.

## Changes required by 17.0

- Regenerate analysis, ABI metadata and every translated body. The new lifter
  preserves flags across relevant calls, masks narrow arithmetic/shift operands,
  lifts save/restore instructions, retains general-purpose EBP, and excludes
  unreachable instruction streams.
- Keep the existing SDL3/FFmpeg host implementation and generic memory/timer
  contracts while integrating upstream fixes. Existing fork memory ownership
  and validated POSIX handles supersede the older implementations those patches
  were based on.
- Remove the legacy `g_seh_ebp` initializer which overwrote 17.0's architectural
  `g_ebp` input in frameless functions. A compiled regression distinguishes the
  two inputs and verifies direct, indirect and sibling calls. External tail
  jumps and fallthroughs also publish architectural EBP, rather than only the
  old SEH bridge. Compiled jump/conditional/fallthrough tests cover stale
  global input after a helper has changed the local frame. This was verified
  after an all-mods startup crash exposed the CRT shared-tail boundary.
- Match the new EBP return publication in the optimized MM3 visibility leaf,
  update the collision-call source anchors, and adapt fixtures to the regenerated
  return sequences and the removed artificial graph-tail function boundary.
- Preserve the fork's vertex-program write rules in the new decoded runner,
  publish constant changes, and invalidate decoded instructions on frame restore.
  The existing randomized comparison and an added restore regression cover this.
- Adapt timer buffer validation to the fork's explicit read/write argument.
- Preserve the corrected high-word EEPROM video flags, explicit off semantics
  for `RECOMP_VBLANK=0`, and new kernel/tooling changes from upstream.

## Reproduction

Use the repository Python environment and Visual Studio's CMake on Windows.
Run `python scripts/generate_mm3.py` for the full pipeline. A subsequent
translator-only correction was regenerated with `--skip-analysis`; that reuses
its unchanged, freshly generated analysis and regenerates every C body, overlay,
runtime entry and dispatch table. Generated game code and extracted assets remain
local, as before.

Build with `cmake --build build --config RelWithDebInfo --target mm3_recomp`.
The all-mods validation driver and its manifest are retained at
`build/v017_all_mods.py` and `build/v017-all-mods-final/suite.json`.
Every registered mod is explicitly on (the teleport pipe is owned by the harness),
with `MM3_SMOOTH=0` and legacy render-every-frame disabled. The replacement timing
build does not activate legacy interpolation. The developer HUD is also exercised
in Cruise. Developer probe assets and each run's writable saves are private copies.

## Validation

- Toolkit Python: 1,024 passed, 21 skipped, 160 subtests. This includes
  compiled EBP call and tail-entry regressions on MSVC.
- Game Python: 157 passed, 5 skipped, 110 subtests; harness selftests: 12 passed.
- Game native regression scripts: all 39 passed against the final generation.
- Instruction conformance: 6,368 vectors, zero mismatches.
- Collision-call seam: all three call sites and 22 rejected anchor variants pass.
- Vertex-program reference/decoded runner: 2 tests pass, including saved-frame
  restore and randomized programs. Fixed-function/presentation: 3 pass.
- Native mouse/mod initialization: 3 pass. Kernel bridge/critical-section
  defaults and 16 additional thread, timer and boundary modes pass.

All seven final scenarios passed with all 14 registered mods enabled and interpolation disabled.
Gameplay and result captures were visually inspected.

| Scenario | Acceptance evidence |
| --- | --- |
| Cruise | 369.09 units of player movement over a 30-second sample; developer probes loaded |
| AI start | Four AI cars moved 624.21-667.38 units over 15 seconds |
| Paris Checkpoint | 8/8 checkpoints; Congratulations screen |
| Washington Checkpoint | 7/7 checkpoints; Congratulations screen |
| Paris Blitz | 5/5 checkpoints; Congratulations screen |
| Washington Blitz | 7/7 checkpoints; Congratulations screen |
| Paris Undercover | 10/10 delivery zones; debrief and Next Race menu |

All runs used the same executable, reached no unresolved generated stub, and
were stopped by their owning harness after acceptance. Cruise and AI input are
bounded throttle checks and include collisions, rather than continuous clean driving.

Final toolkit: `fc51b71a6b8368b54397828cea7209c2ae437cd4`.
Game source: `43ec816c6accd55989e6baec4af5b38c9428d16b`.
Executable SHA-256: `96bbdc31206bd803a108190c5978e82642fd8ccfbe4297213af493d0fa856246`.
Full regeneration produced 50 C sources, 25,948 translated functions and zero
translation failures (26,022 total functions). The remaining reported stubs are
`sub_004775E0` and `sub_6E7D169A`.

The optional automatic XBE conformance pass executed zero function vectors and
is not counted as a passing comparison; the game-specific differential
contracts above compare against actual XBE instructions.
Detailed logs and manifests are retained under `build/v017-validation`.
The translator reports two unresolved stub addresses; acceptance runs enable
stub logging and reject a reached stub. Zero translation failures does not mean
every reported stub or instruction has been proven by gameplay.


These Windows checks do not establish Linux/POSIX execution, physical audio
fidelity, two-machine System Link, unbounded play, or parity on every GPU/settings
combination. Mission teleport checks establish native progression, rather than
normal driving through an entire event.

## Publication

Published toolkit main joins the old SDL3 main history with the validated 17.0
migration using an explicit history merge. Its source tree is byte-for-byte
identical to the tested toolkit `fc51b71a`; publication commit is
`d0ab118819932a54b2d4907fbf51393c3ff353a0`.
The game main includes `feat/profile-thread` and the migration commits.
Occupied old local main worktrees were retained on
`archive/main-before-v017-publish`, preserving their uncommitted work.
Historical experiment, audit, backup and PR branches were retained; they are not
all assumed completed merely because a branch exists. The existing `sdl3-gpu-linux`
branch predates the tested host migration and is retained for comparison.

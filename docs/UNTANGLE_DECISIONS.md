# MM3 structural refactor decisions

Scope: MM3 game repository only. No subagents. No optional comment or test
cleanup. XboxRecomp and car-tools remain unchanged. Baseline: `7e345bf`.
The original working checkout contains concurrent changes; this work uses
`I:/repos/mm3-v017-migration`.

## Preservation boundary

Keep guest function names, ABI, generated instruction bodies, mod identifiers,
defaults, environment parsing, initialization order, public entry points,
settings files, extracted assets and gameplay behavior. Adapt test source paths
and build inputs when ownership moves; keep assertions and cases intact.

## Decisions and supporting problems

### Separate mod metadata from game initialization

- `src/mods/mods.c` currently owns both the metadata/settings cache and runtime
  startup wiring. The launcher consumes only the metadata, but linking it also
  requires renderer, DVD, teleport and input entry points.
- `tests/mm3_launcher_settings_contract.cpp` supplies unrelated fail-fast stubs
  because of that link dependency. Keep those assertions; do not perform the
  optional test cleanup the user excluded.
- Keep metadata and `mm3_mod_on` in `mods.c`; move runtime initialization and
  player-update wiring together to `mods_runtime.c`. Keep the public interface
  and the exact initialization sequence unchanged.
- Verify the existing mod-startup, launcher-settings and pause/UI regressions
  before and after the move, then link the game and actual settings contract.

### Give generated-code integration one build entry point

- The root build mixes UI dependencies, game source declarations, generated
  function ownership discovery, source transforms and focused checks.
- Hook discovery and transformation must change together, including strict
  failure checks, configure dependencies and copied source properties.
- `mm3_prepare_generated_sources` owns the hook declarations, discovery,
  validation and transformation in one function. Its explicit generated-directory
  input and returned source list replace the root build's temporary variables.
- Existing build-reading regression checks now inspect that module as well as
  the root build. Every assertion and case is retained.
- Verified the same 105 compile items with identical per-source metadata and
  byte-identical output for all 16 transformed chunks after reconfiguration.

### Keep performance profile values in one native/tooling data table

- `src/mm3_performance_profile.h`, `scripts/play.py` and `tools/bench/run.py`
  repeated the same Balanced, Quality and LowPower settings. A native change
  could silently leave launch metadata and benchmark presets describing another
  configuration.
- `src/mm3_performance_profiles.def` now owns those rows. C includes the rows
  as initializers; the Python tools load the same six-string row format without
  executing code. The schema is fixed to these profile fields.
- Preserve native fallback behavior, case-insensitive names, independent
  environment overrides, benchmark field names and numeric FPS values.
- Existing native profile and launch subprocess checks remain; add focused
  validation for the new data reader's malformed/duplicate rows.

## Retained structure and special cases

- Geometry helpers are small independently tested algorithms, not pass-through
  wrappers. Keep their current interfaces and folder ownership.
- Guest-address entry points, SEH/x87 register preservation and strict generated
  call anchors encode the game's ABI. Keep them rather than inventing a generic
  dispatch layer or weakening anchor failures.
- Cache timestamp buckets, collision repair, lamp-pass recognition and native
  map/HUD roles encode measured title behavior. Keep those rules game-owned.
- Legacy timing is an explicitly selectable comparison implementation; it is
  not dead code. Its fixed-step helper and the replacement tick clock express
  different time policies, so consolidating them would couple separate behavior.
- SDL and D3D fixture paths retain comparison coverage. Changing the toolkit's
  renderer selection or deleting comparison paths is outside this scope.
- Harness-managed saves, process ownership, bounded waits and pipe requests are
  required isolation/recovery behavior, not sleeps to delete mechanically.
- The builder imports extraction and setup policy from their owning scripts;
  runtime launch logging uses the toolkit's reusable process support. Keep those
  existing dependencies instead of copying their implementations into MM3.
- Feature metadata, native mod logic and authored shaders remain game-owned;
  no code moves into either submodule.

## Whole-repository assessment coverage

- Build and generated-code integration.
- Launcher/UI and startup/settings.
- Native game replacements and geometry helpers.
- Mods, render hooks, timing and developer controls.
- Generation, extraction, setup, launch and builder scripts.
- Game harness, benchmarks, debug-feature preparation and project test wiring.

Structural skeletons and dependencies for 117 first-party source/tooling files
are recorded in `build/untangle/source-atlas.json`. The repeated-body scan found
no exact repeated nontrivial function bodies among those files. This supports
the assessment; it does not replace reading the boundaries above or testing.

The solo second pass rechecked the selected boundaries, import direction, source
ownership and retained cases above. No further structural change was supported
by a concrete co-change problem in this pass. Final verification is recorded below.

## Verification

- Mod-startup, launcher-settings and pause/UI checks passed before the move.
- All 39 existing native regression scripts pass after the refactor.
- Game Python: 159 passed, 5 skipped, 113 subtests; harness self-tests: 12 passed.
- Registry links and runs with only `mods.c` and `feature_switch.c`, without
  renderer, input, UI or game-startup stubs.
- All 105 compile items retain their source metadata. All 16 transformed chunks
  retain identical bytes. The startup body and generated-code recipe compare
  verbatim to the baseline, allowing only CMake function indentation.
- Benchmark presets retain every original value and integer FPS representation.
- Native game and launcher settings contract build successfully. The actual
  launcher contract passes both save/export and fresh-process reload checks.
- Native mouse/mod initialization CMake checks: 3 passed.
- All-mods driving: 140.42 units over 15 seconds; interpolation disabled,
  no unresolved stub reached, capture visually inspected. The harness cleaned
  up only its child. Evidence: `I:/repos/mm3-v017-migration/build/untangle/gameplay/run-9aa0m_6y`.
- Accepted executable SHA-256: `2eb1b90fffc5c454ff51f813f2bc81ea0d7f75ba80ddaa5b2e1a63fbcc8eaa76`.
- The toolkit and car-tools working trees and submodule pins remain unchanged.
  No optional comment/test cleanup was performed.

This is bounded Windows verification. It does not establish Linux behavior,
physical audio fidelity, two-machine networking or every mission/settings combination.
The prior seven-scenario migration acceptance was not repeated in full.

Evidence is retained under `build/untangle`. No test cases or assertions were
removed. Source-reading checks and subprocess fixture copies were updated for
moved ownership; the two new tests cover the new profile-data reader.

Keep required guest-address cases, ABI seams, retries and compatibility behavior
unless the structural redesign proves a replacement preserves their purpose.

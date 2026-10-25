# XboxRecomp ownership

`tools/xboxrecomp` is our fork of sp00nznet/xboxrecomp: upstream `main` in
full, with only generic runtime changes layered on top. It holds documented
Xbox hardware/XDK behaviour and reusable host mechanisms, and nothing that
names this title.

This repository owns everything specific to Midtown Madness 3: guest
addresses, game classifiers, rendering policy, gameplay/timing/HUD hooks,
fixtures and tools. It reaches the runtime only through the fork's generic
hooks, never by patching runtime sources.

- `src/` and `src/mods/`: the port and its mods.
- `src/runtime/`: helpers only this port uses (camera/road geometry, guest
  reads, the launcher's RmlUi host, crash report, write watch, pushbuffer
  reserve/recovery, WBC flush). They left the fork because no other title
  needs them.
- `src/mm3_recomp_types.h`: the title layer over the runtime's
  `recomp_types.h`, including `recomp_cpuid` for the pinned generator.

The audit ledger and source inventory of the previous fork (95a3ec5) are in
git history before this file's rewrite.

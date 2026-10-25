# XboxRecomp ownership

`tools/xboxrecomp` is upstream sp00nznet/xboxrecomp, pinned to a release.
`patches/xboxrecomp` holds the generic runtime changes this port needs on top
of it, one area per patch, applied by `scripts/patch_xboxrecomp.py`. They hold
documented Xbox hardware/XDK behaviour and reusable host mechanisms, nothing
that names this title, so each can be offered upstream.

This repository owns everything specific to Midtown Madness 3: guest
addresses, game classifiers, rendering policy, gameplay/timing/HUD hooks,
fixtures and tools. It reaches the runtime only through generic hooks.

- `src/` and `src/mods/`: the port and its mods.
- `src/mm3_recomp_types.h`: the title layer over the runtime's
  `recomp_types.h`, including `recomp_cpuid` for the pinned generator.

The audit ledger and source inventory of the former MattSG fork are in git
history before this file's rewrite.

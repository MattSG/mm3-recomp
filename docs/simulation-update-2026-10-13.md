# Simulation integration with Matt's October 2026 updates

The all-features branch integrates Matt's updates through `b58c022` with the
fork's `a7ae061`.
Both versions schedule the world update from the timer sampled during the
current frame. The fork had already corrected the earlier startup-only clock
in `c1a6adc`; startup state entry runs once and menu/input states still run
every render frame.

The merge keeps the fork's double-precision accumulator, independent
`MM3_SIM_TRACE` diagnostics and timer validation. Zero, backward and invalid
deltas add no simulation time. A long hitch runs at most four world ticks,
then drops excess time. Each world tick sets the raw double, clamped float
and averaged float timer fields to the same simulation step, restoring their
original values for rendering. `MM3_SIM_HZ` remains 30 by default or 60 when
explicitly selected.

Matt's latest render hook suppresses drawing when the main-loop world update
runs zero ticks, preserving the Xbox's two-argument render return convention.
It waits one millisecond instead of presenting the same state again. This
avoids upstream's duplicate-draw menu-text and tail-light glitches. The game
has no interpolation: with this mod enabled, drawing follows simulation ticks
at up to 30 or 60 FPS even when the frame-limit setting is 90. Other render
callers and disabling the mod retain their original behavior.

`MM3_RENDER_EVERY_FRAME=1` (the launcher's **Draw every frame** option) restores
the fork's previous drawing cadence. It draws duplicate world states between
simulation ticks and leaves simulation timing unchanged. The default is off;
unset, zero and invalid values keep Matt's tick-dependent drawing.

Matt's definition-only hook transformation now applies to every project
wrapper. Generated callers in the same chunk still enter those wrappers.
The transformed copies live under the build's `hooked_chunks` directory and
retain the fork's collision, fence, DMA, dashboard, map, guidance and tree
seams, source properties and regeneration dependencies.

Validation passed with MSVC `/W3 /WX`: 35 timing/ABI scenarios and 24 actual
generated-loop scenarios, including 30/60 Hz simulation through 240 render
frame-loop FPS, irregular frames, disabled hooks, invalid timers, guest
stack/register contracts, tick-dependent draws and diagnostic flushing. The
configured-build preprocessing check also confirms that main-loop calls reach
the wrappers. Negative controls reject a fixed tick per render, drawing a
zero-tick frame and whole-chunk world/render hook renames.

For an isolated checkout, `tests/mm3_simulation_regression.py` accepts
`--generated-dir` to read generated functions from another checkout. The
configured-build test discovers its generated headers from `CMakeCache.txt`.
Both checks can run without modifying that source checkout.

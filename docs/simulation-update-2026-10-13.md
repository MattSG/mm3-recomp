# Simulation integration with Matt's October 2026 updates

The all-features branch merges Matt's `9494613` into the fork's `a7ae061`.
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

Matt's definition-only hook transformation now applies to every project
wrapper. Generated callers in the same chunk still enter those wrappers.
The transformed copies live under the build's `hooked_chunks` directory and
retain the fork's collision, fence, DMA, dashboard, map, guidance and tree
seams, source properties and regeneration dependencies.

Validation passed with MSVC `/W3 /WX`: 18 timing/ABI scenarios and 16 actual
generated-loop scenarios, including 30/60 Hz simulation through 240 render
FPS, irregular frames, disabled hooks, invalid timers, guest stack/register
contracts and diagnostic flushing. The configured-build preprocessing check
also confirms that main-loop calls reach the wrappers. Negative controls
reject a fixed tick per render and a whole-chunk hook rename.

For an isolated checkout, `tests/mm3_simulation_regression.py` accepts
`--generated-dir` to read generated functions from another checkout. The
configured-build test discovers its generated headers from `CMakeCache.txt`.
Both checks can run without modifying that source checkout.

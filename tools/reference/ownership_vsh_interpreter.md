# Vertex interpreter ownership and constant publication

Full nv2a_vsh_interp.c/header source read at fork 44acaede094d0390f5e6c445926eedb2519d5a13. Instruction
uploads, register state, arithmetic, masked outputs and snapshot/version APIs
are generic NV2A mechanisms. The program and constants come from callers;
coordinate-space policy stays with the uploaded program and consumer.

The debug dump selected only c96-c111 and labeled that subset as matrices.
It now dumps all constant registers with VSH_CONST labels. Existing accessors
let applications select ranges themselves. Captured program-recognition and
specific epilogue failure notes moved to legacy_vsh_interpreter_observations.md.

Context-enabled interpreted writes modified constants without incrementing
the published version, unlike upload/direct setters. The shared masked-write
branch now advances the version for valid writes. The actual interpreter
regression fails constant-publication checks before the fix, then passes on
Windows and Linux, including unchanged masked components and no version update
for disabled/out-of-range writes. The existing epilogue calculation remains
covered; both D3D11 and SDL GPU game builds pass.

Remaining fidelity gaps: reserved opcodes and source selectors have implicit
fallbacks, ARL rounding/overflow and float exceptional cases need independent
hardware evidence, global state needs external serialization, counters wrap,
and direct state restore requires trusted snapshots. Full instruction-set,
cache-consumer behavior or gameplay fidelity is not certified here. Source
remains needs_generic_fix outside this publication correction.

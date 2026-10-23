# NV2A fault adapter ownership and decoder consolidation

Full nv2a_mmio_hook.c/header source read at fork dd07ff368211dacd440bfb33b68eac8816468e00. The hook had a
second instruction decoder with the same SIB/immediate/high-byte/flag defects
already fixed in the generic decoder. It now forwards device read/write
callbacks to the shared bounded snapshot decoder. PVIDEO locking remains in
the surrounding handler. Statistics use interlocked increments and count actual
read/write callbacks. Failure logs print context metadata without reading RIP.

The prior hook supports reverse CMP (3A/3B), so those operations were added to
the shared implementation rather than dropped. Forty expected-value CMP edge
cases now cover both directions and four widths. An actual-hook fixture mocks
only the register model and reproduces five failures before consolidation;
word immediates, AH, SIB, reverse flags and unreadable logging pass afterward.
Two focused CTests pass Windows/Linux, and both game builds pass.

Stale extraction notes and zero-page/VRAM alias claims moved to
legacy_mmio_decoder_notes.md. Header failure guidance now agrees with the
inspected host handler: unsupported faults continue searching, not fabricated
zero registers. The remaining hook cleanup is recorded below.

## Initialization and placeholder cleanup

Fork dd80d047e8bed2ae6246c860d32ae8c671dd1520 removes the unused VRAM fault helper and archives its
implementation in legacy_nv2a_vram_helper.md. It allocated unrelated writable
host pages rather than GPU aliases. Existing guest mappings stay owned by the
memory-layout layer. MMIO entry rejects addresses outside its register range.

Initialization preserves an existing process singleton, frees newly allocated
backing if state initialization fails, and allows retry. Both host graphics
branches reject failed GPU initialization. Startup must be quiesced; successful
backing remains for process lifetime, with no teardown API.

The old initializer fails three lifecycle assertions. Final decoder/actual-hook
adapter tests pass 2/2 on Windows and Linux; the Windows actual I/O-mapping test
passes with zero failures. Both D3D11 and SDL GPU game builds pass. These checks
do not certify GPU register semantics, live gameplay, concurrent initialization
or hardware equivalence. The wider ownership audit remains incomplete.

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
zero registers. The legacy VRAM helper still allocates unrelated host pages,
not a modeled physical alias, and repeated GPU init/shutdown lifetime needs
review. Source remains needs_generic_fix for those paths. No live fault delivery,
real GPU model correctness, full architecture or gameplay proof is claimed.

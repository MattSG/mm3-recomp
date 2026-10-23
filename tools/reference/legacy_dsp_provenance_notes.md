# MCPX DSP source provenance

Upstream: https://github.com/xemu-project/xemu

Pinned commit: `478b4f496102379c7eaa7f3ec10e714a703c4300`.
Original directory: `hw/xbox/mcpx/apu/dsp/`.

## Incorporated source

| Local files | Upstream license |
| --- | --- |
| `interp/dsp_cpu.c`, `interp/dsp_cpu.h`, `interp/dsp_cpu_regs.h` | GPL-2.0-or-later |
| `interp/dsp_emu.c.inc`, `interp/dsp_dis.c.inc`, `debug.h` | GPL-2.0-or-later |
| `dsp_dma.c`, `dsp_dma.h`, `dsp_dma_regs.h` | LGPL-2.0-or-later |
| `dsp.c` (adapted from upstream `dsp.c`/`dsp_c.c`) | GPL-2.0-or-later |
| `../apu_dsp.c` (adapted from upstream `gp_ep.c`) | LGPL-2.0-or-later |

Original copyright and license headers are retained. The interpreter credits
espes, Matt Borgerson, the ARAnyM developers and Thomas Huth's Hatari adaptation.
The original upstream license texts are included as `COPYING.GPL-2` and
`COPYING.LGPL-2.1`; the latter is the upstream repository's supplied LGPL text.
Per-file headers specify the license versions applicable to each file.

These files are not covered solely by xboxrecomp's top-level MIT license.
The GPL interpreter's redistribution terms must be accounted for when shipping
a linked binary. No JIT, QEMU machine/device runtime, or full APU was imported.

## Adaptation

`dsp_port.h` replaces QEMU host includes with standard C support and explicit
little-endian reads/writes. QEMU tracing is disabled; instruction behavior is
retained except for explicitly documented ISA corrections below. Regression
tests build independently of the game's host runtime.

The previously missing BCHG/BCLR/BSET/BTST `qq` forms now share the existing
short-I/O handlers with address-bank selection. Tests execute all 64 X
addresses in both banks, bits 0-23 and both initial bit states: 24,576
cases checking destination value, Carry, unchanged remaining SR bits,
PC advance and callback counts. Related BCHG/BCLR/BTST memory decode masks
now accept the fifth bit-number bit instead of rejecting bits 16-23.
The DSP56300FM Rev. 5 specifies bits 0-23 and the two I/O address banks,
but some printed opcode diagrams incorrectly show only four bit fields.
Five-bit encodings are corroborated by the independent emulator's opcode
definitions (reference only; no implementation code incorporated):
https://raw.githubusercontent.com/dsp56300/dsp56300/master/source/dsp56kEmu/opcodeinfo.h
Manual: https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf
The unchanged pinned xemu reference does not implement the new qq forms;
these extensions are specification-based tests, not upstream equivalence
claims. Y-space I/O mapping and qq disassembly remain unimplemented.
All nine Release checks and the captured real GP/EP frame replays pass.
BRCLR/BRSET `qq` forms also share the existing relative bit-branch handlers
with bank selection. Another 49,152 cases cover both X I/O banks, every
address/valid bit, both bit states and four displacements (self, forward,
backward and negative displacement to address zero), checking taken and
untaken PC behavior, read-only access and unchanged SR. Semantics follow
DSP56300FM Rev. 5 section 13, BRCLR/BRSET. The pinned reference still lacks
these qq forms, so their coverage is specification-based. All nine Release
checks pass; real captured GP/EP frames continue to match exactly.
BRCLR/BRSET absolute-short (`aa`) forms now use the same handlers with
absolute address selection. The branch sweep expands to 98,304 cases:
the two X I/O banks plus both X/Y absolute-memory spaces, with every short
address, bit and condition/displacement combination described above.
Absolute-memory cases also check that neither source memory nor peripheral
callbacks are modified. These forms follow DSP56300FM's documented
absolute-short branch semantics; the unchanged pinned interpreter lacks
them.
The seven indirect effective-address BRCLR/BRSET modes now reuse
`emu_calc_ea`: post-decrement/increment by N, post-decrement/increment by
one, indirect, indexed and pre-decrement. A 448-case check covers all eight
address registers, both memory spaces, both instructions and bit states.
It verifies the address update occurs even when the branch is untaken,
indexed mode preserves Rn, source memory/SR remain unchanged, and the
negative displacement and fall-through PCs are correct. Extended-address
mode 6 remains explicitly unsupported pending confirmation of its
extension-word layout; this is not a claim of complete addressing support.
Y-memory long-offset MOVE now shares the X-memory handler with memory-space
selection. Its 1,024-case regression covers both spaces, all eight offset
registers, eight data/accumulator-part registers, loads/stores and zero,
positive and negative displacements, checking the other space, Rn, SR and
two-word PC advance. This test also exposed an existing decoder collision:
JCLR/JSCLR/JSET/JSSET EA matchers accepted MMM=6, selecting a jump instead
of long-offset MOVE. Those matchers now accept indirect modes only, removing
the overlapping encoding. This is a port correction beyond pinned upstream;
no third-party implementation code was incorporated. Full instruction timing,
remaining register combinations and relative-branch extended-address layout
still require audit. DSP56300FM Rev. 5 sections 13-118/13-120 describe the
long-offset memory MOVE encodings. All nine Release checks pass.
Immediate OR (short) and EOR/XOR (short/long) now use the same logical-ALU
helper as existing immediate AND and long OR, removing the duplicate OR
implementation. A 2,520-case sweep covers all short immediates, selected
long values/sign/zero boundaries, both accumulators and all three operations.
It verifies A1/B1 results, preservation of A0/A2 or B0/B2, N/Z updates,
V clearing and existing remaining-SR behavior, plus one/two-word PC advance.
DSP56300FM Rev. 5's EOR section specifies the accumulator slice and unsigned
short-immediate alignment. S/L standard-definition behavior and instruction
timing still require independent audit; these tests preserve the existing
logical helper's flag model rather than claiming full ISA conformance.

DMA advances one descriptor at each scheduled DSP instruction boundary after
START; completion follows an actual transfer reaching EOL. Reading `DMA_CONTROL`
does not change state; the upstream three-read
completion counter was removed. Transfer staging buffers are local to each
descriptor and freed, replacing upstream static shared storage which leaked
on growth. These changes need comparison with xemu for guest-visible timing.

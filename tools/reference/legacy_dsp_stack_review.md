# DSP56300 stack extension implementation notes

Source: local DSP56300FM Rev. 5, `conformance_tmp/DSP56300FM-cache-review.pdf`.
This is an implementation gap, not a completed feature or runtime proof.

## Established architectural rules

- OMR SEN bit 20 enables extension; XYS bit 16 selects X (clear) or Y (set).
- OMR WRP bit 19 becomes sticky when hardware-to-memory copying begins.
- EOV bit 18 sets on a push requested with SP equal to SZ; EUN bit 17 sets on a pull requested with SP zero. Each flag's clear-to-set transition raises the nonmaskable stack-error exception. Both clear on hardware reset or an explicit OMR MOVE.
- With SEN enabled, SP is a 24-bit logical counter. Its low four bits still select the physical top. Bits 4/5 are counter bits, not non-extended SE/UF flags.
- SC is a separate five-bit count of hardware entries. Hardware reset clears it; implicit stack operations and explicit MOVEC reference it.
- SZ is not initialized by hardware reset. Software initializes it before enabling extension. SZ is 15 plus half the extension buffer size in 24-bit words.
- EP is a 24-bit pointer into extension data memory.

Sources: 5-6/5-7 (PDF 86/87), 5-18 through 5-20 (PDF 98-100).

## Transfer thresholds and ordering

The general description says a push is always into the hardware stack, followed by moving least-recently-used entries to memory when full. A pull takes the physical top, followed by loading older entries from memory when empty. Transfers contain one or two 48-bit entries (two or four 24-bit words).

The timing appendix gives more precise thresholds: SC=14 is declared full; an additional push activates extension. SC=2 is declared empty; an additional pop activates extension. Do not implement a naive SC=16/SC=0 trigger.

Sources: 5-17 (PDF 97), A-11 (PDF 435). The exact high/low word address order, EP increment/decrement order and single-versus-double-operation batching are not yet established by these excerpts. Resolve them from a device specification or independent implementation before claiming memory-level conformance. A self-consistent round-trip test alone cannot prove these choices.

## Valid-program restrictions

While SEN is enabled, MOVE to EP and BCHG/BSET/BCLR on EP are prohibited. MOVE to SC with a value greater than 15 is prohibited. Subroutine call instructions must not occupy the stack-error vector while extension is enabled. Documented SSH read/write pipeline hazards require two NOPs before affected following stack reads.

Sources: A-22/A-23 (PDF 446/447).

## Current port and remaining work

EP/SZ guest storage, compatibility widths and SZ reset preservation exist. Non-extended stack-error routing, repeat-fault recovery, SE transitions and UF latching have executed regressions. Explicit SSH/SSL move widths have register and X/Y-memory coverage.

SC storage/accounting, extended logical SP, spill/refill, WRP/EOV/EUN behavior, transfer timing and memory-order verification remain unimplemented. The current 16-entry non-extended helpers must not be presented as extended-stack support. Existing capture layout has spare register slots, but introducing SC accounting changes state interpretation: decide capture compatibility explicitly and regenerate/version fixtures where needed.

Required proof includes more than 16 nested actual guest calls and returns in X and Y extension memory; exact spill words and EP/SC/SP at thresholds; double pushes/pops from loops; sticky fault/rearm behavior; reset; and identical-program/input comparison against an independent extension implementation where available.

## Reference investigation, 2026-10-08

Pinned xemu `478b4f496102379c7eaa7f3ec10e714a703c4300`, `hw/xbox/mcpx/apu/dsp/interp/dsp_cpu.c`: stack push/pop only inspect four physical pointer bits and non-extended SE/UF, with no SEN, SC, EP or extension memory transfers. It cannot establish extended-stack memory ordering.

The inspected independent [dsp56300/dsp56300 interpreter](https://raw.githubusercontent.com/dsp56300/dsp56300/master/source/dsp56kEmu/dsp.cpp), functions `incSP`/`decSP`, increments/decrements SP and SC and asserts physical array bounds. Its [SSH/SSL helpers](https://raw.githubusercontent.com/dsp56300/dsp56300/master/source/dsp56kEmu/dsp.h) access the physical array directly. These inspected paths contain no memory spill/refill; do not treat agreement with them as extension validation. The mutable master URLs are investigation pointers, not a pinned test dependency. No code was incorporated from this GPL-3.0 project.

[NXP's current DSP56300FMAD Rev. 6](https://www.nxp.com/docs/en/reference-manual/DSP56300FMAD.pdf) updates the PLL diagram and ASL/ASR syntax, not stack extension. It does not resolve EP or word order. Next evidence to seek: older manufacturer's extension addendum, compiler task-context save/restore documentation, or the manufacturer's simulator with a known uploaded nested-call program and inspected extension memory. Preserve the unresolved ordering gap until actual evidence is available.

Rev. 5 section 4.3.2 establishes EP as the next available location past the last stored item, and says hardware reset leaves it uninitialized. EP reset preservation is now implemented. Earlier Rev. 2 addendum only clarifies internal X/Y extension location; it supplies no word order. Manufacturer application note APR20 section 3.3 is the next primary source to inspect: https://www.nxp.com/docs/en/application-note/APR20.pdf. Its summary uses a different EP description, so inspect the complete transfer explanation before choosing pointer semantics.

## Transfer-order evidence, 2026-10-08

[NXP APR20 section 3.3, pages 3-8/3-9](https://www.nxp.com/docs/en/application-note/APR20.pdf) confirms a push at SC=14 keeps SC=14 after spilling one entry. A pop at SC=2 refills only when SP exceeds SC, retaining SC=2. Spill advances EP by two; refill decreases EP by two. This supplies the missing refill guard. APR20 uses a capacity formula based on 14, whereas the later family manual states 15; treat capacity differences explicitly rather than silently choosing either.

[Motorola patent EP0720087A1, Figures 3/4, steps 124-148](https://patents.google.com/patent/EP0720087A1/en) describes spilling the oldest stack LOW word at EP, incrementing EP, spilling HIGH at the next address, then incrementing EP again. Refill decrements EP before reading HIGH, decrements it again before reading LOW. It identifies the hardware bottom by SP minus SC and describes two-entry transfers when pipeline overlap produces an additional push. Retrieved HTML and extracted text are retained under `conformance_tmp/stack-extension-patent.{html,txt}`.

This patent is design evidence, not an executed DSP56300 result. The candidate implementation should use LOW/HIGH spill and reverse refill, with the family-manual SP/SZ fault rules. Validate the physical bottom indexing against before/after SC updates and test distinguishable low/high canaries; do not substitute an arbitrary symmetric order merely because round trips pass. Pipeline batching/timing and independent hardware/simulator validation remain open.

Initial SEN-enabled stack execution implemented: SC register storage, 24-bit logical SP, physical slot-zero use, LOW/HIGH spill at SC>14, reverse refill below SC=2 when logical entries remain, EP updates and WRP on spill. Extended EOV/EUN routes are present but not yet independently verified. Two executed 32-entry guest MOVEC sequences in X/Y check every returned SSH/SSL value, exact eighteen spilled word pairs, logical SP/SC thresholds and EP restoration. This is partial extended-stack implementation: normal-mode SC accounting, enabling on a preexisting hardware stack, explicit SC task switching, double-operation pipeline batching, fault edge cases, compatibility semantics and transfer timing remain gaps. Pinned xemu lacks extension, so the new regression is specification-derived local proof only. Capture layout is unchanged; normal SEN-disabled captures retain previous SC behavior pending an explicit accounting/version decision. Full arbitrary-program/audio acceptance remains open.

Guest SC writes below two now use the same refill helper as extended pops. It restores up to two available entries, reads HIGH/LOW in reverse spill order, updates EP/SC and refreshes SSH/SSL without setting WRP. The executed X/Y regression covers SC=0/1 and logical depths 0..3, including no refill on an empty stack and a single entry when only one exists; it failed before the fix. This is specification-derived local coverage, not independent hardware conformance. Normal-mode SC accounting, mode transitions, transfer timing, and full gameplay/profiling acceptance remain open.

Normal SEN-disabled pushes/pops now increment/decrement the five-bit SC register, as required by DSP56300FM Rev. 5 section 5.4.3.2. A guest MOVEC regression pushes fifteen distinguishable entries and pops them, checking every SC value and returned word; it failed before this change. Capture version is now 3 (layout unchanged): versions 1/2 must be regenerated because historical initial states lack architectural SC accounting. New capture/replay tests pass; prior gameplay version-2 results remain historical evidence and cannot be replayed as version 3. Extension mode transitions, timing, independent extension verification, and full gameplay/profiling acceptance remain open.

Executed populated-stack transition coverage: guest code pushes fourteen entries with SEN disabled, enables SEN using MOVEC to OMR, pushes eighteen further entries, checks all eighteen LOW/HIGH spill pairs and SP/SC/EP, then pops all thirty-two entries exactly. Both X/Y cases pass in Debug and Release. This verifies enabling at SC=14 with a coherent normal stack; it does not establish immediate transfer behavior when enabling at SC=15, disabling a nonempty extended stack, pipeline timing, or hardware equivalence.

Exception evidence clarification: Rev. 5 page 5-20 says a stack exception occurs only in non-extended mode, but the detailed OMR EOV/EUN definitions on pages 5-6/5-7 explicitly specify priority-three exceptions on zero-to-one transitions. The implementation follows those detailed definitions. A guest fast-vector ADD regression now verifies both overflow and underflow dispatch with IPL masked, then clears OMR and restores SP/SC using guest MOVEC instructions and triggers a second handler invocation without CPU reset. No prohibited subroutine call occupies the extended stack-error vector. This proves the local interpreter route, not hardware interrupt timing.

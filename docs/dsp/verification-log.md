# MM3 DSP verification log

Moved from xboxrecomp `src/apu/dsp/PROVENANCE.md` (the verification half; the
source provenance stays there). Paths are the machine this work ran on.

## Verification status

DSP host memory reads assemble little-endian byte, halfword and 64-bit reads,
including unaligned word crossings, instead of silently returning zero for
every access except aligned 32-bit reads. Integration regressions cover these
reads against uploaded P words. Writes now merge byte lanes under the device
lock, preserving neighboring bytes and both words of a 64-bit upload. DSP
memory containers discard the upper byte to retain 24-bit words. Regression
checks cover wide uploads, single-byte replacement and cross-word writes.
Integrated run `conformance_tmp/movie_skip_20261007_205356_914` after these
memory-routing changes bootstraps both processors and produces nonzero EP
FIFO0 PCM, reaching the 30-second watchdog without a recorded crash or MMIO
decode failure. Gameplay and audio quality remain unverified.

DMA memory ranges are checked as half-open ranges without unsigned addition
overflow, accepting transfers ending exactly at the X/Y/P window limit.
Interleaved bounds and staging allocation use decoded block/channel word
counts. The focused test reproduces the former P-boundary abort, then checks
both input into the final two P words and two-channel interleaved output from
those words.
Circular scratch DMA stops at a failed chunk instead of continuing through
the wrap into another guest RAM region. The regression injects a tail-chunk
fault and verifies no head-chunk write, no offset writeback and no EOL.
The defined DMA NOP command preserves control and transfer state instead of
asserting as an invalid action; a regression covers NOP on a running frozen
engine. ABORT and configuration-triggered behavior remain unaudited.
After the split-frame, boundary, circular-fault and NOP changes, runtime run
`conformance_tmp/movie_skip_20261007_204141_117` again produces nonzero EP
FIFO0 PCM (531 nonzero bytes in the first nonzero 1,024-byte frame), reaching
the 30-second watchdog with no recorded crash or MMIO decode failure. This
rechecks the integrated path but still does not prove representative gameplay.

The standalone Release build passes uploaded arithmetic/jump execution,
instruction replacement, separate GP/EP state, scratch DMA in both directions,
24-bit masking and frozen-DMA polling without manufactured completion checks.
This does not prove
MM3 DSP execution, broad DMA equivalence, sustained audio or performance acceptance.
GP/EP register uploads, reset/bootstrap, peripherals, scratch SGE and FIFO DMA
are now wired to the interpreter and native PCM frame buffer. Both fake
acknowledgement paths and fixed host reverb were removed. The waveOut backend
now submits each actual 256-sample frame rather than clearing DSP output and
rendering eight synthetic software-mixer frames on each delivery.

Three Release checks pass: CPU/DMA, APU integration, and the same CPU cases
compiled against the unmodified pinned upstream interpreter using host-only
header shims. Integration covers guest-issued DMA completion across SGE pages
and EP PCM output through a wrapping FIFO. This upstream comparison covers
only the uploaded arithmetic/jump/cache/isolation cases, not full MM3 microcode.
It also runs identical uploaded `brclr`/`brset` peripheral-polling instructions
with identical status inputs in the port and unchanged upstream CPU, checking
both taken loops and exits. This checks instruction semantics, not DMA timing
or full-program audio equivalence.
The integration check also injects an invalid scratch SGE and verifies that
it sets DMA error, does not set EOL and leaves the guest command word intact.
DSP assertions remain active in Release so unsupported operations cannot
silently become successful transfers or instructions.

The isolated full-game build is at `I:/repos/mm3-dsp-build`, using
`RECOMP_GEN_DIR=I:/repos/midtown-madness-3-recomp-clean/conformance_tmp/mm3_seeded_gen_20261004`
and `MM3_MOVIE_INPUT_TRACE=ON`. The default `src/recomp/gen` snapshot failed to
link with duplicate guest definitions and missing recovered entries; the
existing `build-msvc-tailfix/CMakeCache.txt` identifies the seeded source tree.
No primary build tree or generated game sources were changed.
The seeded full-game Release executable built successfully, including an
incremental rebuild of the latest DSP sources.

Runtime evidence: `conformance_tmp/movie_skip_20261007_200113_716` passed the
three movie boundaries but terminated at the configured 20-second watchdog.
Its initialization log exposed the remaining native aperture bypass: only
APU/VP registers were trapped, leaving GP/EP upload/reset writes as plain RAM.
The aperture now traps the full 512KB, including GP/EP. A second isolated run,
`conformance_tmp/movie_skip_20261007_200451_401`, also passed movie boundaries
and ended at the watchdog; it does not prove menu/gameplay or DSP completion.
Native bulk-copy/MMIO instruction handling and actual MM3 microcode execution
remain to be verified through this aperture, beyond the direct-handler tests.

Upstream contains unimplemented opcode entries and DMA limitations. Therefore
importing it alone is insufficient evidence of support for every valid DSP
program; those gaps must be audited and addressed or explicitly remain open.

## Remaining acceptance work

Framebuffer capture from run `conformance_tmp/movie_skip_20261007_205551_916`
(`conformance_tmp/dsp_scene_20261007_2100/frame00001.bmp`) visibly shows a
street/race scene paused on Resume Race, with the pizza-delivery objective.
The timed harness keeps pressing Start after movie skipping and can pause
gameplay. Subsequent gameplay validation should use Live movie-only skip
input and deliberate driving controls. This image establishes scene state,
not sustained driving or audio-quality acceptance.
Movie-only Live run `conformance_tmp/movie_skip_20261007_205726_420` reaches
`AttractMode2.bik` without a deliberate title-screen Start press. The previous
race image therefore does not establish entry into player-controlled gameplay;
attract/demo behavior must be excluded during scene validation. A deliberate
title-screen Start followed by observed menu selections is still required.
Run `conformance_tmp/movie_skip_20261007_205907_042` supplies a single
post-intro Start, observes Select a Profile, then presses A and observes the
main menu with Player1 and Work Undercover selected. Evidence:
`conformance_tmp/dsp_menu_20261007_2103/frame00367.bmp` (profile) and
`frame01324.bmp` (main menu). It produces nonzero EP PCM and reaches the
90-second watchdog without a recorded crash. Mission entry and driving
remain unverified; the attempted final A press was too near the run limit.
Run `conformance_tmp/movie_skip_20261007_210204_899` deliberately navigates
profile, Work Undercover, Washington D.C., Pizza Deliverer, Standard Delivery
and Cadillac automatic transmission. `conformance_tmp/dsp_mission_20261007_2110/frame00090.bmp`
shows the playable scene with the player's car, Angelina, minimap and speed HUD.
The run then terminates with access violation at executable RVA `0x30DF856`,
fault address `0x000002B01675795B`; guest register context is zeroed on that
thread. Host PCM was captured in the same artifact directory. Sustained
driving and audio quality cannot be accepted until this crash is diagnosed.
The isolated Release build now emits `I:/repos/mm3-dsp-build/Release/mm3_recomp.map`
for future crash attribution. The earlier crashing executable lacked matching
symbols; relinking changed code placement, so the old RVA must not be assigned
to a subsystem using this new map. Reproduction against the mapped executable
is required. The old fault lies at guest-mapped offset `0xFF98795B`, and its
zeroed guest register context alone does not establish that DSP execution
caused it.
Mapped-build reproduction `conformance_tmp/movie_skip_20261007_211018_256`
uses a single post-intro Start, seven A presses and a 30-second accelerator
hold, all logged as consumed live input. The executable SHA-256 and matching
map are preserved in `conformance_tmp/dsp_mapped_20261007_2115`. At the latest
poll the owned PID 16464 remains live without a recorded crash; runtime
session 91004 is pending. Screen state and final result still need verification.
That run reached the 180-second watchdog without a crash; absent screen
capture it does not prove mission entry. Captured follow-up
`conformance_tmp/movie_skip_20261007_211344_689` shows the sequence requires
an additional A after transmission selection. Its
`conformance_tmp/dsp_mapped_scene_20261007_2120/frame00018.bmp` shows playable
Standard Delivery. Accelerator input is issued and owned PID 33988/session
43298 remains live at the latest check; final outcome is pending.
That captured mapped run subsequently reaches its 180-second watchdog with
no recorded access violation. `frame00035.bmp` shows the Cadillac moved from
its start to a storefront after the logged 30-second accelerator hold, with
the delivery counter at one. This verifies a short interval of controlled
mission movement. It does not resolve the earlier intermittent crash or
prove sustained audio quality, music/voice/effect coverage or transitions.
The earlier mission-entry PCM capture is now analyzed using the existing
stdlib analyzer: 253.504 seconds, stereo RMS 1848.46/1828.85, 97 full-scale
samples. Seconds 208 through 246 are silent, overlapping the long wait at
transmission selection; audio resumes at mission entry. `analysis.json`,
`host.wav` and a `mission-entry.wav` excerpt starting at PCM second 247 are
saved in `conformance_tmp/dsp_mission_20261007_2110`. Capture timing is not
an independently synchronized scene clock, so this remains approximate
scene attribution, and the crash limits actual mission audio coverage.
Audio-captured mapped run `conformance_tmp/movie_skip_20261007_211857_305`
reaches Standard Delivery through eight explicit A selections. Its
`conformance_tmp/dsp_drive_audio_20261007_2125/frame00015.bmp` shows mission
entry with the employer voice subtitle; `frame00030.bmp` shows changed car
position after a 20-second accelerator hold. Brake/reverse and steering inputs
follow. Host PCM capture is active in that directory. Owned PID 39660/session
73008 is live without a recorded crash at the latest check; final audio
analysis and outcome are pending.
That run reaches its 180-second watchdog without a recorded crash. Submitted
PCM duration is 179.749 seconds; channel RMS is 4401.34/4791.00 and 214 samples
are full scale. No whole one-second window after second 46 is silent. The
same directory contains reproducible `analyze.py`, `analysis.json`, `host.wav`
and a `continuity.json` audit of PCM after second 60. These signal statistics
support a longer captured gameplay interval but do not identify music,
voices, effects/reverb or prove sound quality and all kinds of dropouts.

EP FIFO0 monitor staging now accepts a PCM frame split over multiple DMA
descriptors; the regression checks two 512-byte fragments produce the same
frame and FIFO pointer as one 1,024-byte descriptor. Reset clears the staging
offset.
Rewriting an already released EP reset register preserves partially staged
PCM; only reset assertion or release resets its offset. A regression covers
both unchanged-register writes and reset assertion.
Descriptors spanning beyond the remaining host frame capacity still
require queued host output and currently report an error; arbitrary-program
support is not complete.

The isolated MM3 run `conformance_tmp/movie_skip_20261007_201127_957`
with `RECOMP_APU_TRACE=1` confirms guest reset releases bootstrap both cores:
GP P[0..2] = `0BF080 000155 300600`, EP = `050C08 000000 000001`.
Their first frames executed 12,892 and 340 cycles respectively, reached the
guest idle peripheral, and reported no DMA error. Only one completed frame
per core was logged before the 20-second watchdog; continuous execution,
command completion and gameplay audio are still unproven. The bounded trace
is diagnostic evidence, not a gameplay acceptance or performance result.

Follow-up run `conformance_tmp/movie_skip_20261007_201341_730` locates an
execution stall at GP PC `0x11E`, opcode `0CD604` (`brclr #4,X:$FFFFD6`).
DMA control is `0x20`: the program is waiting to observe RUNNING, but the
current synchronous transfer completes and clears it before the next DSP
instruction can read it. Real scheduled DMA progression must replace this
instantaneous state transition; restoring upstream's read-count completion
workaround would not satisfy the requirement. DMA now consumes one descriptor
at a DSP instruction boundary after START, and stops only when that scheduled
transfer reaches EOL. Polling does not advance it. Descriptor granularity is
an approximation; hardware bus-cycle timing has not been validated.

Run `conformance_tmp/movie_skip_20261007_201710_119` confirms repeated guest
frames after this change (GP 458 cycles, EP 15,856 cycles after initialization),
with no reported DMA errors. It subsequently crashes in a host memory access
with guest ESI `0xFE8312E4`, within GP memory. Bulk-copy/address routing needs
investigation before sustained gameplay can be claimed. Regression checks
now verify that a guest-issued START remains observable, repeated reads make
no transfer, and the next instruction boundary completes the actual transfer.

The crash instruction is an AVX-512 load inside host `memcpy`. The existing
current lifter's `_lift_rep_movs` already guards hardware windows, while the
seeded generated build input predates that guard. For runtime validation an
isolated copy at `I:/repos/mm3-dsp-generated` has 1,535 old forward bulk-copy
guards updated to exclude hardware ranges and 32-bit range overflow; its
fallback retains element size and direction. The isolated DSP build now uses
that copy. The shared seeded generation and primary build are untouched.
The completed refreshed-build run
`conformance_tmp/movie_skip_20261007_202734_420` reaches its 30-second watchdog
without the earlier AVX-512 GP-memory crash or an MMIO decode failure. Repeated
GP/EP frames are logged. This headless intro-skip run does not establish menu,
driving, audible output or sustained gameplay acceptance.
Run `conformance_tmp/movie_skip_20261007_203057_963` additionally observes
guest-driven EP FIFO0 DMA producing a 1,024-byte PCM frame with 531 nonzero
bytes after initial silent frames. The diagnostic reports the first nonzero
frame as well as the initial eight frames. This proves nonzero DSP PCM reaches
the native monitor buffer; its sound quality and gameplay content remain
unverified. The three focused Release checks still pass.
The 40-second watchdog run `conformance_tmp/movie_skip_20261007_203238_913`
captures submitted host audio with the existing `RECOMP_XA2_PCM_DUMP` hook.
Artifacts in `conformance_tmp/dsp_audio_20261007_2033` include raw PCM, a WAV,
`analysis.json` and a runnable stdlib `analyze.py`. The capture contains 33.84
seconds at 48 kHz stereo s16le, channel RMS 3551.36/3548.08, and 127 full-scale
samples. One-second windows 2, 3 and 8 through 11 are silent. Movie audio is
included, so these statistics do not isolate DSP quality or prove absence of
dropouts. No crash or MMIO decode failure was recorded before the watchdog.
The chained-descriptor regression also verifies RUNNING persists after the
first descriptor, FREEZE prevents the second transfer, and EOL appears only
after UNFREEZE and the scheduled final transfer. All three Release checks
pass with this coverage.
Input DMA descriptors for FIFO0/FIFO1 now use the existing GP/EP input FIFO
callback instead of asserting. Focused tests cover both FIFO indices with
16-bit samples, including a failed callback that must neither overwrite DSP
memory nor signal EOL. All three Release checks pass. This connects the
existing input path; it is not an independent hardware conformance result,
and interleaved input remains unsupported.
The optional reference build now also compiles unchanged pinned xemu
`dsp_dma.c` with host-header shims. `dsp_dma_reference` runs identical
descriptors and input buffers through both engines and compares complete
test memory, transfer buffers and next pointers byte for byte. All 16 cases
match: linear/circular scratch, 16/24-bit samples, both directions,
interleaved output and all four output FIFOs. Saved results at
`I:/repos/mm3-dsp-xemu-reference/test-build/dma-results.{port,reference}.bin`
have SHA-256 `f02dfbc5c9d3817bf8d07342b74eb54a40000331646395e370a7f297f55b1b0e`.
The comparison deliberately excludes scheduling/read-count status: upstream
performs DMA synchronously and manufactures STOPPED after three reads.
Port timing and no-false-completion regressions remain separate. These
transfer checks do not establish hardware conformance, unsupported modes,
or full-program audio equivalence. All four Release checks pass.
EP FIFO PCM publication now occurs only after the complete guest RAM transfer
succeeds. A focused invalid-SGE regression failed before this change because
the monitor buffer contained the failed payload; it now preserves the prior
monitor bytes, partial-frame offset and FIFO cursor, with DMA error set and
no EOL. Successful wrapping and split-descriptor PCM tests still pass. The
one-frame staging capacity limit remains unresolved.

1. Validate GP/EP MMIO, bootstrap and peripheral behavior using MM3's microcode.
2. Expand DMA checks beyond the tested SGE crossings, FIFO wrap and 16/24-bit
   transfers to all supported DMA formats/modes and fault propagation.
3. Verify MM3's command writes, interrupts and output arise from execution.
4. Validate VP, native host mixing and both XAudio2/waveOut delivery in runtime.
5. Audit upstream missing opcodes and DMA modes against valid DSP56300/MCPX
   programs; importing upstream does not itself close this requirement.
6. Compare identical programs/buffers and resulting DSP state/audio with an
   independently built xemu reference; retain reproducible inputs/results.
7. Verify MM3 music, voices, effects, environmental effects, mixing and scene
   transitions during sustained representative gameplay.
8. Extend the focused checks to guest-driven DMA/peripheral completion so a
   future bypass cannot pass by merely retaining an unused interpreter.
9. Profile the integrated interpreter during normal gameplay; no JIT planned
   without measured need.
10. Keep incorporated source/license attribution current as glue is adapted.

## Reproduce the standalone check

`RECOMP_DSP_FRAME_DUMP=<prefix>` records one real frame per processor to
`<prefix>.{gp,ep}.frame.bin`. `RECOMP_DSP_FRAME_INDEX` selects the zero-based
frame (default 0). The capture contains a version/ABI-size header, initial
DSP state with host pointers/opcode cache removed, actual scratch/FIFO
callback events and payloads, then final state when the processor idles.
Failed transfers have an error event without uninitialized payload data.
This is a local-build ABI diagnostic, not a portable saved-state format.
The standalone `dsp_frame_capture` check validates both complete files and
the EP PCM transfer payload. All five Release checks pass.
Run `conformance_tmp/movie_skip_20261007_213417_592` captured the first real
GP/EP frames in `conformance_tmp/dsp_frame_20261007_213417`, with hashes in
`manifest.json`. Both files are 160,288 bytes and reach idle; GP PC changes
from 0 to `2B`, EP from 0 to `2F3`. These initial frames contain no DMA
events, so later-frame capture is necessary for scratch/FIFO replay.
Both `dsp_frame_replay <capture>` and `dsp_xemu_frame_replay <capture>` now
execute a captured complete frame, replaying only external RAM/FIFO inputs.
Every outgoing payload and the complete sanitized final state must match.
The reference executable compiles the unchanged pinned upstream CPU with
the port's DMA/peripheral glue, so it independently checks CPU execution,
not the full APU model. The separate unchanged-upstream DMA comparison
still checks common transfer modes. All nine Release checks pass, including
synthetic GP/EP complete-frame replay with both CPU builds.
Run `conformance_tmp/movie_skip_20261007_213537_434` captured frame index 32
in `conformance_tmp/dsp_frame_dma_20261007_213537`. Both CPU builds reproduce
GP's 458 cycles/eight DMA transfers/final PC `2B` and EP's 15,856 cycles/eight
transfers/final PC `30`, with exact final state and payload matches. Hashes
and transfer summaries are in `manifest.json`. EP includes a 1,024-byte
FIFO0 output, but it is silent; nonzero representative audio comparison is
still required. Corrupting an outgoing payload byte in either capture makes
the port replay fail at payload comparison, confirming the gate rejects
changed output rather than merely reaching idle.
Run `conformance_tmp/movie_skip_20261007_213744_034` captures frame index
4000 in `conformance_tmp/dsp_frame_audio_20261007_213743`. EP FIFO0's
1,024-byte output contains 768 nonzero bytes. Both replay CPU builds match
that entire PCM payload, all eight DMA events and complete final state
exactly (15,856 cycles, PC `30`). GP also matches its eight events and final
state (458 cycles, PC `2B`), although its captured outputs are silent.
The capture directory retains `manifest.json`, `ep-output.pcm`,
`ep-output.wav` and `pcm-analysis.json`. This is a nonzero single-frame
CPU comparison using shared DMA/peripheral glue, not independent full-APU
or representative sound-quality validation. The 45-second harness run
terminated normally at its time limit and reached attract mode.

`RECOMP_DSP_BOOT_DUMP=<path-prefix>` optionally saves the uploaded 2,048
bootstrap P words as `<prefix>.gp.p.bin` and `<prefix>.ep.p.bin` (32-bit
little-endian containers on this Windows host, masked to 24 bits).
Run `conformance_tmp/movie_skip_20261007_204634_551` captured both images in
`conformance_tmp/dsp_boot_20261007_2045`, with SHA-256 hashes and first words
in `manifest.json`. Both images are 8,192 bytes and all words fit 24 bits.
These are reproducible program inputs; accompanying scratch/DMA input state
and full-program reference execution still need capture and comparison.
Both test executables accept `<program.bin> <state-output> gp|ep` to replay
the bootstrap prefix with identical initialized memory, stopping at the first
peripheral access. Registers, stack, X/Y/P memory, mixbuffer, PC and boundary
event are saved for byte comparison. For the captured MM3 images, GP stops
after two instructions at PC `157` on a write of `FFF` to `FFFFC5`; EP stops
after five at PC `F` on a write of `FFFFFF` to `FFFFB2`. Port/reference state
files in the capture directory have identical SHA-256 hashes per processor.
This is only a short prefix comparison, not full-program DMA/audio validation.

Use the Visual Studio bundled CMake on Windows:

```powershell
cmake -S tools/xboxrecomp/tests/dsp -B <scratch-build> -G 'Visual Studio 18 2026' -A x64
cmake --build <scratch-build> --config Release
ctest --test-dir <scratch-build> -C Release --output-on-failure
```

To include the independent upstream CPU and DMA cases, configure with
`-DDSP_XEMU_REFERENCE_DIR=<pinned-xemu-checkout>`. Configuration verifies the
commit and refuses staged or unstaged modifications to upstream DSP sources.

Immediate AND/OR/EOR regression coverage now checks 30,240 combinations,
including all four initial S/L sticky-bit states and all three valid scaling
modes. Accumulator extension and low words remain unchanged. Preserving S/L
for these instructions follows from the DSP56300 manual SR definitions:
S tracks accumulator bus moves, L tracks overflow/limiting, and immediate
logical operations update the middle word and clear V without a bus move.
This is an inference from the documented flag rules, not independent hardware
validation. All nine DSP CTest checks pass; the captured nonzero MM3 EP frame
still matches all eight transfers and final state exactly. General arithmetic
saturation, instruction timing and full arbitrary-program conformance remain
open. Reference: https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf

Interleaved DMA input now uses the inverse of the existing upstream output
layout: host sample `block * channels + channel` writes DSP word
`channel * blocks + block`. Input uses the same FIFO/scratch callbacks and
format conversion as linear input, and external failure precedes any DSP
memory writes or completion. Tests cover 72 combinations of one to three channels, one/four blocks,
16-bit and 24-bit-LSB inputs through FIFO and linear scratch, ending exactly
at supported X/Y/P memory boundaries. A failed interleaved FIFO transfer
preserves all 12 destination words. A separate DMA program upload replaces
an already decoded ADD instruction with SUB and verifies subsequent guest
execution uses the new word, proving P-memory opcode-cache invalidation. All nine tests and the full MM3 Release build pass;
both CPU replay builds still match the captured nonzero EP frame exactly.
This layout is inferred from the existing output implementation. The pinned
xemu implementation asserts on interleaved input, so independent hardware
or full-APU validation of this newly supported direction remains open.

JCLR/JSET/JSCLR/JSSET now share the existing upper-I/O handlers with the
lower-I/O bank, selecting FFFF80 or FFFFC0 from the instruction encoding.
JCLR/JSET pp/qq opcode patterns accept all five bit-index bits, correcting
the inherited four-bit restriction. Tests execute 24,576 combinations across
both X-I/O banks, all 64 addresses, bits 0..23, all four operations and both
input states. Taken subroutine jumps execute RTS and return to PC+2; untaken
jumps leave the stack unchanged. SR and input values remain unchanged.
All nine tests, the full Release build and captured nonzero EP replay pass.
Y peripheral access, disassembly and independent instruction-cycle validation
remain open. Opcode encoding facts were checked against DSP56300FM and
https://raw.githubusercontent.com/dsp56300/dsp56300/master/source/dsp56kEmu/opcodeinfo.h;
no implementation from that separate emulator was copied.

Conditional absolute jump decoding now accepts five-bit indices for all four
X/Y short-address and indirect forms. JSCLR/JSET short-address rows no longer
apply an indirect-address predicate: their six address bits are unrestricted
absolute addresses. The shared jump test now executes 73,728 cases across
both I/O banks, X/Y short addresses and X/Y `(R0)` indirect addresses, all
valid bit indices and both conditions, including stack/RTS checks. All nine
tests, the full Release build and captured nonzero EP replay pass. Other
indirect update modes, cycle counts and full arbitrary-program conformance
remain open; the instruction handlers themselves are retained from upstream.

Indirect bit-branch/jump coverage now executes 32,256 combinations across
all seven supported addressing modes, all eight Rn registers, X/Y memory,
bits 0..23, both conditions, relative branches, absolute jumps and subroutine
jumps. Tests verify the effective address, Rn updates for taken and untaken
conditions, unchanged operand/SR, stack depth and return to PC+2 after RTS.
All nine CTest checks pass. This validates the existing indirect handlers
without changing production code. Modulo/bit-reversed addressing, extended
mode 6 and instruction timing still need separate conformance evidence.

BRA/Bcc/BSR register-relative forms now reuse the existing long branch
handlers. Their shared operand reader selects Rn without fetching an extension
word or increasing instruction length. Tests cover all eight Rn registers,
eight signed/zero/wrapping displacements, BRA/BSR stack behavior and valid
target RTS returns to PC+1. Bcc runs 262,144 combinations of register,
displacement, condition code and initial CCR, checking its condition result
against the independent equations in DSP56300FM tables 12-17/12-18 while
checking one-word fall-through. The same oracle verifies the long form. All nine
tests, the full Release build and nonzero MM3 EP replay pass. Register writes
are still subject to inherited 16-bit address-register masks; full 24-bit
address-register behavior and branch cycle timing remain open. Semantics
checked against DSP56300FM BRA/Bcc/BSR sections.

BScc now supports long, signed-short and Rn-relative operands by applying
the existing condition evaluator and BSR handlers. Untaken long forms advance
over the extension word; untaken short/register forms advance one word and
do not push the stack. Tests execute 720,896 form/register/displacement/CCR
combinations, verify unchanged CCR/Rn and stack depth, and execute RTS for
accessible taken targets to check the correct return address. The condition
oracle uses DSP56300FM tables 12-17/12-18 directly, covering every CCR byte
and all 16 condition encodings independently of the production evaluator. DSP56300FM section 13-31
confirms the operand/stack semantics. All nine CTest checks, the full Release
build and captured nonzero EP replay pass. Condition-code hardware equivalence,
interrupt-pipeline interaction and cycle timing remain open.

The APU DSP frame runner now releases the APU lock between 1,000-cycle
interpreter batches, yields, and reacquires it before accessing DSP state.
Guest MMIO can reset or command a continuously executing program. Reset
enable bits and the atomic shutdown flag are checked before another batch;
EP enable state is refreshed after GP execution because MMIO can change it.
A threaded regression runs an uploaded JMP-0 program, confirms execution,
resets it through the real MMIO entry point, and verifies the frame thread
returns with cleared DMA state and no manufactured EOL. Tests now honor the
production frame entry point lock contract. All nine checks and the full
Release build pass. This removes lock starvation, not full clocked scheduling:
a continuously running DSP still occupies the frame worker until reset or
shutdown, so independent GP/EP/native-output scheduling remains open.

Post-change 35-second isolated smoke run:
`conformance_tmp/movie_skip_20261008_080427_713`. Harness exited successfully
and owned PID 42576 is absent. Interpreter initialization and guest APU
interrupt delivery appear in stderr. Both host audio devices failed to open
(XAudio2 0x80070490, waveOut error 2), so this run cannot verify audible
playback or native device-output stability. It provides startup/termination
evidence only, not representative gameplay or sound-quality acceptance.

After lock yielding, isolated run `conformance_tmp/movie_skip_20261008_080612_693`
completed its 45-second limit and reached the AttractMode2 movie. Captures
in `conformance_tmp/dsp_frame_20261008_lock_yield` contain GP/EP frame 4000.
Both port and pinned unchanged xemu CPU replay all eight transfers and final
state exactly: GP PC 2B/458 cycles, EP PC 30/15,856 cycles. EP FIFO0 output
is 1,024 bytes with 949 nonzero bytes, RMS L/R 680.348/647.875, peak 2,338
and no full-scale samples. PCM SHA-256:
`b45463cca828907fbdaac9dba22ad08c5fb3a1029e8f084b1e27847e249a95d6`.
Manifest, PCM/WAV and PCM-analysis JSON are retained beside the captures.
Both replay builds share DMA/peripheral glue; this is a single-frame CPU
comparison, not independent full-APU conformance. Windows audio services
are running but no present AudioEndpoint devices were enumerated, and both
XAudio2/waveOut initialization failed. Audible output and representative
gameplay quality cannot be established by this run.

Shutdown no longer waits for the APU idle condition before requesting exit:
it sets the exit flag under the lock, signals the worker and joins it before
finalizing/freeing state. A continuously executing DSP need not become idle.
The GP frame runner returns immediately on exit so a subsequently enabled EP
is not started during shutdown. The threaded regression now covers both reset
and exit; the exit case enables a looping EP and verifies its cycle count
stays zero, while GP execution stops without fake idle or DMA EOL. Both APU
tests have a 10-second CTest timeout to catch lock regressions. All nine
checks and full Release build pass; nonzero EP captured replay remains exact.
The regression exercises the worker exit path, not the full shutdown API.

BSCLR/BSSET memory operands now reuse the bit-branch handlers, pushing PC+2
and SR only when the tested condition takes the relative branch. Supported
forms are both X-I/O banks, X/Y short absolute addresses and the seven
indirect addressing modes. The lower-I/O decoder distinguishes its call bit
from the absolute-address bit in the upper-bank encodings. Tests exercise
98,304 additional short/I/O call cases and 10,752 indirect call cases,
including all valid bit indices, taken/untaken stack checks and RTS returns.
All nine tests, full Release build and nonzero EP capture replay pass.
Register-source forms, extended EA mode 6, interrupt interactions and
instruction timing remain open. Stack/operand semantics are documented in
NXP AN1829 table 19: https://www.nxp.com/docs/en/application-note/AN1829.pdf

BSCLR/BSSET register-source forms now share the established register bit
branch handlers and push the return address/SR only for a taken subroutine
branch. Tests execute 27,648 cases across data registers, accumulator parts,
limited A/B reads and all R/N/M registers, bits 0..23, both input states,
clear/set branches, ordinary/subroutine variants and four displacements.
They verify operand/SR preservation, stack depth and return to PC+2. These
tests use values fitting the inherited register widths; they do not validate
24-bit AGU writes or accumulator scaling/saturation. Special control/stack
register side effects and interrupt interactions remain open. All nine
checks, full Release build and captured nonzero EP replay pass.

Immediate LSL/LSR now share a direct 24-bit shift handler. This adds the
missing immediate LSR form and corrects inherited LSL count-zero behavior:
C and V clear, N/Z reflect the unchanged middle word. Extension/low words
and other SR bits remain unchanged. The test oracle shifts one bit at a time
and checks 128,000 direction/accumulator/count/value/CCR combinations, including
counts 0..23 and the 24-bit boundary. DSP56300FM pages 13-94/13-96 specify
these flag rules. All nine tests, full Release build and captured nonzero EP
replay pass. Counts above 24, register-specified shifts and cycle timing
still need separate conformance evidence.

Register-specified LSL/LSR now share the immediate shift handler, decoding
A1/B1/X0/Y0/X1/Y1 from DSP56300FM table 12-13 and reading the five-bit SH
field before modifying the destination. Shift tests now cover 896,000
combinations across both directions, both accumulators, immediate/all six
source registers, counts 0..24 and every initial CCR byte. Source high bits
are deliberately nonzero to verify only SH controls the count; alias cases
use destination A1/B1 as the source count. Other source registers and the
destination extension/low words remain unchanged. All nine tests, full
Release build and nonzero EP replay pass. Invalid source encodings, counts
above 24, hardware timing and broader ISA conformance remain open.

AGU register destination masks now preserve 24-bit R/N/M values when SC is
clear, and retain 16-bit destination truncation when SC is set. All existing
register-width mask users route through one shared helper, including direct
parallel-move destinations and the central register writer. Tests execute
guest immediate MOVE/MOVEC loads into all 24 AGU registers, both modes and
five width/sign boundaries (240 cases), checking PC and unchanged SR. All
nine tests, full Release build and captured GP/EP frame replays pass. This
is only the register-write part of 24-bit AGU support: source transfers in
SC mode, mode-change pipeline delay, reset values, linear/modulo/bit-reverse
address arithmetic and hardware equivalence remain incomplete. The inherited
address updater still truncates its modifier and results to 16 bits.
Reference: DSP56300FM sections 4.2/4.3 and AN1829 table 2.


Linear AGU address updates now retain full-width offsets and wrap Rn at
24 bits with SC clear, or 16 bits with SC set. The shared updater accepts
an int32_t modifier; subtraction casts before negation. The inherited
low-16-bit M selector is retained: DSP56300FM section 4.5.1 specifies
XXFFFF for linear addressing, rather than requiring all 24 bits to be one.
Tests execute 5,760 real LUA instructions across all eight R/N/M groups,
both SC modes, four update operations, five address boundaries, six offsets
and three linear M encodings. An independent int64_t arithmetic oracle
checks the destination and preservation of source R/N/M, SR and PC.
All nine CTest checks and the full MM3 Release build pass. Captured MM3
GP and nonzero EP frames still match exactly with both the adapted CPU
and pinned xemu CPU replay (the replay engines share the DMA wrapper).
This supersedes the previous linear-arithmetic limitation; modulo and
bit-reverse arithmetic, SC source transfers/pipeline delay, reset values
and independent hardware equivalence remain open.


Reverse-carry AGU updates now reverse the address and offset, perform the
requested addition/subtraction, and reverse the result at the selected
16/24-bit width. This replaces the inherited 16-bit increment-only shortcut,
which ignored the actual update operation and could discard high address
bits. DSP56300FM section 4.5.2 describes reverse-carry operand arithmetic.
Tests execute 5,760 additional LUA cases spanning all eight register groups,
both SC modes, +/-N and +/-1 operations, address/offset boundaries and three
XX0000 modifier encodings. The oracle independently propagates carry or
borrow from MSB to LSB, rather than repeating the implementation's reversal.
All nine checks and full MM3 Release build pass. The captured GP/nonzero EP
frames remain exact under adapted and pinned xemu CPU replay; both replay
engines still share the DMA wrapper. These tests establish instruction-level
arithmetic, not hardware timing, pipeline behavior or complete DSP support.
Modulo/multiple-wrap addressing and SC pipeline/source behavior remain open.


Ordinary modulo AGU updates now preserve 16/24-bit addresses and decode
signed offsets at the selected width. The inherited int16_t address/offset
and uint16_t boundaries discarded high bits and mishandled signed boundary
comparisons. The shared handler now derives the aligned block/base using
full-width arithmetic, wraps valid offsets once, and treats whole-block
offsets as linear buffer selection, as specified by DSP56300FM section
4.5.3. Tests execute LUA across six moduli (2..32768), four low/high bases,
three buffer positions, all eight groups, both SC modes, +/-N and +/-1,
positive/negative offsets and whole-block jumps. The valid-offset oracle
uses mathematical remainder independently of the handler's boundary logic.
All nine tests, full MM3 Release build, diff checks and captured GP/nonzero
EP replays pass; adapted and pinned xemu CPU replays remain exact with a
shared DMA wrapper. Offsets outside the manual's valid range, other than
whole-block jumps, are unpredictable and are not claimed as conformant.
Multiple-wrap addressing, SC pipeline/source behavior and hardware timing
remain incomplete; broader DSP and representative audio acceptance is open.


Multiple-wrap AGU addressing now recognizes valid XX8001..XXBFFF
power-of-two modifiers and wraps the low buffer-position bits while
preserving the base and selected address width. Reserved modifier patterns
retain their previous undefined behavior. This follows DSP56300FM section
4.5.4 and table 4-2; no external emulator implementation was copied.
LUA regressions cover every supported modulus (2..16384), all eight register
groups, both SC modes, low/high bases, three positions, +/-N and +/-1,
whole-block offsets and the 24-bit signed extrema. A mathematical-remainder
oracle checks results plus source R/N/M, SR and PC preservation. All nine
CTest checks, full MM3 Release build and diff checks pass. Captured GP and
nonzero EP frames remain exact with both adapted and pinned xemu CPU
replay, which share the DMA wrapper. SC pipeline/source behavior, wider
ISA/peripheral conformance and representative audible gameplay validation
remain incomplete; this is not complete DSP acceptance.


CPU reset now initializes all eight M registers to FFFFFF, matching
DSP56300FM section 4.3.4 rather than the inherited 00FFFF value. The reset
regression preloads different nondefault values in GP and EP and verifies
all sixteen reset results. The pinned reference source remains unchanged;
its old reset values are deliberately excluded from this corrected-value
assertion. All nine CTest checks and the full MM3 Release build pass.
A fresh isolated 35-second startup (movie_skip_20261008_082708_422, PID
32812) initialized both interpreters and skipped all three intro movies;
the harness ended and removed its owned process at the time limit. This
checks startup after reset, not sustained gameplay or audio quality.
XAudio2 still failed with 80070490 and waveOutOpen with error 2, so no
audible output validation is claimed. Saved frame replays cannot prove
reset semantics because they restore captured initial CPU state.


MOVEC register, short-memory and effective-address forms now mask AGU
source transfers at the active SC width, as specified by DSP56300FM section
4.2. Non-AGU source reads retain their existing behavior. Sixteen executed
M0..M7-to-X0 cases check both SC modes, deliberately nonzero upper bits,
source preservation and PC advancement. All nine tests, full Release build
and captured nonzero EP replay pass. Parallel MOVE and other transfer
families still need the corresponding source-width audit; SC's three-cycle
pipeline delay remains unimplemented. This closes only the MOVEC portion
of source-transfer compatibility, not complete SC or DSP conformance.


SC source masking now uses one MOVE-source helper, reused by MOVEC,
register-to-register parallel MOVE, both MOVEM forms and register-to-upper-
peripheral MOVEP. Non-AGU reads retain their prior values. Eighty additional
executed instructions check every R/N/M source through short MOVEM and
R/N sources through parallel register MOVE, both SC modes, nonzero upper
bits, source preservation and PC. All nine tests, full Release build and
captured GP/nonzero EP replay pass. Other parallel memory-transfer paths
and lower-peripheral MOVEP still need source-width coverage; SC pipeline
delay and complete audio/DSP acceptance remain open.


Parallel memory MOVE and long-offset MOVE now reuse the SC-aware source
helper. Long/short offset effective addresses also clear the upper eight
bits in SC mode. Sixty-four new parallel-memory instructions check every
R/N source to X/Y short memory in both modes. The long-offset matrix now
runs both SC modes, with deliberately nonzero upper R bits in SC mode,
checking loads/stores, both spaces, offsets and unchanged base/SR/PC.
All nine tests, full Release build and captured nonzero EP replay pass.
The decoded lower-X-peripheral MOVEP path transfers memory or immediates,
not register sources; its Y counterpart still has no handler. Other
parallel forms, SC pipeline delay and full audio acceptance remain open.


Parallel-memory SC regression coverage now includes short absolute and all
seven nonextended indirect modes (512 executed instructions), checking
R/N source truncation, X/Y destination memory, actual address updates,
source/SR preservation and PC. All nine CTest checks and diff checks pass.
No production behavior changed in this increment. The missing lower-Y
MOVEP handler cannot simply reuse X peripheral accesses: Y memory currently
asserts outside YRAM and has no separate peripheral mapping/callback.
That mapping and corresponding handler remain an explicit conformance gap;
no unsupported Y access is silently redirected to X. Extended addressing,
SC pipeline delay and representative audible gameplay acceptance remain open.


Lower-X-peripheral register MOVEP now reuses the upper-bank handler,
decoding its split six-bit qq address and preserving the same real
peripheral callbacks and SC source/destination rules. Tests execute 6,144
cases across all 64 addresses, every R/N/M register, both directions and
both SC modes, checking callback count/value, source preservation and PC/SR.
All nine tests, full Release build, diff checks and captured GP/nonzero EP
replay pass. The decoder now has 43 rows with NULL execution handlers;
this is a lower bound on missing behavior, not a complete ISA audit.
The new instruction remains without a disassembler; Y peripheral mapping,
SC pipeline timing and complete DSP/audio acceptance remain open.


Lower-peripheral/program-memory MOVEP now reuses the upper-bank transfer
handler with the decoder's lower-bank address, space and direction fields.
Tests execute 7,168 X-peripheral cases: all 64 addresses, all eight Rn,
seven nonextended indirect modes and both directions. They check actual
peripheral callbacks, program-memory contents, address updates, SR and PC.
All nine tests, full Release build, diff checks and captured GP/nonzero EP
replay pass. Program writes use the existing opcode-cache invalidation path.
Y peripheral mapping remains absent: the shared handler decodes Y but its
access is still unsupported, not redirected to X. Extended modes, the new
disassembler and independent hardware/reference conformance remain open.
The NULL-handler count falls to 42; this does not prove the implemented
rows fully conform or establish representative audio acceptance.


Vendor DSP56300FM pages 13-134..13-136 independently confirm the MOVEP
lower-bank address fields, register split-qq encoding and P-memory bit-14
direction encoding used in the new handlers. Table 12-13 confirms absolute
EA mode 110000. A new regression exercises 64 absolute-address uploads:
each executes ADD, replaces its already decoded word with SUB through a
real lower-X MOVEP read, executes SUB and checks the changed accumulator,
then reads the uploaded program word back through MOVEP. It checks both
extension-word PC consumption and actual callback directions/counts.
All nine tests and diff checks pass. This confirms absolute addressing and
opcode-cache invalidation in the adapted CPU, not independent hardware
execution or Y peripheral mapping. No production code changed this turn.


Both LRA forms now share a PC-relative address handler: Rn or a fetched
24-bit extension is added to the current instruction PC with 24-bit wrap,
then written to the selected destination without changing CCR. Accumulator
loads clear the low word and sign-extend the middle word; ordinary writes
reuse the existing destination-width writer. DSP56300FM page 13-92 provides
the semantics and encodings. Tests execute 5,376 combinations across both
forms, all eight Rn, all 28 encoded valid data/accumulator/R/N destinations,
both SC modes and six offset boundaries, including source/destination alias,
negative wrap, PC length and SR preservation. All nine tests, full Release
build, diff checks and captured GP/nonzero EP replay pass. Timing and an
instruction-specific disassembler remain open; the pinned CPU lacks LRA,
so these new cases use the vendor arithmetic oracle, not upstream execution.
NULL execution-handler rows fall to 40; broader DSP/audio acceptance remains
unproven.


Short-offset LUA now writes R/N destinations through the shared register
writer instead of assigning the raw 24-bit result. This fixes missing
16-bit destination truncation in SC mode without altering normal mode.
Tests execute 163,840 instructions across all signed seven-bit offsets,
all source/destination R indices, R/N destinations, both SC modes and five
address boundaries. They check aliasing, preserved source/SR and PC against
independent signed int64_t arithmetic. All nine tests, full Release build,
diff checks and captured GP/nonzero EP replay pass. SC pipeline delay,
other instruction/timing gaps and representative audio acceptance remain
open; no broader completion claim follows from these tests.


The shared 56-bit ASR helper now sign-fills vacated upper bits and clears
carry safely at count zero. The inherited helper performed a logical shift
and evaluated a negative shift count for zero. Valid counts 0..55 are
checked; undefined larger counts assert rather than invoking C undefined
behavior. DSP56300FM pages 13-16/13-17 specify sign retention, count limits
and zero-count carry. Tests execute 1,344 immediate ASR cases across both
source/destination accumulators, all valid counts and six signed boundaries,
using a one-bit-at-a-time oracle for the result and C/V. All nine tests,
full Release build, diff checks and captured GP/nonzero EP replay pass.
Register-count forms, SA's 40-bit behavior, complete flag/timing conformance
and broader audio acceptance remain open. This corrects the common helper
before adding new forms; it does not claim full arithmetic-shift support.


Register-count ASR now shares the immediate handler and corrected 56-bit
helper. It decodes A1/B1/X0/Y0/X1/Y1, consumes only the six-bit count field,
and reads the count before accumulator destination writes. DSP56300FM
pages 13-16/13-17 and table 12-13 specify the form. The right-shift matrix
now executes 9,408 cases covering immediate/all six register sources,
counts 0..55, both source/destination accumulators, aliasing, deliberately
nonzero count-register high bits and signed boundaries. Result and C/V
use a one-bit oracle. All nine tests, full Release build, diff checks and
captured nonzero EP replay pass. SA's 40-bit behavior, broader flag/timing
conformance and independent reference execution remain open. NULL handler
rows fall to 39; full DSP/audio acceptance remains unproven.


The shared ASL helper now detects any sign transition during the entire
shift, rather than only comparing initial/final sign, and sets L/V together
for overflow. Previously discarded one bits incorrectly set L even for a
valid negative shift, while sign changes that later returned to the original
sign could lose V. Count zero clears C and valid count bounds avoid undefined
C shifts. DSP56300FM page 13-15 specifies the any-transition overflow rule.
Tests execute 1,344 immediate shifts across both accumulator sources and
destinations, counts 0..55 and six signed boundaries; a one-bit arithmetic
oracle checks the result and C/V/L. All nine tests, full Release build,
diff checks and captured nonzero EP replay pass. Register-count ASL,
sticky-flag/scaling/SA conformance, timing and full DSP/audio acceptance
remain open.


Register-count ASL now shares the immediate handler and corrected ASL
helper. ASL/ASR share one six-bit count decoder for A1/B1/X0/Y0/X1/Y1,
read before destination mutation. The ASL matrix now executes 18,816 cases
covering both sticky-L states, all six count sources/immediate, counts
0..55, both source/destination accumulators, aliasing and signed boundaries.
The one-bit oracle checks result, C/V and sticky L preservation. All nine
tests, full Release build, diff checks and captured nonzero EP replay pass.
DSP56300FM pages 13-14/13-15 provide the register form and overflow rules.
SA/scaling/remaining flags, instruction timing and independent hardware
conformance remain open. NULL execution rows fall to 38; complete DSP and
representative audio acceptance is still unproven.


Interpreter SR flag clearing now uses complement masks instead of
BITMASK(16) minus the named flags. The inherited expression silently
cleared every upper status bit whenever arithmetic updated CCR. The
mechanical correction covers all 269 such assignments in dsp_emu.c.inc,
including the common E/U/N/Z updater; it preserves non-target bits without
changing the named low-bit clearing operation. ASL/ASR matrices now seed
FV (bit 16) and explicitly verify it survives, alongside existing result,
C/V and sticky-L checks. All nine tests, full Release build, diff checks
and captured GP/nonzero EP replay pass. Full SR writes, stack/exception
state width and arithmetic-mode behavior still need independent audit;
this fixes flag-clear truncation only, not complete 24-bit PCU conformance.
Full DSP/audio acceptance remains open.


Explicit SR writes now admit its defined 24-bit fields, clearing reserved
bits 18 and 12. The inherited 16-bit width and AF7F write mask discarded
EMR, DM and S. In preexisting SC mode, writes preserve the upper SR byte
while replacing the low sixteen bits, per DSP56300FM section 5.4.1.2 and
table 5-1 (pages 5-10..5-13). Sixteen real long-immediate MOVEC cases verify
normal/SC writes, upper-field and low-field boundaries, reserved bits and
extension-word PC advancement. All nine tests, full Release build, diff
checks and captured GP/nonzero EP replay pass. This preserves mode state
but does not implement the behavioral semantics of SA/RM/SM/CE/CP/DM;
SC pipeline delay, SR source transfers, stack/exception width and full
DSP/audio acceptance remain open.


The shared MOVE source reader now clears the transferred upper byte for
SR/OMR sources in SC mode while retaining the source register. DSP56300FM
table 5-1 page 5-13 specifies this rule separately from AGU transfers.
Four executed MOVEC SR-to-X0 and MOVEM SR-to-P cases verify normal/SC
widths, source preservation and PC. All nine tests, full Release build,
diff checks and captured nonzero EP replay pass. OMR's wider fields are
still absent, and PCU LA/LC/stack widths, SC pipeline timing and complete
DSP/audio acceptance remain unproven.


A fresh 55-second isolated MM3 run after the accumulated ISA/AGU/SR fixes
(movie_skip_20261008_084938_472, owned PID 29412) skipped all three intros
and reached AttractMode2. Frame index 4000 was captured separately for GP
and EP under conformance_tmp/dsp_frame_20261008_status_width. Both frames
replay exactly through adapted and unchanged pinned xemu CPUs: GP PC 2B,
458 cycles/8 transfers; EP PC 30, 15856 cycles/8 transfers. These engines
share the DMA/peripheral wrapper. EP output contains 1024 PCM bytes, 976
nonzero bytes, peak 2446, channel RMS 834.641/861.944 and SHA256
d71c291a9a0f63640d3cec87029691593cdb74223ded63a9cd527dbeb0093f67.
The artifact manifest records hashes and transfer events. The harness ended
normally at its limit and removed its owned process. This verifies fresh
real guest execution and nonzero output after reset, not paired GP/EP
instantaneous audio, sustained gameplay, audible quality or full-APU
independent equivalence. Native audio endpoint failure remains a separate
acceptance gap; no complete DSP/audio claim follows from the capture.


LA/LC register transfers now share the normal 24-bit/SC 16-bit width rule
with AGU transfers, for both destination writes and source moves. The
inherited static 16-bit masks discarded valid normal-mode loop addresses
and counts. DSP56300FM sections 5.4.4.2/5.4.4.3 define both registers as
24-bit; table 5-1 defines SC transfer truncation. Forty-eight executed MOVEC
loads/reads cover both registers, both modes and six width boundaries,
checking source/SR preservation and PC. All nine tests, full Release build,
diff checks and fresh nonzero EP captured replay pass. Hardware-loop update,
zero-count semantics, stack save/restore, SC pipeline and broader audio
acceptance remain incomplete; wider register transfers alone do not prove
24-bit looping.


REP register/memory count initialization and the CPU's REP/DO decrement
paths now use the selected LC width instead of unconditional 16-bit wrap.
Ten full REP programs exercise counts 1, 2, FFFF, 10001 and 20001 in normal
and SC modes. They execute ADD on every iteration, check held/advanced PC,
final accumulator count, saved LC restoration and source preservation.
Normal-mode counts beyond FFFF now complete their full iteration count;
SC truncation remains. All nine tests, full Release build, diff checks and
fresh GP/nonzero EP replays pass. DO initialization/stack restoration,
zero-count semantics, maximum-count boundary behavior and loop/interrupt
interaction remain incomplete; the DO decrement change is not full DO
conformance. Complete DSP/audio acceptance remains open.


REP SSH now consumes the source through the existing stack-pop operation
instead of reading SSH without decrementing SP. A real MOVEC-to-SSH,
REP-SSH and repeated-ADD sequence checks stack depth, count, result,
PC and saved LC restoration. The full REP count matrix also executes
zero-count programs in both modes, confirming 65,536 repeats. The explicit
REP specification (DSP56300FM page 13-160) requires this zero behavior;
it is retained rather than applying the general DO zero-count description.
All nine tests, full Release build, diff checks and fresh nonzero EP replay
pass. Fetch-once/noninterruptible REP behavior, stack width and DO/loop
edge cases remain incomplete; this is not complete loop or audio acceptance.


### 2026-10-08: preserve status on DO-loop exit

NXP DSP56300 Family Manual Rev. 5, ENDDO (13-67), specifies restoration of LF only; the remaining saved SR is purged. Normal loop completion and ENDDO previously cleared SR to its lowest seven bits before restoring LF, losing sticky S and mode bits. Both paths now clear only LF before restoring it.

Four uploaded-program regressions cover normal three-iteration DO and early ENDDO in normal and compatibility modes, preserving S, interrupt priority and high mode bits while restoring LA/LC and stack depth. All nine Release CTests pass; recorded MM3 GP/EP frames still match their captured final states (458/15856 cycles, eight transfers each). The pinned upstream CPU remains unchanged; these new cases are excluded from its test target because it retains the defect. This does not establish full DO/PCU conformance: wide loop initialization, full-width stack saves, zero-count and forever-loop behavior remain open.


### 2026-10-08: DO zero and full-width source counts

NXP DSP56300 Family Manual Rev. 5, DO (13-56 through 13-58), specifies zero iterations for an initial zero count with SC clear, versus 65536 with SC set. All four DO source forms now use a shared empty-loop exit that restores saved LA/LC and LF without executing the body. Register and memory sources use the active LC width; the absolute loop address uses the active LA width. Tests execute 22 complete uploaded loops across immediate/register/short-memory/indirect-memory sources, both modes, and counts zero, one and 0x10001 where encodable. All nine Release CTests pass. This does not repair DOR, DO FOREVER, full-width stack saves, source-order corner cases or pipeline timing; those remain separate conformance gaps.


### 2026-10-08: relative-loop execution

NXP DSP56300 Family Manual Rev. 5, DOR (13-61 through 13-64), specifies PC plus a 24-bit displacement, normal-mode zero skipping and compatibility-mode zero wrapping. The formerly missing short/indirect memory DOR handlers now reuse the corresponding DO handlers with relative addressing selected by the encoding. Existing immediate/register DOR paths now preserve active LA/LC widths and use the shared empty-loop exit. The DO/DOR count matrix executes 44 programs plus eight high-address zero-skip cases. Missing execution rows decrease from 38 to 36. DOR source ordering for SP/SSL, full-width stack saves, forever loops, timing and disassembly remain open; these tests do not prove complete arbitrary-program support.


### 2026-10-08: DO/DOR controller-source ordering

The DO/DOR descriptions and implementation notes (NXP DSP56300 Family Manual Rev. 5, 13-56/57 and 13-63) require the source count after the first LA/LC push but before the second PC/SR push and LF update. DOR previously read SP/SSL/SR after that second push; both register forms overwrote LA before reading LA as the source. Count reads now precede those updates. Twenty executed DO/DOR plus ENDDO programs cover SP, SSL, SR, LA and LC sources in both modes. All nine Release CTests pass; recorded real MM3 GP/EP frames still match exactly and the full Release build succeeds. Full-width PCU stack storage remains a separate known defect.


Fresh 35-second isolated startup: `conformance_tmp/movie_skip_20261008_090321_127`, owned PID 24688, harness exited successfully and process is gone. All three intro movies were skipped. No assertion/exception/fatal match was found in stderr; this is startup evidence only, not menu or gameplay acceptance. Host output is still unavailable: CreateMasteringVoice 0x80070490 and waveOutOpen error 2. No audible-quality claim follows from this run.


### 2026-10-08: 24-bit stack payloads

NXP DSP56300 Family Manual Rev. 5, 5.4.3 (5-16/17), defines a 16-level, 48-bit system stack, with 24-bit SSH and SSL payloads. Pushes and explicit SSL writes formerly truncated payloads to 16 bits. They now retain 24 bits, and SSH/SSL register-width metadata agrees. Twelve normal-mode uploaded programs cover six boundary values through explicit SSH/SSL writes plus RTS and saved LA/LC through DO/ENDDO. All nine Release CTests pass. Stack extension, mode-dependent explicit-transfer semantics and interrupt-return behavior still require separate verification; this fixes payload truncation, not all PCU behavior.


### 2026-10-08: interrupt return and forever loops

Twelve uploaded RTI programs verify full-width return addresses and restored SR mode bits, including SC; no RTI handler change was needed after the stack fix. NXP DSP56300 Family Manual Rev. 5, DO/DOR FOREVER (13-59/60, 13-65/66) and FV (5-12), defines unchanged initial LC, repeated decrements without a zero exit, and stacked/restored FV. Both missing forever-loop handlers now share one implementation; counted-loop setup clears FV and loop exits restore LF/FV. Twelve programs exercise absolute/relative forever loops in both modes, counts zero/one/two, five iterations through zero and wrap, and ENDDO restoration. All nine Release CTests pass. Missing-handler rows decrease to 34. Nested mixed counted/forever loops, interrupt delivery, stack extension and timing remain unproven.


### 2026-10-08: nested loops and conditional exit

Sixteen uploaded programs verify all counted/forever inner/outer combinations in normal/compatibility modes and absolute/relative forms, including outer zero crossing, FV/LF restoration and stack unwind. BRKcc now uses the existing condition evaluator and ENDDO unwind, jumping to LA+1 only when taken, per NXP DSP56300 Family Manual Rev. 5, 13-28. Eight programs verify EQ/NE taken/untaken exits for counted and forever loops with full-width saved LC. All nine Release CTests pass. Missing execution rows decrease to 33. BRKcc timing and disassembly are not yet validated; broad audio acceptance remains open.


### 2026-10-08: software interrupt execution

TRAP/TRAPcc now enqueue the existing DSP trap interrupt, per NXP DSP56300 Family Manual Rev. 5, 13-179/180. Both long-interrupt entry sites preserve upper status bits while clearing the specified loop/scaling/priority fields. Six uploaded programs cover unconditional and EQ/NE taken/untaken traps, real vector JSR execution, priority 3, saved full SR and RTI restoration. All nine Release CTests pass. Missing execution rows decrease to 31. This validates the implemented long-interrupt path, not hardware-cycle timing, every conditional code, nested interrupt arbitration or independent full-xemu APU conformance.


### 2026-10-08: REP single-fetch and deferred interrupt

NXP DSP56300 Family Manual Rev. 5, REP (13-160), specifies one instruction fetch and noninterruptible repetition. The interpreter previously reread P memory each iteration. The existing loop_rep field now distinguishes the first fetch from retained execution, keeping the state layout unchanged. Retained execution decodes its saved word without reading or populating the program-memory opcode cache, so a guest P rewrite is observed after REP finishes. Two full programs, one per compatibility mode, rewrite ADD to SUB after the first iteration, enqueue a trap during REP, verify all three ADDs and deferred interrupt delivery, execute vector JSR/RTI, then verify the rewritten SUB is visible. All nine Release CTests pass. Retained-word lookup cost still needs final profiling; no JIT added. REP timing and every opcode form remain unproven.


### 2026-10-08: immediate signed multiply accumulation

MACI now shares MPYI operand decoding and the existing signed fractional multiply / 56-bit add helpers, per NXP DSP56300 Family Manual Rev. 5, MACI (13-101). Two thousand instruction cases compare accumulator words with independent signed-integer product-plus-accumulator arithmetic across all four source registers, both destinations/signs, five signed 24-bit boundaries and five accumulator boundaries. Cases also verify PC extension consumption, overflow and retained carry/high status bits. All nine Release CTests pass. Missing execution rows decrease to 30. SA, DM, saturation and rounded multiply forms remain unproven or missing; this establishes normal 24-bit MACI arithmetic only.


### 2026-10-08: shared rounding-mode correction

The existing rounding helper ignored SR.RM and always corrected half ties to even. NXP DSP56300 Family Manual Rev. 5, RND (13-163/164) and RM (5-11), defines convergent rounding with RM clear and two's-complement rounding with RM set. The shared helper now performs the specified add, optional tie correction and low-bit clearing with unsigned 56-bit storage. Three hundred sixty real RND cases use independent quotient/remainder rounding across both modes, both accumulators, all three nonreserved scaling modes, sign/wrap boundaries and below/exact/above-half fractions. All nine Release CTests pass. This affects existing MPYR/MACR callers as well as RND. SA rounding positions, saturation and full flag conformance remain open; rounded immediate handlers have not yet been enabled.


### 2026-10-08: rounded immediate multiply forms

MPYRI and MACRI now reuse MPYI/MACI operand handling and the corrected shared rounding helper, per NXP DSP56300 Family Manual Rev. 5, 13-143 and 13-105. The immediate multiply matrix now executes 28000 cases across MPYI/MACI/MPYRI/MACRI, both signs/destinations, all source registers, boundary operands/accumulators and both rounding modes with three nonreserved scaling modes for rounded forms. Independent signed product, modular addition and quotient/remainder rounding predict accumulator values. Prior accumulator contents are nonzero for multiply replacement checks. Normal 24-bit behavior is covered; SA/SM/DM behavior and final timing still remain acceptance gaps. Missing execution rows decrease to 28.


### 2026-10-08: power-of-two multiply forms

MPY/MPYR/MAC/MACR S,#n now reuse the immediate multiply path, decoding the QQ source order Y1/X0/Y0/X1 and the positive coefficient 2^-n without an extension word. NXP DSP56300 Family Manual Rev. 5, Table 12-16 (12-21/22), lists n=1..22; other encodings are not silently assigned arbitrary coefficients. The arithmetic matrix now runs 151200 cases, including all 22 coefficients and rounded/unrounded replacement/accumulation. All nine Release CTests pass. Missing execution rows decrease to 24. SA/SM/DM behavior, disassembly and timing remain open, and full MM3 audio acceptance is not implied by these cases.


### 2026-10-08: incorporated source inventory

[SOURCE_INVENTORY.md](SOURCE_INVENTORY.md) records all eleven incorporated source headers, exact pinned upstream paths and local/upstream SHA-256 hashes, plus the additional dsp_c.c origin. Both supplied license texts were verified against pinned xemu after line-ending normalization. Production linkage and the shared-wrapper reference boundary were read back from CMake. This documents incorporated source licensing/provenance; no distribution package or complete audio acceptance is certified.


### 2026-10-08: mixed signed/unsigned multiply

MPYsu/MPYuu and MACsu/MACuu now execute all sixteen encoded source pairs, both signs and both accumulators. Operand pairing and signedness follow NXP DSP56300 Family Manual Rev. 5, Table 12-16 (12-21/23), MPY(su,uu) (13-139) and MAC(su,uu) (13-102). They share result/flag storage with immediate multiplication. Independent integer arithmetic checks 19200 instructions across boundary operands, accumulator replacement/addition and register aliasing. All nine Release CTests pass. Missing execution rows decrease to 22; SA/SM/DM, complete flags, timing and full audio acceptance remain open. Local inventory hashes were refreshed after these changes.


### 2026-10-08: DMAC normal arithmetic

DMAC now shares mixed multiplication/result storage, handles ss/su/uu encodings, and arithmetically shifts the accumulator 24 bits before accumulation. NXP DSP56300 Family Manual Rev. 5 section 3 distinguishes normal 24-bit shifting from SA 16-bit shifting; the DMAC instruction's operation table prints 16 despite its normal-mode description saying 24. This implementation follows the normal-mode description and ALU chapter. Reserved signedness encoding is rejected. The mixed/DMAC matrix executes 33600 instructions, including 14400 DMAC cases with independent signed-shift and product arithmetic. All nine Release CTests pass. Missing execution rows decrease to 21; SA, saturation, full flags and timing remain unproven. Local inventory hashes refreshed.


### 2026-10-08: bit-field extraction

All four EXTRACT/EXTRACTU immediate/register control forms execute normal 56-bit extraction and signed/unsigned extension, per NXP DSP56300 Family Manual Rev. 5, 13-70 through 13-73. Control and source operands are captured before destination writes, preserving aliases. V/C clear; shared E/U/N/Z logic updates those flags. Independent bit-by-bit oracles verify 446880 instructions covering every nonzero width/offset fitting in 56 bits, all control registers, both accumulators, signedness and five source patterns. All nine Release CTests pass. Missing execution rows decrease to 17. Zero-width behavior, SA control layout, full flag conformance, disassembly and timing remain open; the handler count is not a conformance proof.


### 2026-10-08: bit-field insertion

Both INSERT forms now share extraction control decoding and flag updates. Normal-mode fields up to 24 bits replace only selected destination bits; source/control/destination aliases are read before writes. Semantics follow NXP DSP56300 Family Manual Rev. 5, 13-78/79. Independent bit-by-bit replacement verifies 448560 executed instructions across six source registers, six control registers plus immediate control, both destinations, every nonzero fitting width/offset and five destination patterns. All nine Release CTests pass. Missing execution rows decrease to 15. SA layout/bias, zero width, full flags, disassembly and timing remain gaps. Inventory hashes refreshed.


### 2026-10-08: control-word merge

MERGE concatenates the source low 12 bits with the destination middle-word low 12 bits in normal mode, preserving the extension/low accumulator words and updating only N/Z/V, per NXP DSP56300 Family Manual Rev. 5, 13-108/109. The description's 12-bit halves are used; its operation table prints source bits 7:0 inconsistently. Independent bit assembly verifies 393216 instructions across every low half, all six sources/destinations and source boundaries. A three-instruction MERGE/INSERT/EXTRACT program constructs a width-five/offset-eleven control and inserts/extracts signed -11. Missing execution rows decrease to 14. SA form, disassembly and timing remain open; no full arbitrary-program/audio acceptance is claimed.


### 2026-10-08: leading-bit count

CLB scans all 56 source bits, returns nine minus the leading sign-bit count in the destination middle word, sign-extends its extension, clears its low word, and handles the all-zero exception, per NXP DSP56300 Family Manual Rev. 5, 13-42/43. Exact N/Z/V updates preserve the remaining SR. A constructed-leading-prefix oracle verifies 1792 instructions across every count, both signs, both source/destination accumulators and four tail patterns, including aliases and all-one/all-zero input. All nine Release CTests pass. Missing execution rows decrease to 13; SA, NORMF, disassembly and timing remain open. Inventory hashes refreshed.


### 2026-10-08: fast normalization

NORMF now captures its signed source count before accumulator writes, shifts left for negative counts and arithmetically right for nonnegative counts, and preserves Carry. Normal-mode range -55..56 follows NXP DSP56300 Family Manual Rev. 5, 13-147/148; the 56-bit right-shift boundary is handled explicitly. Shared shift helpers supply normal V/L behavior; E/U/N/Z are updated. Independent one-bit shift oracles verify 16128 instructions covering all six source registers, accumulator aliases, every valid count and sticky flags. Another 110 CLB/NORMF guest programs verify normalized bit 47/46 polarity for both signs. All nine Release CTests pass. Missing execution rows decrease to 12. SA, saturation, disassembly, cycle timing and full audio acceptance remain open. Inventory hashes refreshed.


### 2026-10-08: Viterbi split-memory store

VSL now stores the raw accumulator middle word to X:ea and the low word shifted left with the encoded input bit to Y:ea, preserving the accumulator and SR, per NXP DSP56300 Family Manual Rev. 5, 13-182. Existing EA calculation handles address updates and absolute extension consumption; immediate-data EA is rejected. 2280 real instructions cover both accumulators, input bits, normal/compatibility modes, all seven indirect modes across eight Rn registers plus absolute addressing and five data boundaries. Tests check X/Y output, adjacent guards, source/SR preservation and PC/Rn changes. All nine Release CTests pass. Missing execution rows decrease to 11. SA-specific data semantics, broader bus conformance, disassembly and timing remain open. Inventory hashes refreshed.


### 2026-10-08: fresh accumulated-interpreter replay

An isolated 55-second startup run, movie_skip_20261008_094359_233 (owned PID 33328, stopped by the harness), skipped three intros and reached AttractMode2. Frame-index-4000 captures in conformance_tmp/dsp_frame_20261008_vsl replay exactly with both the adapted CPU and pinned xemu CPU: GP PC 0x2B, 458 cycles, eight transfers; EP PC 0x30, 15856 cycles, eight transfers. Both comparisons check outgoing payloads and full final state. EP output contains 1024 PCM bytes, 512 nonzero signed samples, peak 6474, channel RMS 3719.984/3574.142; SHA256 860c8377a31a2f90fcd80c977dd4bc6e24ab6ae6e056856d3ba27f5123cfe172. This is fresh guest execution after the accumulated ISA changes, not audible or independent full-APU proof: reference replay shares adapted peripherals/DMA, and startup still reports XAudio2 80070490 and waveOutOpen error 2. No assertion/exception/fatal log match was found. Nine Release CTests and submodule diff-check pass. Representative gameplay, sustained output and arbitrary-program acceptance remain open.


### 2026-10-08: peripheral and endpoint investigation

CPU peripheral callbacks carry an address but no memory-space argument. X invokes them; Y asserts against YRAM size. Y MOVEP requires a specified Y interface/map; no unsupported alias to X was implemented. Read-only Windows inspection found AudioEndpointBuilder/Audiosrv running, but all four registered AudioEndpoint PnP instances report Present=False (WH-1000XM5 and Creative Stage SE mini). This supports a missing-endpoint diagnosis, not exhaustive audio API availability proof. The APU README now replaces stale GP/EP-stub claims and bypass diagram with real guest execution, native output and acceptance/license references; stale LOC counts were removed.


### 2026-10-08: preserve partial EP output through monitor submission

EP FIFO0 previously staged partial PCM directly in monitor.frame_buf, which the native output path clears after submission. A descriptor split across monitor submissions could lose its first half. A separate one-frame EP staging buffer now retains partial output; only a successfully completed frame is copied into the monitor output buffer. The existing offset/reset and failed-transfer behavior are retained. A focused APU regression writes two 512-byte halves through the real FIFO callback, checks no partial publication, clears the monitor output between halves, and verifies the complete 1024-byte payload afterward. Nine Release CTests and full isolated MM3 Release build pass; source inventory hashes refreshed. This removes the partial-output loss mechanism but does not implement a queue for transfers spanning multiple completed output frames or prove audible stability.


### 2026-10-08: reset during frame capture

Reset now closes an active diagnostic frame capture and restores its original RAM/FIFO callbacks before CPU/DMA reset. The interrupted artifact deliberately has no completion trailer and final state. A new reset regression captures a valid uploaded halt program before execution, resets it, verifies the file is closed at header-plus-initial-state length, checks callback restoration and invokes the real FIFO callback afterward. Replay rejects that artifact specifically for a missing completion trailer (exit 1). Ten Release CTests and full isolated MM3 Release build pass; inventory hashes refreshed. Capture remains a diagnostic local ABI format, not a portable save state or full host-MMIO journal.


### 2026-10-08: live replay after EP staging/reset fixes

The current full Release executable ran in isolated movie_skip_20261008_095055_984 for 55 seconds (owned PID 3524, harness completed and process absent), skipped all three intros and reached AttractMode2. New frame-index-4000 captures in conformance_tmp/dsp_frame_20261008_ep_staging match outgoing payloads and final state with both adapted and pinned CPU replay: GP PC 0x2B / 458 cycles / eight transfers; EP PC 0x30 / 15856 cycles / eight transfers. EP generated 1024 PCM bytes, 512 nonzero samples, peak 1907, RMS 557.920/499.093, SHA256 27c66e82ad6b398d2c7680099e3c3cca5d0392c11b3f9096b790e9719e005720. The native monitor review confirmed partial staging is now separate from its cleared output buffer. Remaining native-output gaps include dropped submissions when XAudio2 is full and waveOut overwriting an in-queue buffer after its wait timeout; these need focused validation and fixes. Host endpoint creation still fails (80070490 / waveOut error 2), so this capture proves current guest execution, not audible stability, representative gameplay or full APU equivalence.


### 2026-10-08: native output backpressure

XAudio2 submission now distinguishes accepted (1), full queue (0), and unavailable/failed output (-1). The native monitor waits for queue capacity without discarding its frame, releasing the APU lock while waiting and checking shutdown/pause after reacquisition. A critical engine callback sets an interlocked error flag so endpoint loss cannot leave this wait treating a failed engine as a full queue. The existing backend mock regression checks failed submission preserves ring position/queued contents, full capacity does not overwrite queued samples, FIFO completion permits wraparound, and critical error returns failure. waveOut no longer overwrites an in-queue buffer after a 50-ms timeout: it waits for completion with shutdown/pause checks, and only clears/advances its output after waveOutWrite succeeds. A permanently stalled waveOut device can hold the worker until pause/shutdown; device recovery remains open. Backend mock CTest passes 1/1, DSP CTests 10/10, full MM3 Release build and diff-check pass. These checks do not exercise the full monitor waiting loop against a real endpoint; audible output, shutdown under device stall and sustained gameplay remain acceptance gates. No JIT or DSP substitution added.


### 2026-10-08: native submission length validation

XAudio2 previously copied a signed negative frame count as an oversized byte count and silently truncated requests larger than its slot. Submission now rejects null input, zero/negative counts and counts above the advertised slot capacity before touching queue storage. The existing backend regression checks all four invalid cases leave queue count, ring position and written-frame count unchanged. Its valid 1024-frame submission/ring/error cases continue to pass; the production monitor submits 256 frames. Backend CTest 1/1 and full MM3 Release build pass. Inspection confirms wait-loop cancellation occurs after APU lock reacquisition, but no full monitor cancellation-under-stall test has yet been performed. This validates the native input boundary, not audible output or full DSP acceptance.


### 2026-10-08: explicit unsupported DMA-format failure

Packing format 0 was decoded as one-byte items but every transfer path asserted on that item size; formats 3/4/5/7 also asserted during format decoding. These unimplemented formats now set DMA error and stopped state before any payload callback, writeback or EOL publication. A focused regression checks all five formats preserve scratch bytes and descriptor offset, perform no transfers, and never publish completion. This is an explicit failure path, not implementation of those formats or hardware error-register conformance. Arbitrary-valid-program acceptance still requires the valid missing formats and their independently verified packing. Ten CTests and full MM3 Release build pass; source hashes refreshed.


### 2026-10-08: production monitor backpressure regression

Added a device-free monitor test beside the existing XAudio2 backend test. CMake extracts the current mcpx_apu_monitor_frame body from apu_core.c into a build-only include and reconfigures when that source changes; the test compiles that body rather than maintaining a copied wait algorithm. Simulated output APIs check XAudio2 retries retain the exact payload until accepted, shutdown during full-queue waiting restores the caller lock and preserves output, waveOut waits through 60 completion polls without touching queued storage (beyond its removed 50-poll limit), shutdown cancels stalled waveOut without submission, and both output-error paths preserve pending data. Both native-output CTests pass (2/2). This closes the missing deterministic monitor-loop regression gate, not real endpoint timing, actual concurrent lock scheduling, device recovery or audible stability.


### 2026-10-08: monitor entry after shutdown request

The production-body monitor regression demonstrated a failing case: entering with exiting already set still mixed and submitted an output frame when the device was immediately ready. An entry guard now returns before mixer/output work when shutdown is requested. Regression checks both XAudio2 and waveOut preserve pending PCM, invoke neither mixer nor submission, and retain the caller lock. The added regression failed before the guard and passes afterward; both native-output CTests and the full MM3 Release build pass. This supplements cancellation during waits; actual concurrent teardown/device-stall behavior still requires live verification.


### 2026-10-08: keep engine-error callback nonblocking

The XAudio2 critical-error callback previously wrote stderr directly on the audio processing thread. It now only atomically records the HRESULT; the APU submitting thread reports that error once and returns explicit submission failure. This follows Microsoft callback guidance (https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-callbacks): callback work must not block on storage or expensive synchronization. The existing backend regression verifies the callback records the exact error without marking it reported, subsequent submission reports it, repeated submission remains failed, and queued data stays unchanged. Native-output CTests 2/2 and full MM3 Release build pass. This removes callback-side logging; critical device-error recovery and live audible validation remain open.


### 2026-10-08: EP transfers spanning native output frames

Removed the one-frame FIFO0 length rejection. EP output now assembles full 1024-byte PCM frames, retaining additional complete frames in a contiguous queue and a trailing partial frame in separate staging. Queue capacity is reserved before guest FIFO writes; failed writes never publish their payload. Only successful native submission consumes the current frame and advances the queued output, preserving FIFO order; EP reset clears queued/partial/ready state and shutdown frees queue storage. The focused APU regression transfers 2560 bytes through the real FIFO callback, verifies first/second-frame order, then completes the third frame with a separate 512-byte transfer. Existing partial, failed-DMA and released-reset tests remain green. Ten DSP CTests, two native-output CTests and full MM3 Release build pass. Queue data is native staging, not DSP state/capture ABI. The direct callback regression does not independently prove hardware timing or sustained output; production exceeding native consumption can grow the queue, especially with no host endpoint. Bounded pacing/recovery and representative audible gameplay remain required.


### 2026-10-08: executed guest multi-frame DMA regression

Strengthened the EP multi-frame regression to execute the uploaded guest program through mcpx_apu_dsp_frame: the program starts a 1280-word 16-bit FIFO0 DMA descriptor (2560 bytes), halts, then executes again with a 256-word descriptor to complete the trailing partial frame. MMIO uploads the descriptor and input words. Checks verify guest instruction progress, halt, DMA EOL, all three native frame payloads and ordering, and partial-frame continuity across guest invocations. The test no longer relies on a direct callback for the multi-frame case. The production-body monitor check now also counts consumption notifications: exactly one after successful output and none during queue-full cancellation or failed XAudio2 submission. Ten DSP CTests and two native-output checks pass. Queue-growth/backpressure, independent hardware timing, live endpoint output and full arbitrary-program acceptance remain open.


### 2026-10-08: bounded native staging independent of guest DMA

Pending EP monitor PCM is now capped at 64 complete frames (64 KiB, approximately 341 ms at 48 kHz), plus the current/partial frame. Excess complete native frames are counted in ep_pcm_dropped_frames and the first overflow is logged; guest FIFO RAM writes and DMA completion still execute. Native allocation failure likewise no longer manufactures a guest DMA error: available staging is used and native loss is counted separately. A stalled-consumer regression executes 70 additional guest DMA frames without consuming output, verifies all instructions/transfers complete without DMA error, queue storage remains bounded, exactly six native frames are counted dropped, and the current unconsumed frame is preserved. Ten DSP CTests and full MM3 Release build pass; source hashes refreshed. This is bounded failure handling, not lossless audio under sustained consumer starvation or an acceptance claim. Normal representative gameplay must demonstrate zero staging loss with a working endpoint, and pacing/recovery remains required.


### 2026-10-08: injected native staging allocation failure

The APU test now compiles the actual apu_dsp.c through a nine-line test wrapper that replaces only realloc, leaving the production interpreter/DMA/FIFO logic unchanged. Injected native queue allocation failure during an executed guest DMA program verifies instruction progress, actual ring cursor advancement, all 1024 guest RAM bytes, DMA EOL without DMA error, preservation of the current native frame, zero queue allocation and exactly one counted host drop. Restoring allocation then permits the next guest frame to queue and consume normally without another drop. This verifies native OOM does not fake or suppress guest transfer completion. Ten DSP CTests pass; actual native allocator failure/recovery in a sustained game and audible acceptance remain unproven.


### 2026-10-08: current queued implementation with live native endpoint

Windows now reports a present Remote Audio endpoint. The 55-second isolated movie_skip_20261008_131649_896 (PID 28892, harness terminal) opened XAudio2, skipped three intros and reached AttractMode2. New frame-index-4000 GP/EP captures in conformance_tmp/dsp_frame_20261008_queue match transfers and final state with adapted and pinned CPU replay: GP 458 cycles/eight transfers, EP 15856 cycles/eight transfers. EP output has 1024 bytes, 508 nonzero samples, peak 221, SHA256 64f42109e8baa0f57771776e8e9d93409a3101c9ca6eccd9475e9a0ed33a5138.

A separate isolated 100-second run movie_skip_20261008_131853_079 (PID 19564, harness terminal/process absent) captured accepted native submissions in conformance_tmp/dsp_native_20261008_queue/host.pcm and host.wav. It received real USB start, nine A presses and RT input; no fresh visual evidence establishes gameplay, so no driving acceptance is claimed. The host mix contains native movie/mixer audio as well as DSP output: 19017728 bytes / 99.050667 seconds, channel RMS 3571.11/3527.32, SHA256 24766ad6f8fa9c5940aa1d1de1ab73cdde4404c3f8c0423680e10b6b467276b1. One-second silent windows occur at 0..4, 7..8 and 57..60; 4224 clipped samples occur. These need scene correlation and listening, not automatic attribution to a DSP defect or successful quality acceptance. Both runs report no staging overflow, output-submission failure, assertion/exception/fatal log matches. This updates the missing-endpoint finding: native output now opens successfully, while representative audible music/voices/effects/reverb/transitions and full compatibility remain unproven.


### 2026-10-08: fresh visually verified driving and sustained native capture

Current queued/bounded EP build ran in isolated movie_skip_20261008_132407_895 for 180 seconds (owned PID 41732, harness terminal and process absent). Timed capture checkpoints in conformance_tmp/dsp_scene_audio_20261008_queue pair framebuffer dumps with accepted-PCM byte positions. Viewed frame00136.bmp shows playable Washington Standard Delivery with the Cadillac and driving HUD; frame00154.bmp is before RT input at the pizza storefront, and frame00179.bmp shows the car moved behind traffic after a 20-second accelerator hold. This verifies current-build controlled movement in one mission, not just loading/intro proof. Native host capture contains 34384896 bytes / 179.088 seconds, channel RMS 4384.28/3991.60, SHA256 be235b62d305181cdc4e08bf9405ae0828bcb0b82041d04e4ff7060d40cfcd92. Silent one-second windows occur at 0..4, 7..8 and 40..44 (menu/loading interval); none occur after 45 seconds. Clipped samples total 375. No staging-overflow, submission-failure, assertion/exception/fatal log match occurred. host.wav, analysis.json, scene-timeline.json and evidence.json preserve the evidence and limits. Audible quality has not been judged; clipping attribution, music/voice/effects isolation, other scenes/environment transitions, independent full-APU equivalence and arbitrary-program support remain acceptance gates.


### 2026-10-08: explicit fake-completion mutation rejected

NOFAKE_AUDIT.md records the active source completion path, former-switch search and proof limits. In a separate external build, deliberately changed DMA_CONTROL reads to manufacture STOPPED/EOL without transfer. The existing production APU regression fails at its RUNNING assertion with exit 1; production DMA source bytes remained unchanged by hash comparison. Evidence is I:/repos/mm3-dsp-nofake-mutant-20261008/result.json. This demonstrates the focused nofake gate catches read-triggered acknowledgement regressions. It does not establish all peripheral semantics, arbitrary valid programs or environmental audio quality.


### 2026-10-08: command-only and CPU-bypass mutations rejected

Extended external mutation proof to two faulty runtime copies. ack_only clears the command word with a host scratch write, omits the remaining descriptor payload, then fabricates EOL; the production APU regression rejects the missing 0x654321 cross-page payload at guest RAM 0x7000 (exit 1). cpu_bypass advances PC/instruction counts without executing instructions; the regression rejects the missing actual DMA START state (exit 1). Source bytes remain unchanged by hash comparison, and the unmutated dsp_apu CTest passes. Result JSON files under I:/repos/mm3-dsp-nofake-mutant-20261008 preserve failures. NOFAKE_AUDIT.md now records all three mutation classes and their scope limits.

### 2026-10-08: compatibility-mode X peripheral addressing

The shared CPU X-memory read/write paths now alias SC-mode addresses `0xff80..0xffff` to the existing `0xffff80..0xffffff` peripheral callbacks. NXP [DSP56362 User Manual, section 3.1](https://www.nxp.com/docs/en/user-guide/DSP56362UM.pdf) specifies these normal/16-bit peripheral windows and access through MOVE, MOVEP and bit instructions. This applies the shared DSP56300 core addressing rule; it is not independent proof of every MCPX peripheral. Y peripherals remain unsupported. The added regression failed before the fix at the first SC-mode read; afterward all 128 addresses pass direct reads/writes and uploaded indirect MOVE loads/stores in both modes. Normal-mode `0xff80` remains unmapped. All ten DSP CTests pass.

### 2026-10-08: cache-disabled Illegal exceptions

All seven cache-control decoder rows now use one shared handler which raises Illegal when SR bit 19 (CE) is clear; enabled-cache execution remains explicitly unsupported. The explicit ILLEGAL instruction no longer aborts the host when exception debugging is enabled: it queues the existing guest interrupt. The misleading inherited P:$003E comment was corrected to P:$04, supported by [DSP56300FM Table 2-2 and chapter 8](https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf); the instruction-description page itself still mentions the older vector, so the core vector table remains authoritative here. The new production regression failed with a host assertion before the fix, then all sixteen normal/SC instruction cases reached the guest P:$04 vector/handler at IPL 3 with stacked SR preserved. All ten DSP CTests pass. This does not implement an enabled instruction cache or reduce the full compatibility gate.

### 2026-10-08: WAIT core suspension and interrupt wake

WAIT now marks the core waiting/idle, advances once, and suspends subsequent instruction fetch/execution. The existing interrupt arbiter clears this state only when it selects an eligible interrupt; posting a masked request does not wake the core. A request already pending when WAIT executes proceeds through the normal interrupt pipeline. Host frame starts no longer wake a guest WAIT by themselves. Hardware/core reset clears WAIT. The new boolean occupies existing structure padding: the APU regression verifies `sizeof(DSPState) == 80128`, preserving current capture layout. [DSP56300FM WAIT, page 13-183](https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf) is the semantic source. A before-fix regression failed because WAIT did not become idle; CPU and production APU scheduler regressions now pass along with all ten DSP CTests. This is core suspension/wake support, not full low-power conformance: independently clocked peripheral/DMA progression while waiting, external reset/debug wake signals and STOP/RESET instruction effects remain open.

### 2026-10-08: DMA progression while the guest waits

`dsp_has_work` now distinguishes sleeping CPU plus runnable DMA from an inactive processor. `dsp_run` advances actual DMA descriptors while WAIT prevents instruction fetch, and the APU frame worker keeps scheduling bounded batches until that work stops. Frozen/error DMA remains inactive. The regression uploads a real START/WAIT program, freezes DMA, verifies unchanged physical SGE RAM, unfreezes it, verifies one descriptor in a two-cycle budget, then completes the three-node chain with exact physical payloads while PC/A remain asleep and EOL follows transfers. It failed before the fix because the first physical payload stayed unchanged. All ten DSP checks pass. Scheduling remains the existing coarse descriptor-boundary approximation (two-cycle slots while asleep), not measured MCPX bus timing. WAIT/clock rationale: [DSP56300FM page 13-183](https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf). DMA completion does not fabricate a CPU wake; routing MCPX peripheral status to eligible core interrupt sources remains a separate conformance gap.

### 2026-10-08: STOP core/peripheral suspension

STOP now sets a distinct stopped/idle state, suppresses further instruction execution and interrupt arbitration, prevents host frame starts from waking it, and stops the interpreter scheduler from advancing active DMA. Core/hardware reset clears it. Ordinary queued TRAP requests do not stand in for the required external wake signals. The flag uses the remaining pre-cycle_count padding; the APU capture ABI size assertion remains 80128. A pre-fix CPU regression failed because STOP left the core active. CPU tests now cover suspension, queued-request persistence and reset recovery; the production APU test uploads the same START/three-node program used for WAIT but executes STOP, proving all physical destinations remain unchanged through frame restart while DMA stays pending and no EOL appears. All ten DSP CTests pass. Semantic source: [DSP56300FM Stop processing state and STOP instruction](https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf). External IRQA/debug wake, PLL/startup delays and precise instruction-latch/bus ordering remain unsupported, so this is not full STOP conformance.

### 2026-10-08: fresh post-power-state live replay

The full isolated Release executable ran for 55 seconds in `movie_skip_20261008_135100_025` (owned PID 15788, harness terminal exit 0). Fresh frame-4000 GP/EP artifacts in `conformance_tmp/dsp_frame_20261008_power` match both adapted and pinned CPU replay exactly: GP PC 00002B/458 cycles/eight transfers; EP PC 000030/15856 cycles/eight transfers. The submitted native PCM is 10,376,192 bytes (54.0427 seconds, channel RMS 1405.03/1383.13), no clipped samples and silence confined to startup seconds 0-4 and 7-8. No staging-overflow, submission-failure, assertion, exception or fatal log matches were found. `evidence.json`, `analysis.json` and `host.wav` retain the evidence. This intro/attract run checks current execution after the SC/cache-disabled/WAIT/STOP scheduler changes; it does not prove gameplay listening quality or independent full-APU equivalence.

### 2026-10-08: work-aware replay and multi-batch WAIT capture

The frame replay verifier now checks `dsp_has_work` at entry, throughout its bounded loop and at completion. A new production capture fixture uploads START/WAIT plus 512 chained scratch descriptors, verifies the core is asleep with outstanding DMA after the first 1000-cycle batch, then verifies all 512 physical SGE destinations and genuine EOL after the next batch. Before the verifier fix, its idle-only loop stopped early and failed at the end-marker check; afterward it matches all 512 outgoing transfer payloads and the entire final sanitized state. Two CTests retain capture/replay coverage, raising the DSP suite to twelve passing checks. The unchanged pinned xemu CPU has a no-op WAIT, so the new WAIT fixture intentionally tests only the adapted implementation; independent hardware/upstream low-power conformance remains open. The fresh `dsp_frame_20261008_power` GP/EP captures still match both local and pinned CPU replay with the corrected verifier. Production runtime was unchanged this increment.

### 2026-10-08: unsupported DMA buffer routes

The descriptor boundary now rejects unsupported input routes 2..D and output routes 4..D before payload allocation/copy/callbacks, stops RUNNING and records an explicit implementation error. It does not manufacture EOL or offset writeback. A 22-case regression verifies no callback/transfer and unchanged writeback for both directions, then resumes supported transfer tests; before the fix the first unsupported input route aborted the host with assertion exit 0xc0000409. All twelve DSP CTests pass. This is explicit unsupported-operation handling, not implementation of those routes or independent proof of the MCPX hardware error/status response.

### 2026-10-08: DMA payload memory bounds

DMA payload validation now follows implemented XRAM, mixbuffer, YRAM and PRAM extents rather than treating the entire X DMA segment as mapped. The X gap 1000..13FF and unsupported segments are rejected before allocation/transfer; crossing an extent is rejected with explicit error/stopped state and no EOL or writeback. Twenty direction/range regressions failed before the fix with a host XRAM assertion, then pass without callbacks or changed writeback. This bounds the adapted memory map; it does not prove absent external-memory mapping or the real hardware error response. All twelve DSP checks pass.

### 2026-10-08: shared descriptor/payload range validation

The same implemented-memory range decoder now validates both the seven-word descriptor fetch and its payload. This removes duplicated maps and rejects X holes, unsupported segments and descriptors crossing an extent before fetching any words. Nine descriptor-pointer regressions reproduce a pre-fix host assertion from decoding unmapped memory; after the fix they retain NEXT_BLOCK, perform no transfer and report explicit error/stopped without EOL. All twelve DSP CTests pass. The supported memory-map and hardware-error-response limitations from the payload-bounds entry still apply.

### 2026-10-08: legal DMA memory-edge verification

A focused 48-case production regression now complements invalid-range checks. It places descriptors in the final valid seven-word slots of XRAM/mixbuffer/YRAM/PRAM, and separately places one-word payloads at each final valid word. Both directions and implemented formats 1, 2 and 6 complete with exactly one real callback, exact payload values and EOL without error. The checks validate that tighter mapped-memory bounds preserve legal edge transfers; they are local tests, not independent hardware-map evidence. All twelve DSP CTests pass. No production change was needed for these valid cases.

### 2026-10-08: vector base relocation

VBA now occupies its architectural six-bit MOVEC register encoding 0x30, clears its low eight bits on write and supplies the upper vector-address bits during interrupt selection. Internal REP LC-save storage moved to reserved register-array slot 0x2F to avoid corrupting VBA. Sources: [DSP56300FM section 5.4.4.4 and Table 12-14](https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf), PDF pages 101/234. Sixteen uploaded MOVEC/TRAP programs verify vector relocation throughout the implemented PRAM, and a REP program verifies VBA remains intact while LC is restored; reset clears VBA. Before the fix, the MOVEC register-value check failed. All twelve DSP checks and the recent `dsp_frame_20261008_power` local GP/EP replays pass. State size is unchanged, but older raw captures containing nonzero internal REP saves in slot 0x30 require regeneration/format migration; pinned xemu does not implement this VBA/internal-save correction. High external-vector addresses remain outside the implemented PRAM map.

### 2026-10-08: capture state version 2

New captures declare version 2 to distinguish VBA at 0x30 and internal REP-save at 0x2F from prior raw states. Replay rejects version 1 before loading state, with an explicit regeneration message. Because these are local-build ABI artifacts, ambiguous old states are retained as historical evidence rather than silently converted. A complete capture with only its version word changed to 1 is rejected by a new CTest; changing that word back to 2 in an isolated positive-control copy matches full final state, proving the rejection is version-specific rather than truncation. Fourteen DSP CTests pass, and the full isolated Release build passes. A fresh live v2 corpus is being regenerated; prior v1 captures are not claimed runnable under the current verifier.

Fresh v2 corpus completed: `dsp_frame_20261008_v2` GP and EP both match local and pinned CPU replay exactly (GP PC 00002B/458 cycles/eight transfers, EP PC 000030/15856 cycles/eight transfers). Harness `movie_skip_20261008_140937_538`, owned PID 13792, exited 0 after 55 seconds; headers explicitly verify version 2 and state size 80128. `evidence.json` retains hashes and comparison scope. This is intro/attract execution evidence, not full gameplay audio acceptance.

### 2026-10-08: SC-mode VBA transfers

The shared register width and move-source helpers now apply the sixteen-bit compatibility rule to VBA, and the VBA destination write retains only permitted upper-vector bits within that width. [DSP56300FM Table 5-1, SC bit, page 5-13](https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf) explicitly requires MOVE operations to/from VBA to clear the destination upper eight bits in SC mode. Forty-eight uploaded instruction checks cover long MOVEC writes, MOVEC register reads and MOVEM program-memory stores for eight values in normal/SC modes, preserving status and instruction length. The pre-fix test failed on an SC-mode destination value; all fourteen DSP CTests pass after the shared fix. Other PCU registers and sixteen-bit arithmetic remain separate conformance gaps.

### 2026-10-08: OMR register width and SC transfers

OMR moves now retain DSP56300 twenty-four-bit fields instead of the inherited eight-bit/c7 mask. Writes clear reserved bit 5; SC writes preserve the old upper byte while source moves continue clearing the destination upper byte. Sources: [DSP56300FM section 5.4.1.1/Table 5-2 and SC definition](https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf), PDF pages 85-89/93. Twenty-four uploaded MOVEC write/read cases cover six values in normal/SC mode; the pre-fix destination-value assertion failed and all fourteen DSP tests pass afterward. This implements register storage/transfers, not extended-stack, bus arbitration, patching, memory-switch or cache-burst behavior. MCPX-specific availability of optional family fields remains unverified.

### 2026-10-08: immediate control-byte logic

ANDI/ORI now share selected-byte logic for MR, CCR, COM and EOM. ANDI preserves unrelated upper bytes instead of applying an inherited sixteen-bit mask; EOM is implemented instead of falling through. Shared register writes retain SR/OMR reserved-bit and SC rules. Source: [DSP56300FM instruction partial encodings, Table 12-13](https://www.nxp.com/docs/en/reference-manual/DSP56300FM.pdf). A 4096-case executed regression covers every immediate byte, all four targets, both operations and normal/SC modes, checking the changed register and untouched counterpart. Before the fix the value check failed; all fourteen DSP CTests pass afterward. This corrects register operations without claiming implementation of every mode those bits select.

Control-byte trace follow-up: ANDI/ORI disassembly now names all four selected bytes (MR, CCR, COM, EOM), removing the missing EOM output and misleading whole-OMR label. A private-formatter test exercises all 2048 operation/target/immediate combinations through opcode decoding and checks text, instruction length and unchanged PC. Fifteen Release DSP CTests and the full isolated MM3 Release build pass. This diagnostic correction does not close arbitrary-program, representative gameplay audio or finished profiling acceptance.

Software RESET now clears modeled peripheral storage, MCPX frame/DMA interrupt status and DMA registers/error/EOL through the existing peripheral callback. Hardware reset reuses the same peripheral reset helper. CPU registers, stack, uploaded memories and subsequent PC are retained; DMA callbacks remain attached, and resetting a running engine prevents a scheduled post-instruction transfer or fabricated EOL. The callback event uses UINT32_MAX outside the 24-bit guest address domain, so state/capture ABI remains unchanged. An executed APU regression checks these modeled reset invariants. Source: DSP56300FM Rev. 5, 13-162 (PDF page 401). Full device-specific peripheral reset values, unmodeled interrupt priority registers and cycle timing remain unproven; the four implemented interrupt sources are the exceptions preserved by RESET. Fifteen DSP Release tests pass.

RESET cancellation proof strengthened: the executed integration regression now uses a valid physical SGE descriptor with a canary destination in both runnable and pre-faulted states. RESET cancels the pending transfer, retains uploaded X/Y/P data and descriptor contents plus all DMA callbacks, and the following ADD executes. Starting that exact descriptor afterward writes 0x123456 to the canary destination and completes normally, providing a positive control for the no-write result. All fifteen Release DSP CTests pass.

Fresh post-RESET runtime: `dsp_reset_runtime_20261008_142355` from `movie_skip_20261008_142355_717` (owned PID 22568, terminal exit 0 after 55 seconds) matches local and pinned CPU replay for GP (PC 2B, 458 cycles, eight transfers) and EP (PC 30, 15856 cycles, eight transfers). Submitted native PCM spans 54.107 seconds with channel RMS 1405.72/1383.77, 0 clipped samples and 0 matching error log lines; hashes and silence windows are retained in evidence.json. Both native-output regressions also pass. This is intro/attract evidence, not representative gameplay or independent full-APU equivalence.

Unsupported DMA control actions 5 (ABORT), 6 and 7 now report an explicit error/stopped state rather than asserting on the host. Six regressions cover existing EOL clear/set states, frozen/running status and preserved descriptor/configuration registers; stepping with null callbacks verifies no payload access. Existing EOL is retained, never manufactured. This is failure containment, not implemented ABORT hardware semantics, and arbitrary valid program support remains incomplete. Fifteen DSP Release CTests pass; inventory hashes refreshed.

Current-build gameplay run `dsp_gameplay_20261008_142657` / `movie_skip_20261008_142657_855` ended at the 180-second watchdog (owned PID 48660 absent afterward, harness exit 0). Captured profile, Washington Standard Delivery, movement to a different street after acceleration, pause, and resumed gameplay with delivery counter 1/14. Submitted PCM spans 179.125 seconds, RMS 6332.16/6248.68, 5566 clipped samples, 0 matching error lines; evidence.json retains hashes/silence windows. IMPORTANT: frame-15000 GP local replay matches (55612 cycles, 106 transfers), but pinned CPU replay fails final state at byte 146 (00 versus ff). EP local/pinned replay matches. The new gameplay corpus exposes a comparison gap requiring diagnosis; it must not be reported as full xemu equivalence. Listening quality, isolated music/voice/environmental coverage and final profiling remain open.

Gameplay GP mismatch diagnosed without weakening comparison: all 106 outgoing payloads, final PC 2B and 55612 cycles match. Exactly eight raw state bytes differ. M0-M5 high bytes reflect pinned static 16-bit register-write masks versus the adapted normal-mode 24-bit masks; first observed M0 write is PC 50E/opcode 05F420. Slot 0x30 is upstream internal REP-save (1) versus architectural VBA (0); captured internal save is separately in 0x2F (1). SSL stack slot 4 contains pinned 000010 versus captured 100010 because upstream masks stack words to 16 bits; last local write PC F73/opcode 062090. Isolated instruction traces and gp-mismatch-diagnosis.json preserve the evidence. Earlier wording calling this a stack-status word was premature: SSL also carries saved loop counts. These are differences in known register/state adaptations, not an outgoing PCM mismatch. Full raw upstream replay remains FAILED; arbitrary-program and independent full-APU equivalence remain unproven. Replay diagnostics now report the differing-byte count and register values plus at most sixteen byte differences, without filtering any comparison.

Gameplay clipping investigation: clipping-analysis.json records every affected one-second PCM window. The largest windows include seconds 114 (1122 clipped samples), 112 (972) and 155 (798). The captured EP FIFO0 payload is unclipped (peak 8008) and appears byte-for-byte at host PCM offset 15361024 (80.0053 seconds), so that frame passed through unchanged. XAudio2 submission copies samples, but mcpx_apu_monitor_frame calls mixer_render before submission, so other clipped windows cannot yet be attributed solely to guest DSP output. Full host.wav and a 110-116 second clipping excerpt are retained for listening and further comparison. No gain reduction or fabricated effect was added. Paired pre-mix/post-mix capture during a clipped frame remains necessary to determine origin.

Native retry bug fixed: mcpx_apu_monitor_frame previously called mixer_render again after a failed submission or canceled queue wait, adding native voices to an already mixed frame and advancing voice offsets twice. A monitor regression invoking failure then success failed before the fix. A per-monitor frame_mixed flag retains that frame across retries; consumption and EP reset clear it. New EP FIFO payloads queue behind retained mixed native-only output rather than overwriting it, covered by two actual 512-byte guest FIFO writes and subsequent promotion. Both native-output tests, all fifteen DSP tests and the full isolated Release build pass. DSPState/capture ABI is unchanged. This does not explain the earlier gameplay clipping by itself, since that run had no logged submission failure; paired capture and representative listening remain required.

Retry regression expanded for canceled XAudio2/waveOut waits followed by successful retry, and a subsequent newly mixed frame. Both native-output tests pass and full isolated Release build passes. Opt-in RECOMP_APU_PREMIX_DUMP records raw s16le stereo 48kHz before native mixing, once per new pending frame rather than per retry; compare with RECOMP_XA2_PCM_DUMP only after validating counts/reset/error history. Short isolated run `dsp_pcm_pair_20261008_143953` / `movie_skip_20261008_143953_840` (owned PID 41404, terminal harness exit 0) produced 10362880 premix bytes and 10362880 submitted bytes; equal counts=True, identical payloads=True, matching error lines=0. Hashes are in evidence.json. This establishes capture alignment for intro/attract; gameplay clipping attribution remains open.

Paired gameplay capture `dsp_gameplay_pair_20261008_144122` / `movie_skip_20261008_144122_311` ran to the 180-second watchdog (owned PID 31668, terminal harness exit 0). Visuals verify Washington Standard Delivery, acceleration to a different location with delivery 1/14, pause and resumed gameplay. Premix/submitted streams each contain 34372608 bytes (179.024 seconds), identical payloads=True, matching error lines=0. There are 21354 full-scale samples in the premix stream. For this run, native mixer/output did not introduce those samples: whole streams are identical. This locates the observed clipping upstream of native mixing but does not prove original-hardware behavior, correct DSP arithmetic or audible quality. DSP/input-side comparison is the next gate. evidence.json retains hashes and frame references.

Added opt-in RECOMP_DSP_CAPTURE_CLIPPED selection: after the chosen frame index, EP capture rewrites complete candidate files until a successful FIFO0 output contains a signed-16 full-scale sample, then retains that whole initial-state/transfer/final-state frame. GP selection stays unchanged. Guest execution and payloads are unchanged; this is a diagnostic trigger, not a guest effect substitution. A three-frame regression emits ordinary/full-scale/ordinary samples and verifies the middle frame remains captured; local replay passes. Seventeen DSP tests pass. Mixbin float_to_24b uses the same saturation thresholds and nearest-integer conversion as pinned xemu; input amplitude and clipped guest-frame comparison remain unverified.

Clipped gameplay EP comparison completed: `dsp_clipped_gameplay_20261008_144746` / `movie_skip_20261008_144746_484` (owned PID 27508, terminal harness exit 0 at 120 seconds) visually reached Washington Standard Delivery and moved to a different street with delivery 1/14. Triggered EP output contains three full-scale samples in 1024 bytes. Local and pinned CPU replay both match all outgoing transfers and complete final state (PC 30, 15856 cycles, eight transfers). That exact payload appears at submitted PCM byte offset 12151808; premix/submitted whole streams are identical (22857728 bytes) with 0 matching error lines. This proves the observed selected EP full-scale output is reproduced by pinned xemu CPU with identical input; shared DMA/peripherals, GP processing, input amplitude, original-hardware behavior and audible acceptance remain separate gates. evidence.json retains hashes and visual references.

GP-only retention repaired: direct GP monitor writes previously overwrote an already mixed pending host frame. An executed regression failed on the first overwritten byte. GP slices now assemble in the existing PCM staging buffer and publish only after all eight 32-sample slices; completed GP and EP frames share publish_pcm_frame and the same bounded native queue. The regression retains a canary host frame through eight real GP executions, then consumes it and verifies exact stereo 8192/-16384 samples in the promoted GP frame. The earlier single-slice assertion now checks staging storage, with full-frame delivery checked separately. Seventeen DSP tests and full isolated Release build pass; native-output checks remain the prior unchanged gate. This corrects GP-only/native retry ownership without claiming full audio acceptance.

GP-only queue ceiling and native allocation failure now have executed coverage. Seventy complete GP frames (560 real 32-sample DSP executions) fill the shared 64-frame queue and account for six excess host frames without changing the retained stereo frame. Forced queue allocation failure accounts for one further drop, preserves pending samples and queue invariants, and leaves genuine GP execution active with no DMA error or fabricated EOL. All seventeen Release DSP checks pass. This verifies bounded output ownership for the GP-only branch; it does not establish arbitrary guest-program or sustained endpoint acceptance.

Guest stack faults no longer assert on the host in dsp_write_reg(SP), dsp_stack_push or dsp_stack_pop. Existing stack-error interrupt routing and state updates are preserved. Executed overflow via MOVEC SSH at SP=15, underflow via RTS at SP=0, and explicit SP error-bit write all reach vector 2 at masked IPL 3; a guest handler repairs SP and executes ADD. The focused `dsp_cpu_test stack-errors` path passes in an assertion-enabled Debug build. Seventeen Release DSP checks and full isolated Release build pass. Extended-stack capacity, exact fault-state hardware conformance and double-fault recovery remain incomplete; no general stack conformance claim is made. Inventory hashes refreshed.

Stack-error rearming now has executed coverage: after each overflow, underflow and explicit SP error-bit fault reaches vector 2 and the guest handler repairs SP, the same running program writes the error bit again without a CPU reset. The second fault reaches the same handler, repairs SP and executes a second ADD with no pending stack interrupt left. The focused assertion-enabled Debug check and all seventeen Release DSP CTests pass. This verifies modeled interrupt rearming, not extended-stack spill/refill, exact fault timing or full arbitrary-program acceptance. DSP56300FM sections 5.4.2-5.4.5 describe additional EP/SC/SZ and software-extension behavior still requiring implementation and verification.

EP (register 0x2A) and SZ (0x38) now retain actual guest MOVEC values rather than applying a zero-width mask. Both support normal 24-bit and compatibility 16-bit source/destination widths; trace names identify them. Hardware CPU reset preserves SZ as specified by DSP56300FM 5.4.3.3. Four executed write/read cases cover both registers and modes plus SZ reset preservation. Seventeen Release DSP CTests pass. Sources: DSP56300FM Rev. 5 Table 12-14, Table 5-1 and sections 5.4.2/5.4.3.3. SC accounting and extended-stack spill/refill remain unimplemented. Existing register slots are used without changing capture layout/version; old captures contain zero in these formerly unwritable slots and do not establish new-register equivalence.

Explicit SP writes now raise stack error only on SE bit 4 changing from zero to one, independent of UF bit 5. Previously UF alone fabricated an exception and an old UF bit suppressed a new SE transition. The write also preserves low pointer bits when raising the real exception. Sixteen executed old/new SE/UF combinations check exact SP storage and interrupt arbitration; the focused regression failed before the fix. Source: DSP56300FM Rev. 5 Table 5-2 (5-19). Extended-mode SP and stack extension remain incomplete.

Latched non-extended stack fault flags are now preserved through implicit pops. Previously popping with SE=1, UF=0 and low SP=0 allowed unsigned subtraction to set UF, changing an overflow into underflow without the explicit SP write required by DSP56300FM Table 5-2. The shared pop path now wraps only the four pointer bits once SE is set. Thirty-two executed MOVEC SSH,X0 cases cover both UF values and every low pointer value, checking exact SP, transferred SSH and no repeated interrupt. The regression failed before the fix; all seventeen Release DSP CTests pass. Initial underflow still generates SE/UF normally. Extended-stack semantics and hardware fault timing remain open.

Explicit SSH/SSL MOVE width now follows DSP56300FM Table 5-1: compatibility mode clears the upper eight bits, while normal mode retains all 24. The shared register mask/source handling covers explicit writes and SSL reads; six explicit SSH-pop MOVE paths mask the returned operand without changing implicit RTS/RTI or loop restoration. Four executed immediate-write/register-read pairs cover both registers and modes, with stack pointer side effects checked. An implicit JSR positive control preserves a full-width stacked SR in compatibility mode. The new explicit-write check failed before the fix. Extended-stack spill/refill, compatibility pipeline delays and full arbitrary-program acceptance remain open.

SSH/SSL explicit-memory conformance regression now executes 32 additional cases: X/Y spaces, short absolute and extension-word absolute MOVEC addressing, loads/stores, both stack halves and normal/compatibility widths. Checks include exact memory/stack payload, SSH push/pop versus SSL unchanged SP, instruction length and no interrupt. Assertion-enabled Debug focused stack checks and all seventeen Release DSP CTests pass. No production change was needed for these memory forms. MOVEM/other operand forms and extended-stack spill/refill still need broader coverage; these checks do not establish arbitrary-program or gameplay audio acceptance.

Hardware CPU reset now preserves EP as well as SZ. DSP56300FM Rev. 5 section 4.3.2 (4-5, PDF 73) explicitly says EP is not initialized by hardware reset; the previous register-array clear violated this. The existing executed EP/SZ write/read regression now checks both registers across reset in normal and compatibility modes. All seventeen Release DSP CTests pass. Extension transfer semantics remain unfinished.

Initial SEN-enabled stack execution implemented: SC register storage, 24-bit logical SP, physical slot-zero use, LOW/HIGH spill at SC>14, reverse refill below SC=2 when logical entries remain, EP updates and WRP on spill. Extended EOV/EUN routes are present but not yet independently verified. Two executed 32-entry guest MOVEC sequences in X/Y check every returned SSH/SSL value, exact eighteen spilled word pairs, logical SP/SC thresholds and EP restoration. This is partial extended-stack implementation: normal-mode SC accounting, enabling on a preexisting hardware stack, explicit SC task switching, double-operation pipeline batching, fault edge cases, compatibility semantics and transfer timing remain gaps. Pinned xemu lacks extension, so the new regression is specification-derived local proof only. Capture layout is unchanged; normal SEN-disabled captures retain previous SC behavior pending an explicit accounting/version decision. Full arbitrary-program/audio acceptance remains open.

Extended fault edge coverage added: eight executed X/Y overflow/underflow cases with each EOV/EUN flag initially clear or already sticky. Checks verify exact OMR flag preservation, full-width logical SP (underflow FFFFFF), masked-IPL interrupt arbitration only for a newly set flag, retained EP/memory canary and actual popped operand. A subsequent guest MOVEC to OMR clears the sticky flag without CPU reset. Assertion-enabled Debug focused checks and all seventeen Release DSP CTests pass. This tests modeled fault transitions, not exact SC underflow hardware state, a second fault after clearing/handler return, independent extension equivalence or sustained gameplay. No production change was required.

Extended-stack control-flow proof added: uploaded MOVEC instructions initialize EP/SZ/OMR, then 32 actual guest JSRs descend through a call chain and 32 actual RTSs return to the caller. Both X/Y extension cases verify every call/return PC and logical SP, exact eighteen spilled return-PC/SR pairs, SC/EP restoration and a subsequent caller ADD. Assertion-enabled Debug focused checks and all seventeen Release DSP CTests pass. This covers implicit single pushes/pops beyond physical stack capacity without synthetic return handling. Loop double operations, mode transitions, timing and independent extension equivalence remain open. No production change was needed.

Nested extended return coverage now includes both RTS and RTI in X/Y memory. Each of the four sequences executes 32 guest JSRs with distinguishable full-width saved status, checks exact spilled SR/PC words, then executes 32 guest return instructions. RTI restores each saved SR, RTS retains current SR; both restore every PC/SP and EP/SC and execute the caller ADD. Debug focused stack checks and all seventeen Release DSP CTests pass. RTI is exercised on call-created stack entries; this is not nested interrupt-dispatch proof. No production change was required. Mode transitions, double stack operations, timing and independent extension equivalence remain open.

Extended physical-index coverage now starts from a populated stack at each of sixteen low SP indices in both X/Y spaces. Thirty-two cases provide coherent initial logical depths 16-31, fourteen hardware entries and older extension words; each executes 32 further guest pushes, verifies all spilled LOW/HIGH pairs, then pops all 48-63 entries with exact operand/SP checks and restores empty SC/EP. This verifies spill/refill across every physical wrap position and preexisting memory entries. Debug focused checks and all seventeen Release DSP CTests pass. Initial context is constructed by the test; guest task-switch restoration, SEN transitions and independent hardware equivalence remain unproven. No production change was required.

Extended loop double-stack regression added: twenty executed nested DO #1 instructions create forty logical entries in both X/Y extension spaces, followed by twenty guest ENDDOs. Each double pop restores exact LA/LC and loop flags; checks include every logical SP, final SC/EP, PC and the first spilled loop-state/PC-status pairs. Debug focused checks and all seventeen Release DSP CTests pass. This proves functional sequential double pushes/pops in the current interpreter; it does not prove pipeline-coalesced two-entry transfers or exact cycle timing, automatic loop termination or independent hardware equivalence. No production change was required.

Extended loop regression also covers automatic termination: twenty nested DO #1 loops have distinct descending endpoints in both X/Y spaces, then a guest JMP reaches the innermost endpoint and twenty ADD instructions unwind the loops naturally. Each loop restores parent LA/LC and non-CCR status/loop flags, every double-pop SP, and final EP/SC; A1=20 proves all endpoint bodies executed. CCR is allowed to change through ADD, unlike the existing ENDDO-only variant. Debug focused checks and all seventeen Release DSP CTests pass. This is functional local execution evidence, not pipeline/hardware conformance or sustained audio acceptance. No production change was required.

Extended SP explicit-source width corrected in shared dsp_move_source: compatibility MOVE reads now clear upper eight bits while retaining the internal logical pointer. A guest MOVEC first writes ABCDEF in normal extended mode; register and X-memory reads in both modes check exact returned width, unchanged SP and no fabricated interrupt from SP counter bits. The compatibility register-read regression failed before the fix. All seventeen Release DSP CTests pass. Compatibility timing and broader extended mode transitions remain open.

Extended SP write regression expanded to 24 executed immediate, X0-register and X-memory MOVEC cases across normal/compatibility modes and zero/full/high-pointer values. It checks stored pointer width, selected physical SSH/SSL entries, instruction length and no SE/UF-derived interrupt when SEN is set. Debug focused checks and all seventeen Release DSP CTests pass. No production change was needed. Normal-mode SC accounting and enabling extension over a preexisting hardware stack remain implementation gaps, rather than accepted limitations of the final arbitrary-program requirement.

Deep extended JSR/RTS/RTI regression now runs in normal and compatibility modes, giving eight X/Y/return-kind/mode combinations. Each executes 32 calls and returns and checks full-width saved SR including the compatibility bit, exact spill words and restored control flow. Debug focused checks and all seventeen Release DSP CTests pass. No production change was required; this validates implicit full-width entries separately from explicit 16-bit MOVE operands. Mode-enable accounting, task-switch behavior, timing and full gameplay acceptance remain open.

Extended push now drains excess hardware entries until SC returns to fourteen, rather than spilling only one. An executed guest SC=15 restoration followed by SSH/SSL push failed the previous SC/EP assertion. X/Y cases now verify two exact oldest LOW/HIGH spill pairs, SC=14, EP advancing by four, and all sixteen values returned with EP/SC restored. All seventeen Release DSP CTests pass. This fixes normalization at a subsequent push; explicit SC-write immediate side effects and pipeline-coalesced transfer timing remain incomplete. Full arbitrary-program/audio acceptance is still open.

Guest SC writes above fourteen now invoke the shared spill helper immediately when SEN is enabled, implementing the documented stack-extension activation for high SC restoration. The SC=15 regression failed before this change; it now checks the oldest pair and EP increment before any subsequent push, then verifies the next push and full restoration. All seventeen Release DSP CTests pass. SC writes below two still lack immediate refill, and normal-mode accounting/transition semantics and transfer timing remain incomplete. Shared helper extraction preserves existing push transfer ordering.

Guest SC writes below two now use the same refill helper as extended pops. It restores up to two available entries, reads HIGH/LOW in reverse spill order, updates EP/SC and refreshes SSH/SSL without setting WRP. The executed X/Y regression covers SC=0/1 and logical depths 0..3, including no refill on an empty stack and a single entry when only one exists; it failed before the fix. This is specification-derived local coverage, not independent hardware conformance. Normal-mode SC accounting, mode transitions, transfer timing, and full gameplay/profiling acceptance remain open.

Normal SEN-disabled pushes/pops now increment/decrement the five-bit SC register, as required by DSP56300FM Rev. 5 section 5.4.3.2. A guest MOVEC regression pushes fifteen distinguishable entries and pops them, checking every SC value and returned word; it failed before this change. Capture version is now 3 (layout unchanged): versions 1/2 must be regenerated because historical initial states lack architectural SC accounting. New capture/replay tests pass; prior gameplay version-2 results remain historical evidence and cannot be replayed as version 3. Extension mode transitions, timing, independent extension verification, and full gameplay/profiling acceptance remain open.

DMA control audit: pinned xemu also asserts on ABORT and supplies no implemented reference transition; ABORT remains explicitly unsupported rather than invented. Descriptor control bits 2/3/13 previously relied on host assertions (the new bit-2 regression aborted before the fix), and could be ignored with NDEBUG. They now set error/STOPPED before any payload, offset writeback, or EOL publication. The regression covers each bit with valid 16-bit packing and checks unchanged payload/writeback and no completion. All seventeen Release tests and full isolated Release build pass. This closes a host-abort/silent-bypass gap; it does not implement these flags or establish arbitrary-program DMA support.

Lower peripheral MOVEP register form now decodes Y:qq through the existing register helper, selecting X/Y from the encoding rather than forcing X. Both lower X/Y forms now format all 64 addresses and directions correctly; the new formatter regression failed before the fix. Y execution checks verify PC advancement, no illegal interrupt, retained source register on write and existing unmapped-memory value on read. This implements the instruction route only; no Y peripheral device mapping is fabricated or claimed. Three NULL execution rows remain (debug/debugcc and Y:qq memory MOVEP). All seventeen Release tests and full isolated Release build pass.

Y-memory MOVEP now shares the lower-peripheral helper with X, selecting peripheral space from bit 14. All 64 addresses, both EA spaces and directions are checked with indirect R0, unchanged R0, exact destination/source checks, PC and interrupt state. Register MOVEP execution tests were corrected to upload via dsp56k_write_memory, invalidating opcode cache; prior direct-pram execution evidence was unreliable and is superseded. Y addresses beyond modeled RAM now return 0xffffff/drop writes rather than asserting/indexing past RAM; this is an explicit unverified unmapped-memory fallback, not Y peripheral hardware support. Only DEBUG/DEBUGcc NULL execution rows remain. Full arbitrary-program/peripheral and audio acceptance remain open.

DEBUG/DEBUGcc now enter a distinct idle debug state rather than the illegal-instruction route, following DSP56300FM Rev. 5 pages 13-49/50. False DEBUGcc proceeds normally; the local regression checks all 4096 condition/CCR combinations using the existing condition evaluator, unchanged SR, PC, no pending illegal IRQ, halted execution, unconditional DEBUG and reset. The new state uses padding beside exception_debugging, preserving layout and version-3 non-debug capture interpretation. Frame start and normal interrupt dispatch do not release debug state. OnCE command handling/resume, external debug requests and debug-mode DMA timing are not modeled; the wrapper currently suspends DMA while debugging. No NULL execution decoder rows remain, but that does not prove complete instruction/peripheral semantics or arbitrary-program support.

Failed bootstrap transfers now latch bootstrap_failed in existing state padding, stop scheduling partial/stale program RAM, and require reset. Both GP/EP regressions simulate a partial scratch copy plus error, check three subsequent frame starts produce no instructions, ADD or EOL, then reset and execute a fresh uploaded ADD. This is an explicit host transfer-failure policy, not a verified hardware fault-latch model. It closes execution-after-failed-upload behavior without adding acknowledgements; successful bootstrap behavior is unchanged. DMA configuration semantics remain unimplemented in pinned xemu and are not inferred from bit names.

Sixteen-bit arithmetic rounding: DSP56300FM Rev. 5 sections 3.4.2 and 12.2 specify virtually concatenated 16-bit portions (40-bit accumulator), held in bits 23..8 of A1/A0 or B1/B0. Shared rounding now packs those portions, applies existing scaling/tie rules at the 16-bit boundary, and unpacks with cleared low bytes. Expanded executed RND tests cover both arithmetic widths, both accumulators, three scaling modes, two tie modes and boundary fractions, with independently computed quotient/remainder results. SA cases failed the old middle-word assertion. All seventeen Release tests pass. This is partial SA implementation: other arithmetic/moves/flags and round-enabled multiply input generation still need conformance work; no full SA or independent hardware claim.

SA word-register bus moves: shared dsp_move_source/dsp_write_reg now shift between bus bits 15..0 and register bits 23..8 for X0/X1/Y0/Y1/A0/A1/B0/B1, following DSP56300FM Rev. 5 tables 3-3/3-4. Sixty-four executed MOVEP read/write pairs cover eight registers, four distinguishable values and both arithmetic widths, checking internal placement and outgoing bus value; the old helper failed SA cases. All seventeen Release tests pass. Full accumulators, extension sign reads, immediate-short conventions, parallel moves and helper-bypassing paths remain incomplete; this is not full SA acceptance.

SA full-accumulator writes now align the sixteen bus LSBs into A1/B1 bits 23..8, clear A0/B0 and sign-extend bit 15 through the existing accumulator helper. Twenty executed MOVEP loads cover both widths, A/B, signed endpoints and bus values with nonzero upper bytes; SA failed before the fix. Duplicated MOVEC-register and parallel-memory register write blocks now route through dsp_write_reg. Existing SR-to-X0 test deliberately has SA set in 0x230134; its X0 expectation now includes the required word alignment while program-memory SR output retains the bus expectation. Full accumulator reads/limiting, immediate-short conventions and remaining parallel paths are still incomplete.

SA accumulator word reads now virtually concatenate the forty meaningful bits, apply scaling before signed-word extraction, limit to -32768..32767 and sign-extend the result on the 24-bit bus (table 3-4); limiting sets sticky L. Sixty executed MOVEP outputs cover A/B, three scaling modes and positive/negative boundary/extension values, with numerical reference expectations; old behavior failed. Shared emu_pm_read_accu24 serves existing accumulator-word callers. All seventeen Release tests pass. Dual-bus accumulator moves, other arithmetic and mode-transition hazards remain incomplete; specification-derived local coverage does not establish independent hardware SA equivalence.

SA immediate-short partial-accumulator alignment now shifts A0/A1/B0/B1 destinations into bits 15..8, per DSP56300FM Rev. 5 section 3.4.1.3. Existing signed-fraction X/Y/full-accumulator and extension-register alignment stays on its distinct path. Executed MOVE+NOP coverage includes all 256 byte values, twelve ALU destinations and both widths (6144 cases), checking value/sign extension, PC and unchanged SR; SA partial cases failed before the fix. All seventeen Release tests pass. Other parallel arithmetic/move interactions, dual-bus transfers and mode hazards remain incomplete.

SA extension-register bus reads: dsp_move_source now sign-extends A2/B2 bit 7 across the upper sixteen data-bus bits in SA mode, per DSP56300 Family Manual Rev. 5 section 3.4.1.2 and table 3-4 (pages 3-16/3-17). Added 1,024 executed MOVEP cases covering both accumulators, all byte values, and both arithmetic modes; baseline failed before the correction. All seventeen Release tests and the isolated full Release build pass. This does not establish remaining SA arithmetic, dual-bus moves, or sustained audio acceptance.

SA partial long moves: emu_pm_4x now routes A10/B10/X/Y bus reads and writes through the existing word-move helpers, preserving accumulator extensions. Added 32 load/store pairs (64 executed instructions) across four register pairs, four input pairs, and both arithmetic modes; baseline failed, corrected implementation passes. Manual Rev. 5 section 3.4.1.1 and table 3-3 specify the input placement; partial-accumulator output placement is in table 3-4. The X/Y low-word output prose in table 3-4 says sixteen LSBs while input placement uses sixteen MSBs; implementation uses the significant sixteen bits consistently with the partial-word moves, so independent hardware confirmation of that wording remains open. Full-accumulator long scaling/limiting and general SA arithmetic remain incomplete. All seventeen Release tests and isolated full Release build pass.

SA full-accumulator long moves: A/B and AB/BA loads now reuse dsp_write_reg for correct sixteen-bit placement, sign extension, and low-word clearing. A/B long stores now scale the concatenated forty-bit value and clamp the resulting thirty-two-bit double word, with sign-extended X bus and zero-extended Y bus per manual Rev. 5 sections 3.4.1.1-3.4.1.4. Added 32 executed loads across both modes and all four destinations, plus 60 SA long stores covering both accumulators, three scaling modes, and ten signed boundary inputs. Load baseline failed before correction. All seventeen Release tests and isolated full Release build pass. Independent hardware/reference validation and broader SA arithmetic remain open.

SA long-store regression sensitivity verified independently of the load fix: copied the current interpreter and complete private execution test into conformance_tmp/sa-long-store-mutant, removed only the SA A/B long-store override, and built the isolated mutant. Mutant exits 1; current executable exits 0. Hashes and outputs are recorded in that directory/results.json. Shared source was not temporarily reverted. This demonstrates the new long-store cases reject the previous store behavior; it is not an independent hardware oracle.

SA parallel register moves: emu_pm_2_2 destination handling now uses existing dsp_write_reg rather than a duplicate raw assignment, so bus placement for partial/full accumulators and word registers agrees with the other MOVE paths. Added 96 executed R0-to-ALU-register moves spanning twelve destinations, four source values, and both arithmetic modes; baseline failed before correction. All seventeen Release tests and isolated full Release build pass. Remaining SA arithmetic and representative gameplay audio acceptance are not established by this gate.

Post-SA-move gameplay replay refresh: current interpreter exactly reproduces the selected GP frame (55,648 cycles, 107 transfers) and EP frame (15,856 cycles, 8 transfers) from dsp_v3_gameplay_20261008_190547, including final state and outgoing payloads. Pinned xemu CPU exactly reproduces EP; GP retains the same eight raw final-state byte differences in modifier/VBA/stack state, with all 107 transfers matching. Executable/capture hashes and raw diagnostics are in conformance_tmp/dsp_v3_gameplay_20261008_190547/post-sa-moves-replay.json. This is a selected-frame regression refresh, not new sustained gameplay, listening acceptance, or independent full-APU validation.

Post-SA native-output gate: rebuilt and passed both xaudio2_test and monitor_test from current sources. Inspected coverage: production output queue validates submissions and preserves queued data through failures/full-ring/wrap; extracted production monitor preserves PCM through output failures/cancellation, unlocks during backpressure, and mixes retained frames once. Platform calls are mocked, so this does not prove physical device playback or listening quality. Raw results and executable hashes: conformance_tmp/post-sa-native-output/results.json.

SA condition-code operand width: shared emu_ccr_update_e_u_n_z masks the ignored low bytes of MSP/LSP before flag evaluation, per manual Rev. 5 section 3.4.2. Added 1,024 executed TST cases across both accumulators, both modes, and all low-byte pairs summing to 255, verifying SA zero detection while preserving the underlying accumulator. Baseline failed; corrected implementation passes all seventeen Release tests and isolated full Release build. General forty-bit add/subtract, shifts, and other SA arithmetic remain incomplete.

SA ADC/SBC carry position: all eight X/Y-to-A/B handlers now add/subtract the incoming carry at physical LSP bit 8 in SA mode (bit 0 in normal mode), per manual Rev. 5 section 3.4.2. Added 32 executed cases across eight forms, both carry states, and both modes; baseline failed, corrected implementation passes all seventeen Release tests and isolated full Release build. This fixes carry position only: continuous forty-bit carry propagation across MSP/LSP, ignored operand bytes, and arithmetic-result flags still require further work and boundary tests.

SA ADC/SBC continuous arithmetic: shared emu_carry_arithmetic now packs significant operands into forty bits, performs add/subtract, and unpacks them with ignored bytes cleared. Normal mode delegates to unchanged add/subtract helpers. All eight ADC/SBC forms use it; added 48 executed incoming-carry boundary cases (zero, LSP/MSP boundaries, signed extremes, full wrap). Baseline failed; all seventeen Release tests pass. Source-buffer arithmetic, exact combined flag behavior across two-stage carry operations, and other SA arithmetic still need broader coverage; this is not full arithmetic conformance.

SA ADC/SBC combined flags: incoming carry is now included in the single forty-bit operation, preventing intermediate overflow flags from leaking into a final result that does not overflow. Added 480 executed cases across eight forms, six accumulator boundaries, five signed source values, and both incoming carry states; ignored source/destination bytes are seeded nonzero. Checks significant result, carry/borrow, and final overflow against signed/unsigned arithmetic expectations. Baseline failed; corrected implementation passes all seventeen Release tests and full isolated Release build. Normal-mode two-stage behavior is retained. Other SA arithmetic and independent hardware confirmation remain open.

SA accumulator ADD/SUB: four A/B accumulator-to-accumulator forms now reuse the existing forty-bit arithmetic helper (renamed emu_add_sub_arithmetic), with unchanged normal-mode delegation. Added 144 executed boundary pairs, seeding ignored bytes and checking result plus carry/overflow; baseline failed, current code passes all seventeen Release tests and isolated full Release build. Other ADD/SUB operand forms and the rest of SA arithmetic remain open.

SA X/Y double-word ADD/SUB: eight ordinary X/Y-to-A/B forms now reuse emu_add_sub_arithmetic with zero incoming carry. Extended existing source-buffer test loop to ordinary forms (480 additional executed cases), including ignored bytes, signed boundaries, carry/borrow and overflow. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Word-source, immediate, shifted arithmetic, and remaining SA instructions still require work.

SA word-source ADD/SUB: sixteen X0/X1/Y0/Y1-to-A/B forms now reuse the forty-bit arithmetic helper. Added 480 executed cases across all forms, six accumulator boundaries, and five signed word values, with nonzero ignored bytes and result/carry/overflow checks. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Immediate/shifted arithmetic and other SA instructions still remain incomplete.

Post-SA-arithmetic gameplay replay: after ADC/SBC combined-carry and accumulator/double-word/word ADD/SUB changes, current replay still exactly matches selected gameplay GP (55,648 cycles, 107 transfers) and EP (15,856 cycles, 8 transfers). Pinned xemu EP is exact; GP retains eight raw final-state differences with all transfers matching. Fresh executable hashes and diagnostics: conformance_tmp/dsp_v3_gameplay_20261008_190547/post-sa-arithmetic-replay.json. This does not establish fresh sustained gameplay, listening quality, or remaining SA instructions.

SA accumulator CMP: both A/B accumulator comparison forms now use shared forty-bit subtraction for flags without modifying accumulator storage. Added 72 executed boundary pairs with unequal ignored bytes, checking carry/overflow/zero/negative and exact preservation of both accumulators. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Other comparison forms, other SA arithmetic, and sustained gameplay audio acceptance remain open.

SA word-source CMP: eight X0/X1/Y0/Y1-to-A/B comparison forms now use shared forty-bit subtraction. Extended the word arithmetic loop with 240 executed comparison cases covering signed sources and accumulator boundaries, checking carry/overflow and exact accumulator preservation. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Magnitude/immediate comparisons, other SA arithmetic, and sustained gameplay audio acceptance remain open.

SA NOT width: both accumulator NOT forms now clear ignored low bits in the MSP result rather than complementing them, following manual Rev. 5 section 3.4.2. Added 24 executed cases across both modes, both accumulators, and six boundary/ignored-byte values, checking exact SR and untouched LSP/EXT. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Other logical operations and broader SA conformance remain open.

SA word logical result width: all twenty-four word-source AND/OR/EOR forms clear the ignored low byte before flag evaluation, per manual Rev. 5 section 3.4.2. Added 1,728 executed cases across both modes, four sources, two accumulators, three operations, and six-by-six input pairs, checking exact SR and untouched LSP/EXT. Baseline failed; corrected implementation passes all seventeen Release tests and isolated full Release build. Immediate logic, shifts, multiplication, other SA instructions, and sustained gameplay acceptance remain open.

SA single-bit LSL/LSR: four accumulator logical-shift forms now operate on significant sixteen bits; LSR takes carry from physical bit 8 and clears the unused low byte, while LSL excludes ignored input bits before shifting. Added 64 executed cases across both modes, both accumulators, both directions, and eight boundary inputs, checking exact SR and untouched LSP/EXT. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Counted shifts, rotations, arithmetic shifts, multiplication, and broader acceptance remain open.

ROL/ROR carry semantics and SA width corrected: all four forms insert previous SR.C instead of recirculating outgoing operand bit, following manual Rev. 5 ROL/ROR pages 13-165/13-166. SA operates on sixteen significant bits per section 3.4.2; normal mode remains twenty-four bits. Added 128 executed cases across both incoming carries, both modes, accumulators/directions and eight boundary inputs, checking exact SR and untouched LSP/EXT. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. This intentionally corrects inherited normal-mode rotation behavior too. Full gameplay/reference acceptance remains unproven.

SA counted logical shift width: emu_logic_shift now shifts significant sixteen bits in SA and twenty-four in normal mode, with width-dependent carry and cleared ignored result byte. Added 2,048 executed immediate-count cases (all counts 0-31, both modes, accumulators/directions, eight input boundaries), checking exact SR and untouched LSP/EXT. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Register-count source interpretation in SA, arithmetic shifts, multiplication, and broader gameplay/reference acceptance remain open.

Register-count logical shift verification: manual Rev. 5 pages 13-93 through 13-96 define control-register low-five-bit SH field in normal mode, zero-count carry clearing, and allowed shift width. Added 4,800 executed normal-mode cases spanning all six sources, both accumulators/directions, counts 0-24, and eight inputs; A1/B1 source aliases are included. Existing implementation passes; no runtime change. SA count-field position is not explicit in these pages and remains unresolved rather than silently inferred. All seventeen Release tests pass.

SA single-bit arithmetic shifts: four ASL/ASR forms now pack and shift the continuous forty-bit value, restoring significant words with ignored bytes cleared. Normal mode delegates to unchanged dsp_asl56/dsp_asr56. Added 24 executed boundary cases with seeded ignored bytes, checking result/carry/overflow; baseline failed, corrected code passes all seventeen Release tests and isolated full Release build. Counted/shifted arithmetic, multiplication, independent conformance and gameplay audio acceptance remain open.

SA counted arithmetic shifts: generalized existing single-bit helper to a count and routed ASL/ASR immediate/register forms through it. SA shifts continuous forty-bit values, accumulates overflow across shifted bits, preserves final carry, and clears ignored bytes; normal mode delegates to existing full-width helpers. Added 1,920 executed immediate cases across both source/destination accumulators, directions, counts 0-39, and six boundary inputs. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Register-count placement, out-of-range count semantics, multiplication, and broader acceptance remain open.

SA parallel MPY: added shared emu_multiply signed sixteen-bit fractional multiplication with forty-bit placement and unchanged normal-mode delegation. All thirty-two unrounded parallel MPY forms now use it. Added 800 executed cases across eight operand pairs, both destinations/sign selections and five-by-five signed boundary inputs, seeding ignored bytes; baseline failed. All seventeen Release tests and isolated full Release build pass. Rounded/accumulated/immediate multiplication forms, full flag coverage, and broader acceptance remain open.

SA parallel MPYR: all thirty-two rounded parallel multiply forms now use emu_multiply before existing width-aware dsp_rnd56. Extended multiply execution loop with 800 rounded cases and independent floor/remainder nearest-even expectations for unscaled default rounding. Baseline failed; corrected code passes all seventeen Release tests and isolated full Release build. Other scaling/rounding modes, tie-specific multiplication inputs, accumulated/immediate forms and full flag coverage remain open.

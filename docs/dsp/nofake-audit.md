# DSP completion and substitution audit - 2026-10-08

This audit proves specific active completion invariants, not full MCPX conformance.

- Source search with fff.multi_grep over C/C++ headers/sources for `RECOMP_APU_DSP_ACK`, `RECOMP_DSP_FAKE`, `dma_read_count`, and `reverb` found only the environmental-reverb API description in `src/audio/dsound_xbox.h`. Search absence alone is not proof against every possible substitution.
- `dsp_dma_read` returns stored control state. It performs no transfer and does not publish completion. `dsp_dma_step` executes descriptor payload callbacks and only publishes EOL after successful transfer/writeback. Explicit unsupported-format/error handling never completes the failed transfer.
- `dsp_run` executes guest instructions and advances DMA at instruction boundaries. `dsp_start_frame` signals the actual frame interrupt. Peripheral interrupt writes clear the stored bits/EOL; they do not clear guest commands through a host substitute.
- `apu_dsp.c` bootstraps guest P memory from guest scratch RAM and provides scatter/gather/FIFO callbacks. Native PCM staging is downstream of successful guest FIFO writes. Host overflow/OOM is counted independently from guest DMA completion.
- `dsp_apu_test.c` uploads executable instructions and descriptors. It checks RUNNING remains observable through repeated peripheral reads before the next execution boundary, checks the command word remains unchanged before DMA, and checks actual cross-page RAM writes before completion. Invalid SGE cannot be acknowledged.

## Mutation proof

An isolated build in `I:/repos/mm3-dsp-nofake-mutant-20261008` replaces only DMA_CONTROL read semantics with deliberately fake STOPPED/EOL publication. The production APU regression rejects that build with exit 1 at its RUNNING assertion (line 142 at this revision). Production files were hash-checked unchanged. `result.json` in that build directory records the source hash, mutation, exit and failure output. Thus this regression demonstrably detects the old class of read-triggered completion, rather than merely passing the current implementation.

## Limits

Unmapped peripheral reads still return upstream's `0xababa`; the counter peripheral remains upstream's zero model. These are incomplete hardware models, not verified arbitrary-program semantics. Eleven NULL decoder rows, partial instruction modes, missing DMA formats and approximate scheduling remain open. GP monitor addresses reflect the modeled mixbuffer layout and do not establish environmental-effect correctness. Frame reference replay shares the adapted DMA/peripheral wrapper. The visually verified 180-second Standard Delivery capture proves one current scene, not all music/voices/reverb/transitions or audible quality. No complete claim is made for the broader objective.

## Additional mutation proof

Two further isolated builds compile the current production APU regression with deliberately faulty runtime copies:

| Mutation | Rejected invariant | Evidence under the isolated build root |
|---|---|---|
| Host clears only the command word, skips the descriptor payload and publishes EOL | Cross-page payload word at guest RAM 0x7000 must equal 0x654321 (exit 1) | `ack_only/result.json` |
| Runtime increments PC/instruction metadata without executing guest DSP instructions | Uploaded START must actually set DMA_CONTROL_RUNNING (exit 1) | `cpu_bypass/result.json` |

Both files are under `I:/repos/mm3-dsp-nofake-mutant-20261008`. Original production source bytes were hash-checked unchanged. The unmutated `dsp_apu` CTest passes as a positive control. These three rejected mutations demonstrate coverage for poll-triggered completion, host command-only substitution, and instruction-execution bypass. They do not exhaust every possible bypass or verify all instruction/peripheral semantics.

DMA control audit: pinned xemu also asserts on ABORT and supplies no implemented reference transition; ABORT remains explicitly unsupported rather than invented. Descriptor control bits 2/3/13 previously relied on host assertions (the new bit-2 regression aborted before the fix), and could be ignored with NDEBUG. They now set error/STOPPED before any payload, offset writeback, or EOL publication. The regression covers each bit with valid 16-bit packing and checks unchanged payload/writeback and no completion. All seventeen Release tests and full isolated Release build pass. This closes a host-abort/silent-bypass gap; it does not implement these flags or establish arbitrary-program DMA support.

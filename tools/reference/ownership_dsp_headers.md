# DSP header ownership review

Reviewed complete sources at fork bd3708177f7a4786766a675310a8f672ecdcd8f3. All seven byte hashes match
the current source inventory. No source or license text changed.

| Header | Generic responsibility |
| --- | --- |
| dsp.h | DSP core/DMA state composition and reset/bootstrap/frame/memory API |
| dsp_port.h | Standalone endian helpers, common C vocabulary, assertion/trace policy |
| debug.h | Conditional diagnostic printing with upstream license retained |
| dsp_dma_regs.h | DMA action/configuration/node bit definitions |
| dsp_dma.h | DMA state, memory/FIFO/scratch callbacks and register API |
| interp/dsp_cpu_regs.h | DSP register indexes, memory spaces, interrupt/state constants |
| interp/dsp_cpu.h | Interpreter/disassembly state, peripheral callbacks and execution API |

These definitions/mechanisms support the generic DSP implementation. Numerical
register maps are hardware vocabulary, not captured title programs. No title
addresses, uploaded program bytes, game-specific tables or application
callbacks were found. Upstream rights/provenance notices remain unchanged.

Limits: this is ownership source review, not hardware conformance. No new build,
DSP execution or audible-game test was performed. Portability helpers force
assertions in Release and disable upstream trace macros; this is explicit host
policy, not proof that unsupported execution is recoverable. State padding,
capture version 3, recompiled/interpreter transitions and DMA behavior still
need implementation evidence. No new ABI/capture compatibility claim.

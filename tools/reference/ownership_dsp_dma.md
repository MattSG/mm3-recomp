# DSP DMA source ownership review

Reviewed complete dsp_dma.c at fork e7252320435a2da4d1c7cb51b94c4a7966382738. The implementation uses
caller callbacks for DSP memory, host scratch and FIFO transfers. Descriptor
fields select generic memory spaces, sample conversion/interleave and routing.
No title addresses, uploaded program, captured descriptor chain or application
callback implementation is embedded here. Rights/provenance notices remain.

The sole source change replaces a captured profiling percentage with a generic
staging-buffer explanation. Original wording is in
legacy_dsp_dma_profile_notes.md. The 4096-byte local buffer and heap fallback,
parsing and transfer behavior are unchanged.

Existing dsp_cpu CTest passes on Windows and Linux. Its DMA checks exercise
memory edges, supported transfer formats and unsupported control/error paths.
This is a bounded regression, not independent full-DMA/audio fidelity proof.

Limits observed from source: scheduling transfers one descriptor per step,
unmodeled bit 9/step fields, 24-bit packing variants, native integer access
endian/alignment assumptions, and callback fault/progress handling require
further evidence. Formats/routes explicitly rejected by the code should not be
reported as supported. Read polling does not itself advance transfer completion.
No fresh title capture, audible gameplay or upstream-equivalence claim.

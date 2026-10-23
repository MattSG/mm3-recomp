# MCPX APU register vocabulary ownership

Full src/apu/apu_regs.h source read at fork 9dc9f370bc0f23b605f1a3783e3cd91326abb24e. The header defines
MCPX MMIO/front-end method offsets, masks, voice structure fields, FIFO counts,
DSP memory regions, notifier statuses, HRTF handles and sample/mixbin capacities.
These describe the device interface; no game identifiers, guest code addresses,
captured audio payloads, title presets or runtime policy callbacks are present.
Existing copyright and LGPL notices remain intact.

Caller inspection confirms voice structure offsets and ADPCM block sizes feed
apu_vp.c, while EP_FRAME_US feeds the apu_core.c frame scheduler. The frame
interval and monitor-byte-rate definitions describe fixed audio processing
rates, not a title-specific frame-rate target. No constants changed and no new
runtime tests were needed for this source-only ownership decision.

Retain the header in the generic runtime. Its existing FIFO-mask FIXME and
ADPCM sample-count/interpolation FIXME are accuracy questions, not evidence of
application ownership. Their numeric correctness, scheduler precision, signed
mask expressions and full consumer behavior are not certified by this review.
The NV2A register header was only sampled and remains outside this full review.

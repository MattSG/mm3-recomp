# APU state header ownership and HRTF correction

Reviewed complete apu_state.h at fork d7582c4b740b7cbfc879e2d4d72221db84335419. The header contains generic
APU/voice/DSP state, SVF/HRTF processing, ADPCM step/index tables and submodule
interfaces. No title-specific binary payload or fixed title address table was
found. Original rights notices remain unchanged.

Captured Jet Set Radio Future padding/block statistics and DSP queue performance
narrative moved to legacy_apu_state_observations.md. ADPCM behavior and queue
layout/locking behavior are unchanged by moving those comments.

HRTF target normalization previously used break when a channel sum was zero or
one. That stopped the stereo loop and left the right target stale. It now skips
normalization for that channel with continue, then updates the other channel.
The actual-header regression failed before the fix and passes for left sums
zero/one/two. Existing exact filter processing state/PCM comparison also passes.
The generic HRTF test is now available on Linux, with libm linked there.
Windows/Linux HRTF CTests and both game builds pass.

Limits: no audible title run or complete APU/codec/register conformance.
ADPCM assumes its callers provide supported channel counts and output capacity;
nonfinite filter coefficients and state lifecycle/concurrency need wider proof.
This is generic ownership and bounded algorithm regression evidence.

# Historical audio support comments

Comments below preceded this ownership/source review; they describe old
bring-up intent and do not prove output or guest voice/DSP fidelity.

```c
 * The VP uses libsamplerate for pitch-shifted voice resampling.
 * We stub it initially; voices will play at native rate.
```

```c
 * The monitor output uses SDL. We stub it; actual audio output
 * will be connected via WASAPI separately.
```

```c
/* Play a 440Hz test tone through the APU pipeline to verify audio output.
 * Directly programs a voice without going through DirectSound. */
```

The compatibility resampler callbacks return zero frames. SDL shim entrypoints
do not open a stream. Native output implementations remain in apu_xaudio2.c.
The test tone writes monitor output rather than programming a hardware voice.
The APU shim's Burnout project copyright is retained as source attribution;
a title name in a rights notice is not executable application policy.

## Generic trace correction

RECOMP_APU_TRACE formerly called mcpx_apu_mmio_read after every attempted
decode, even writes/rejections. The actual-hook synthetic regression fails
with trace enabled before the fix and passes with/without tracing afterward.
The decoder now passes its serviced value to logging without extra device IO.
Windows and Linux CTest projects each pass 2/2. This is callback-count/control
flow evidence, not hardware APU timing or complete x86 instruction coverage.

Tests are independently runnable with cmake -S tools/xboxrecomp/tests/apu_mmio
-B build/apu-mmio, cmake --build build/apu-mmio (select Release on Windows),
and ctest --test-dir build/apu-mmio (Windows: -C Release). CTest creates
separate traced/plain processes so the cached trace option cannot mask cases.
Five synthetic instruction cases cover load, register/immediate stores, OR
read-modify-write and a rejected opcode, checking callback counts, RIP and
selected values. Logs: conformance_tmp/ownership-apu-mmio-*.log. Both final
game builds pass; no new driving or real audio-output proof is claimed.

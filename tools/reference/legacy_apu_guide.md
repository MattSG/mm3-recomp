# xbox_apu — MCPX APU Audio Emulation

Hardware emulation of the Xbox's MCPX APU (Audio Processing Unit), extracted from [xemu](https://github.com/xemu-project/xemu). The APU is a custom chip with three processors:

- **VP (Voice Processor)** — 256 hardware voices, ADPCM/PCM decode, pitch shifting, envelopes, HRTF 3D audio
- **GP (Global Processor)** — DSP for global effects (reverb, chorus). Executes guest-uploaded programs through the adapted xemu interpreter.
- **EP (Encode Processor)** — DSP for AC3/DTS encoding. Executes guest-uploaded programs through the adapted xemu interpreter; AC3/DTS conformance remains unproven.

Native output uses XAudio2 with Windows `waveOut` fallback at 48kHz stereo 16-bit. See [provenance/licensing](dsp/PROVENANCE.md) for where the DSP code comes from and how it was adapted.

## Files

| File | Purpose |
|------|---------|
| `apu.h` | **Public API** — init, shutdown, MMIO, mixer voices |
| `apu_state.h` | APU state struct (VP, GP, EP, DSP core, 256 voices) |
| `apu_regs.h` | MCPX APU register definitions and field masks |
| `apu_core.c` | MMIO handlers, frame thread, waveOut output, test tone |
| `apu_vp.c` | Voice Processor — decode, resample, mix, envelopes, HRTF |
| `apu_dsp.c` | GP/EP RAM, scatter/gather and FIFO DMA integration |
| `apu_mmio_hook.c` | VEH x86-64 instruction decoder for 0xFE800000+ MMIO |
| `apu_shim.h` | QEMU compatibility (threads, atomics, clocks → Win32) |
| `apu_debug.h` | Debug logging macros |
| `fpconv.h` | IEEE 754 float/int conversion utilities |

## Quick Start

```c
#include "apu.h"

// Initialize APU with pointer to Xbox RAM
// (ram_ptr = native pointer to Xbox VA 0x00000000)
MCPXAPUState *apu = mcpx_apu_init_standalone(ram_ptr);

// Test: play a 440Hz sine tone
mcpx_apu_play_test_tone(apu);

// --- OR use the software mixer for direct PCM playback ---

// Allocate a mixer voice
int voice = apu_mixer_alloc_voice();
APUMixerVoice *v = apu_mixer_get_voice(voice);

// Configure it
v->pcm_data = my_pcm_buffer;     // 16-bit signed PCM
v->pcm_bytes = buffer_size;
v->num_channels = 2;              // 1=mono, 2=stereo
v->sample_rate = 44100;           // Source sample rate
v->volume = 0.8f;                 // 0.0 - 1.0

// Play
apu_mixer_play(voice, 1);  // 1 = looping

// Stop and free
apu_mixer_stop(voice);
apu_mixer_free_voice(voice);

// Shutdown
mcpx_apu_shutdown(apu);
```

## Architecture

Recompiled guest audio calls reach APU registers through the compatibility/MMIO layer. The native VP supplies mixbins; GP/EP execute uploaded DSP instructions and scratch/FIFO DMA. Completed output feeds the native monitor/mixer and XAudio2 or waveOut. Scheduling uses instruction batches; full MCPX timing conformance remains unproven.

## Two Audio Paths

### 1. Hardware APU Emulation (MMIO)

For games that program the APU directly. The game writes to MMIO registers at 0xFE800000+, which are intercepted by the VEH hook and routed to the APU emulation:

```c
// Game writes: voice 5, set buffer address
*(uint32_t*)(0xFE802000 + 5 * voice_stride + VOICE_BUF_OFFSET) = buffer_addr;

// VEH intercepts → mcpx_apu_mmio_write() → VP processes voice
```

The VP runs in a separate thread, mixing active voices at the hardware frame rate.

### 2. Software Mixer (Direct API)

For games where you've decoded the audio format yourself and just want to play PCM. The mixer provides 64 voice slots with:

- 16-bit signed PCM input (mono or stereo)
- 16.16 fixed-point resampling (any rate → 48kHz)
- Per-voice volume control
- Looping support

```c
int apu_mixer_alloc_voice(void);                    // Returns slot 0-63, or -1
void apu_mixer_free_voice(int slot);
APUMixerVoice *apu_mixer_get_voice(int slot);       // Configure voice params
void apu_mixer_play(int slot, int looping);          // Start playback
void apu_mixer_stop(int slot);                       // Stop playback
```

## MMIO Hook

The APU MMIO hook (`apu_mmio_hook.c`) uses Windows Vectored Exception Handling to intercept memory accesses to the APU register range:

```
Access to 0xFE800000+
  → Page fault (no physical memory there)
  → VEH handler catches EXCEPTION_ACCESS_VIOLATION
  → Decodes the faulting x86-64 instruction (MOV, ADD, XOR, etc.)
  → Routes to mcpx_apu_mmio_read() or mcpx_apu_mmio_write()
  → Advances RIP past the instruction
  → Resumes execution
```

This allows recompiled code to access APU registers using normal memory operations, exactly as the original Xbox code did.

## Voice Processor Details

The VP processes 256 hardware voices per frame:

- **ADPCM decode**: Xbox ADPCM (4-bit, 64-sample blocks)
- **PCM formats**: 8-bit unsigned, 16-bit signed, 24-bit
- **Pitch**: Log2 fixed-point (4.12 format). Pitch 0 = 48kHz native rate.
- **Envelopes**: Multi-segment amplitude envelopes (attack, decay, sustain, release)
- **HRTF**: Head-Related Transfer Function for 3D positional audio
- **Mixbins**: 32 output channels, voices route to any combination
- **Filters**: Per-voice low-pass/high-pass (IIR biquad)

## Register Map (Key Registers)

```
Offset      Name                   Description
──────────────────────────────────────────────────
0x1000      NV_PAPU_ISTS           Interrupt status
0x1100      NV_PAPU_FECTL          Frontend control
0x2000      NV_PAPU_SECTL          Synth engine control
0x202C      NV_PAPU_VPVADDR        VP voice address base
0x3000+     NV_PAPU_GP*            Global Processor registers
0x4000+     NV_PAPU_EP*            Encode Processor registers
```

Full register definitions in `apu_regs.h`.

## Dependencies

- `qemu_shim.h` (from `../nv2a/`) — QEMU type abstraction layer
- `kernel32.lib` — Win32 threading
- `winmm.lib` — waveOut audio output


## Historical build observation

```cmake
        # RelWithDebInfo's default /Ob1 keeps the DSP56300 interpreter's small
        # helpers out of line: a recorded GP frame took 400 us at /Ob1 and
        # 330 us at /Ob2, of a 667 us real-time budget.
```

The shared guide above is a historical snapshot, including obsolete or
overstated instruction/output descriptions. Current generic documentation
points to implemented interfaces and explicitly bounded test contracts. The
recorded local timings are not current benchmark or whole-game evidence.

## Output source review and validation

Both XAudio2/SDL output branches were read as generic host queue, gain, pause
and source-PCM capture mechanisms. No title asset/address selector was found
in this source scope. Endpoint lifecycle, COM/thread ownership and hardware
timing remain separate review questions.

The SDL backend multiplied num_samples before validating it. Extending the
existing invalid-input cases with INT_MIN/INT_MAX reproduced signed overflow
in the actual backend under UBSan. Byte sizing now follows range and active
stream checks. Windows queue/monitor tests pass 2/2; the full SDL dummy queue
test passes 1/1 with UBSAN_OPTIONS=halt_on_error=1. It also checks padding,
source PCM capture, mute/pause and reinitialization. No real device or driving
acceptance is claimed. Logs: conformance_tmp/ownership-xaudio-*.log.

The Linux Debug build uses -fsanitize=undefined for compilation/linking and
SDL_UNIX_CONSOLE_BUILD=ON for a private dummy-audio dependency build. The
initial configuration lacked that headless option and stopped at SDL's
desktop-window requirement; after correcting configuration the same build
completed. The sanitizer red log identifies apu_xaudio2.c:326, not an SDL
configuration or unrelated dependency error.

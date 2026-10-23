# Historical APU state/decoder observations

Prior ADPCM header commentary included title-specific buffer/padding statistics.
The decoder behavior is unchanged by moving this narrative; these observations
are not current hardware-fidelity or audible-game acceptance proof.

```c
        /* The fourth header byte is RESERVED, not validated: the MCPX does not
         * refuse a block over it, and titles ship blocks where it is non-zero
         * (Jet Set Radio Future ends every ADPCM buffer in a 0x08 pad, and
         * 3.5-4.4% of its blocks were silenced by this test). The step index
         * is clamped for the same reason -- and it must be clamped, not
         * trusted, because step_table has 89 entries. Refusing either returned
         * 0 AFTER the first sample of each channel had been written. */
```

## Prior DSP queue performance narrative

```c
    /* The GP/EP run on their own thread (apu_core.c, mcpx_apu_dsp_thread),
     * through the frames the frame thread queues as it mixes their voices --
     * up to one output group of 8, which it waits for before consuming the
     * group's output. dsp_lock guards the GP/EP, their registers and the PCM
     * they publish (monitor.ep_pcm_*, frame_buf); whoever needs it and lock
     * takes lock first. The queue has its own lock, never held while a frame
     * runs: posting behind a running frame's dsp_lock stalled the frame thread
     * (a critical section is not fair) and made the thread a net loss. */
```

# Historical DSP DMA profiling note

Original staging-buffer comment:

```c
    /* Per-frame transfers are small; the heap round trip was ~1.5% of the APU thread. */
```

This captured percentage describes a prior run, not a generic performance
contract. The local 4096-byte staging buffer and heap fallback are unchanged.

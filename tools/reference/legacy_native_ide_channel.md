# Historical native IDE-channel placeholder

Source: XboxRecomp `1bc1dcbe3e7907e194a95fa457c206743c448bde`, `src/kernel/kernel_hal.c`.

This scalar marker did not implement the exported structure. Native consumers
now require an explicit host-owned object; the separate guest queue remains
a bounded runtime implementation. Original normalized block:

```c
/*
 * IDE channel object. Real kernels export a device object for the ATA channel;
 * drivers only ever pass it back to us, so identity is all that is required.
 */
static ULONG g_idex_channel_data = 0x49444558; /* 'IDEX' */
PVOID xbox_IdexChannelObject = &g_idex_channel_data;
```

The native import initializer also used this fallback for every unresolved
export, including unavailable data. The IDE import now remains NULL instead.

```c
            /* Point unresolved thunks to our error handler */
            xbox_kernel_thunk_table[i] = (ULONG_PTR)xbox_unresolved_thunk;
```

# Historical Prcb scratch-page bootstrap

Source: XboxRecomp `c2933c5d5cd09a1566f26d4cf4077ca1a3afacf2`, `src/kernel/xbox_memory_layout.c`.
The old bootstrap below supplied a separate zeroed block to steer a cache
probe. Current shared initialization points each KPCR at its own inline
PrcbData, matching the already-used DPC state storage. The captured scratch
address and workaround description remain here as port history.

Reference checked on 2026-10-22: [Cxbx Xbox KPCR/KPRCB declarations](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/master/src/core/kernel/common/types.h#L1981)
place PrcbData at KPCR offset 0x28 and DpcRoutineActive at KPRCB offset 0x30.
This is layout corroboration, not full kernel emulation or game fidelity proof.

```c
        /*
         * fs:[0x20] - On Xbox KPCR, this is the Prcb pointer.
         * Game code reads [fs:[0x20] + 0x250] which on the real Xbox
         * accesses a D3D cache structure. We set it to 0 so the read
         * at offset 0x250 returns 0, causing the cache init to be skipped.
         */
        /* A zeroed block rather than a null pointer. The read is
         * [fs:[0x20] + 0x250], and this used to be left at 0 so that read
         * landed on guest address 0x250 and returned zero by accident -- which
         * only worked while page zero was mapped. Pointing at real zeroed
         * memory says the same thing to the title and survives that page being
         * unmapped, which is what makes a genuine null dereference visible. */
        #define FAKE_PRCB_VA 0x00761000  /* zeroed KPCR Prcb stand-in */
        memset(XBOX_VA(FAKE_PRCB_VA), 0, 0x400);
        MEM32_INIT(XBOX_FS_BASE + 0x20, FAKE_PRCB_VA);
        #undef FAKE_PRCB_VA
```

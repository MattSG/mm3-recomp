# Historical file-volume startup observations

A captured Half-Life 2 CRT startup check motivated the original fixed geometry
comment. It is application evidence, not proof that all Xbox volumes share that
geometry. The generic runtime keeps its existing default geometry but documents
it as a model default. No new per-title switch was added.

Removed historical source note:

```text
/* Xbox volume geometry.
 *
 * FATX uses 16 KB clusters: 512-byte sectors, 32 sectors per cluster. That is
 * not cosmetic. A title's CRT startup asks for FileFsSizeInformation and
 * multiplies SectorsPerAllocationUnit by BytesPerSector, then *requires* the
 * product to equal the cluster size it was built for. Half-Life 2 checks for
 * 0x4000 and returns STATUS_DEVICE_NOT_READY (0xC000014F) otherwise, which
 * aborts CRT init before main ever runs -- the process then exits cleanly,
 * which reads as a title that did nothing rather than one that failed.
 *
 * Reporting the host's PC-typical 4 KB cluster (512 x 8) fails that check. */
```

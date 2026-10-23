# Historical kernel-data placement

Source: XboxRecomp `26385963c02165c849994f381ba0bd58131d53a3`.
The shared header fixed every port's export page at this application address.
MM3 now selects it explicitly; default runtime placement follows the image.

## src/kernel/xbox_memory_layout.h

```c
#define XBOX_KERNEL_DATA_BASE   0x00740000
```

## src/kernel/README.md

```text
Some kernel ordinals are data exports (not functions). We allocate a kernel data area:

```c
#define XBOX_KERNEL_DATA_BASE  0x00740000

// Offsets within the data area
#define KDATA_HARDWARE_INFO     0x000  // XBOX_HARDWARE_INFO struct
#define KDATA_KRNL_VERSION      0x010  // XBOX_KRNL_VERSION struct
#define KDATA_TICK_COUNT        0x020  // KeTickCount (updated by timer)
#define KDATA_HD_KEY            0x030  // XboxHDKey (16 bytes)
#define KDATA_EEPROM_KEY        0x050  // XboxEEPROMKey (16 bytes)
// ... more at documented offsets
```
```

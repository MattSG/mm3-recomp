# Historical kernel string/topology policy

Source: XboxRecomp `0f55435cee9442b891e95be38d9e75f65e38cf74`.
Original bridge/native defaults and fixed inline string buffers are retained
below as normalized reference text. MM3 now supplies its inherited values;
the runtime marshals counted strings and topology from native exports.

## src/kernel/kernel_bridge.c

```text
    /* HalDiskCachePartitionCount (ordinal 40) - number of cache partitions.
     * Retail consoles report 3 (X, Y, Z). Titles size a partition array from
     * this, so 0 gives a zero-length array and 1 hides two drives. */
    BRIDGE_MEM32(XBOX_KERNEL_DATA_BASE + KDATA_DISK_CACHE_PARTS) = 3;
```

## src/kernel/kernel_bridge.c

```text
    /* HAL disk identity strings (ordinals 41/42). The exported symbol is an
     * XBOX_ANSI_STRING whose Buffer must be an Xbox VA the title can deref --
     * HalRandGather reads the bytes for entropy. Build the struct and its text
     * inside the kernel data area so both are addressable. */
    {
        struct { uint32_t str_off, buf_off; const char *text; } d[] = {
            { KDATA_DISK_MODEL_STR,  KDATA_DISK_MODEL_BUF,  "XBOXRECOMP VIRTUAL HDD" },
            { KDATA_DISK_SERIAL_STR, KDATA_DISK_SERIAL_BUF, "XR0000000000" },
            /* XeImageFileName (ordinal 326). Declared but never filled in, so
             * its Buffer held whatever was in the page -- Half-Life 2's CRT
             * reads it while working out the running image's path, took the
             * uninitialised bytes as a char*, and dereferenced 0x68737572
             * (the ASCII "rush"). A disc-booted title's value looks like this. */
            { KDATA_XE_IMAGE_FILENAME, KDATA_XE_IMAGE_BUF,
              "\\Device\\CdRom0\\default.xbe" },
        };
        for (int k = 0; k < (int)(sizeof(d) / sizeof(d[0])); k++) {
            uint32_t str_va = XBOX_KERNEL_DATA_BASE + d[k].str_off;
            uint32_t buf_va = XBOX_KERNEL_DATA_BASE + d[k].buf_off;
            size_t len = strlen(d[k].text);
            memcpy(XBOX_TO_NATIVE(buf_va), d[k].text, len + 1);
            BRIDGE_MEM16(str_va + 0) = (uint16_t)len;        /* Length */
            BRIDGE_MEM16(str_va + 2) = (uint16_t)(len + 1);  /* MaximumLength */
            BRIDGE_MEM32(str_va + 4) = buf_va;               /* Buffer (Xbox VA) */
        }
    }
```

## src/kernel/kernel_xbox.c

```text
/* Image filename - the XBE path as seen by the kernel */
static char g_image_filename[] = "\\Device\\CdRom0\\default.xbe";
XBOX_ANSI_STRING xbox_XeImageFileName = {
    .Length        = sizeof(g_image_filename) - 1,
    .MaximumLength = sizeof(g_image_filename),
    .Buffer        = g_image_filename
};
```

## src/kernel/kernel_hal.c

```text
ULONG xbox_HalDiskCachePartitionCount = 3;

static char g_disk_model[]  = "XBOXRECOMP VIRTUAL HDD";
static char g_disk_serial[] = "XR0000000000";

XBOX_ANSI_STRING xbox_HalDiskModelNumber = {
    sizeof(g_disk_model) - 1,
    sizeof(g_disk_model) - 1,
    g_disk_model
};

XBOX_ANSI_STRING xbox_HalDiskSerialNumber = {
    sizeof(g_disk_serial) - 1,
    sizeof(g_disk_serial) - 1,
    g_disk_serial
};
```

## src/kernel/xbox_memory_layout.h

```text
/* HAL disk identity strings (ordinals 41/42). Each is an XBOX_ANSI_STRING
 * (Length, MaximumLength, Buffer VA) followed by the string bytes it points to,
 * because HalRandGather dereferences Buffer to read the text as entropy. */
#define KDATA_DISK_MODEL_STR    0x420  /* XBOX_ANSI_STRING (8 bytes) */
#define KDATA_DISK_MODEL_BUF    0x430  /* model text (up to 48 bytes) */
#define KDATA_DISK_SERIAL_STR   0x460  /* XBOX_ANSI_STRING (8 bytes) */
#define KDATA_DISK_SERIAL_BUF   0x470  /* serial text (up to 32 bytes) */
#define KDATA_DISK_CACHE_PARTS  0x4A0  /* HalDiskCachePartitionCount (4 bytes) */
/* XeImageFileName's text. The exported symbol at KDATA_XE_IMAGE_FILENAME is
 * an XBOX_ANSI_STRING, and a title dereferences its Buffer -- the CRT reads
 * it to work out the running image's path. The struct was declared without
 * anything to point at, so Buffer held whatever was in that page. */
#define KDATA_XE_IMAGE_BUF      0x4B0  /* image path text (up to 64 bytes) */
```

# Historical kernel identity policy

Source: XboxRecomp `5de8b116af3b46407478b952588eb18ce2943fbd`.
Captured defaults, environment handling and explanatory examples follow as
normalized reference text. They are not a current console/SDK ABI guarantee.
MM3 retains its inherited guest identity explicitly; generic exports now share
caller-supplied values between native and bridge paths.

## src/kernel/kernel_bridge.c

```text
    /* XboxHardwareInfo (ordinal 322) - XBOX_HARDWARE_INFO
     *   +0: ULONG Flags (0 = retail, 0x20 = devkit)
     *   +4: UCHAR GpuRevision
     *   +5: UCHAR McpRevision
     */
    BRIDGE_MEM32(XBOX_KERNEL_DATA_BASE + KDATA_HARDWARE_INFO + 0) = 0;   /* Retail */
    BRIDGE_MEM8(XBOX_KERNEL_DATA_BASE + KDATA_HARDWARE_INFO + 4) = 0xA1; /* NV2A A1 */
    BRIDGE_MEM8(XBOX_KERNEL_DATA_BASE + KDATA_HARDWARE_INFO + 5) = 0xB1; /* MCPX B1 */

    /* XboxKrnlVersion (ordinal 324) - XBOX_KRNL_VERSION
     *   +0: USHORT Major (1)
     *   +2: USHORT Minor (0)
     *   +4: USHORT Build (5849 = XDK version)
     *   +6: USHORT Qfe (0)
     */
    BRIDGE_MEM16(XBOX_KERNEL_DATA_BASE + KDATA_KRNL_VERSION + 0) = 1;
    BRIDGE_MEM16(XBOX_KERNEL_DATA_BASE + KDATA_KRNL_VERSION + 2) = 0;
    BRIDGE_MEM16(XBOX_KERNEL_DATA_BASE + KDATA_KRNL_VERSION + 4) = 5849;
    BRIDGE_MEM16(XBOX_KERNEL_DATA_BASE + KDATA_KRNL_VERSION + 6) = 0;
```

## src/kernel/kernel_bridge.c

```text
    /* XboxHDKey (323), XboxSignatureKey (325), XboxLANKey (326, 355): zeros,
     * or a console's own from RECOMP_XBOX_HDKEY / _SIGKEY / _LANKEY (32 hex
     * digits each). A real kernel decrypts them from the EEPROM; Xbox Live
     * accounts are bound to the HD key, so an account made on another
     * console (or xemu) signs in only with that console's key. */
    {
        static const struct { uint32_t off; const char *env; } k[] = {
            { KDATA_HD_KEY, "RECOMP_XBOX_HDKEY" },
            { KDATA_SIGNATURE_KEY, "RECOMP_XBOX_SIGKEY" },
            { KDATA_LAN_KEY, "RECOMP_XBOX_LANKEY" },
        };
        for (int i = 0; i < 3; i++) {
            uint8_t *p = (uint8_t *)((uintptr_t)(XBOX_KERNEL_DATA_BASE + k[i].off) + g_xbox_mem_offset);
            const char *v = getenv(k[i].env);
            memset(p, 0, 16);
            for (int b = 0; v && b < 16 && sscanf(v + 2 * b, "%2hhx", &p[b]) == 1; b++)
                ;
        }
    }

    /* XboxAlternateSignatureKeys (ordinals 327, 356) - 256 bytes of zeros */
    memset((void*)((uintptr_t)(XBOX_KERNEL_DATA_BASE + KDATA_ALT_SIGNATURE_KEYS) + g_xbox_mem_offset), 0, 256);

    /* XePublicKeyData (ordinal 357) - 284 bytes of zeros */
    memset((void*)((uintptr_t)(XBOX_KERNEL_DATA_BASE + KDATA_XE_PUBLIC_KEY) + g_xbox_mem_offset), 0, 284);
```

## src/kernel/kernel_xbox.c

```text
/* Hardware info - default report is a standard retail Xbox.
 * A target may override via xbox_kernel_set_version() or by patching the
 * global directly; no title-specific values are baked in. */
XBOX_HARDWARE_INFO xbox_HardwareInfo = {
    .Flags       = 0x00000020,  /* Retail Xbox */
    .GpuRevision = 0xD2,       /* NV2A D2 revision */
    .McpRevision = 0xD4,       /* MCPX D4 revision */
    .Reserved    = {0, 0}
};

/* Kernel version - default is a stock retail value. Titles built against a
 * different XDK can report their own through xbox_kernel_set_version(). */
XBOX_KRNL_VERSION xbox_KrnlVersion = {
    .Major = 1,
    .Minor = 0,
    .Build = 5849,
    .Qfe   = 1
};

/* Override the kernel version reported to game code (ordinal 324).
 * Match the XDK revision the target title was built with if it checks it. */
```

## docs/technical/kernel-replacement.md

```text
## Data vs Function Exports

Some kernel ordinals export data, not functions. For example:

| Ordinal | Export | Type |
|---------|--------|------|
| 17 | ExEventObjectType | Data (type object) |
| 156 | KeTickCount | Data (counter) |
| 322 | XboxHardwareInfo | Data (hardware info struct) |
| 324 | XboxKrnlVersion | Data (version struct) |

The game reads these thunk entries and **dereferences** the result to access the data:

```asm
mov eax, [0x0036B800]    ; read thunk entry for KeTickCount
mov ecx, [eax]           ; dereference to get the tick count value
```

For data exports, the thunk entry must point to actual readable memory containing the expected structure. A "kernel data area" is allocated at 0x00740000 with the necessary structures:

```c
// XboxHardwareInfo at kernel data area
BRIDGE_MEM32(XBOX_KERNEL_DATA_BASE + KDATA_HARDWARE_INFO + 0) = 0;     // Retail
BRIDGE_MEM8(XBOX_KERNEL_DATA_BASE + KDATA_HARDWARE_INFO + 4)  = 0xA1;  // NV2A rev A1
BRIDGE_MEM8(XBOX_KERNEL_DATA_BASE + KDATA_HARDWARE_INFO + 5)  = 0xB1;  // MCPX rev B1

// XboxKrnlVersion
BRIDGE_MEM16(XBOX_KERNEL_DATA_BASE + KDATA_KRNL_VERSION + 0) = 1;     // Major
BRIDGE_MEM16(XBOX_KERNEL_DATA_BASE + KDATA_KRNL_VERSION + 2) = 0;     // Minor
BRIDGE_MEM16(XBOX_KERNEL_DATA_BASE + KDATA_KRNL_VERSION + 4) = 5849;  // Build (XDK version)

// KeTickCount - updated every ~1ms by a background thread
BRIDGE_MEM32(XBOX_KERNEL_DATA_BASE + KDATA_TICK_COUNT) = GetTickCount();
```
```

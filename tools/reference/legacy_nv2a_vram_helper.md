# Removed unused NV2A VRAM fault placeholder

No source caller was found for nv2a_hook_handle_vram. Its host-page allocations
were not device aliases and had no allocation ownership. Guest mappings belong
to the memory-layout layer. The unused API and its fallback-host stub were
removed; this is archived historical code, not integration guidance.

```c
bool nv2a_hook_handle_vram(uintptr_t fault_addr, uint32_t fault_xbox_va)
{
    /* For VRAM range (0xF0000000-0xF3FFFFFF), allocate pages as before.
     * In the future, we can map these to NV2A VRAM for push buffer DMA.
     * For now, just allocate writable pages. */
    uintptr_t alloc_base = fault_addr & ~(uintptr_t)0xFFFF;
    LPVOID result = VirtualAlloc((LPVOID)alloc_base, 0x10000,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!result) {
        result = VirtualAlloc((LPVOID)alloc_base, 0x10000,
                              MEM_COMMIT, PAGE_READWRITE);
    }
    if (result) {
        memset(result, 0, 0x10000);
        return true;
    }
    return false;
}

```

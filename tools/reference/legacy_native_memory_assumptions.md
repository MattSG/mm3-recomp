# Historical native memory wrapper assumptions

Prior comments described GPU handling as justification for partial native
allocation wrappers. These are archived implementation assumptions, not proof
of guest allocation, alignment, protection or physical-address semantics.

```text
 * Implements Mm* and NtAllocateVirtualMemory/NtFreeVirtualMemory/NtQueryVirtualMemory
 * using Win32 VirtualAlloc/VirtualFree/VirtualQuery.
 *
 * Xbox contiguous memory (MmAllocateContiguousMemory) is used for GPU-accessible
 * buffers. On Windows, actual GPU resources are handled by our D3D11 layer;
 * these allocations just need to return valid CPU-accessible memory.
    /*
     * Xbox requests physically contiguous, aligned memory for GPU use.
     * We can't guarantee physical contiguity on Windows, but the game's
     * CPU-side code just needs a valid pointer. GPU resources are handled
     * separately by our D3D11 layer.
     *
     * Use _aligned_malloc for alignment, then VirtualAlloc for a fallback.
     */
```

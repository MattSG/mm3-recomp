# Historical kernel integration guide

Source: XboxRecomp `a66c66c72b73cb6ad22638bfa5e15ce514d9c7ec`, `src/kernel/README.md`.

This is the normalized original, including obsolete title memory addresses,
API descriptions and implementation claims. Relative links use the fork root
context. It is historical evidence, not current integration guidance.

---

# xbox_kernel — Xbox Kernel Replacement Layer

Replaces all 371 Xbox kernel exports (347 function ordinals + 24 data exports) with Win32 equivalents. This is the foundation layer that every recompiled game needs — it provides memory layout, file I/O, threading, synchronization, and hardware abstraction.

All 371 ordinals are routed: every export now has either a per-ordinal `bridge_*` wrapper (real implementation or a documented stub) or a kernel data export entry. Verified by `tools/kernel_audit/test_bridge_ordinals.py`, which cross-checks routing, argument sizes, and data exports against the reference export table.

## Files

| File | LOC | Purpose |
|------|-----|---------|
| `kernel.h` | 829 | Master public header — all types, constants, and function prototypes |
| `xbox_memory_layout.h` | 204 | Memory layout constants and API |
| `xbox_memory_layout.c` | 534 | Memory mapping (CreateFileMapping for RAM + 28 reserved, unmapped slots) |
| `kernel_bridge.c` | 8,390 | Core dispatcher — thunk table, ICALL resolution, all 371 per-ordinal bridges, kernel data exports |
| `kernel_file.c` | 834 | File I/O — NtCreateFile, NtReadFile, path translation |
| `kernel_thread.c` | 344 | Threading — PsCreateSystemThreadEx, thread priorities |
| `kernel_sync.c` | 547 | Synchronization — events, semaphores, mutexes, critical sections |
| `kernel_memory.c` | 323 | Memory allocation — MmAllocateContiguousMemory, page alloc |
| `kernel_rtl.c` | 334 | Runtime library — string ops, memory copy, ANSI/Unicode conversion |
| `kernel_hal.c` | 453 | Hardware abstraction — PCI, timers, DPC, IRQLs |
| `kernel_crypto.c` | 409 | Cryptography — SHA1, RC4, HMAC |
| `kernel_thunks.c` | 414 | Thunk layer for recompiled code → kernel dispatch |
| `kernel_ob.c` | 126 | Object manager — handle table, reference counting |
| `kernel_io.c` | 242 | Device I/O control |
| `kernel_path.c` | 199 | Xbox path parsing (`\Device\CdRom0\` → Windows path) |
| `kernel_pool.c` | 49 | Pool memory allocator |
| `kernel_xbox.c` | 121 | Misc Xbox APIs (XeLoadSection, etc.) |

## Memory Layout

The Xbox has 64 MB of unified RAM. We reproduce the exact address layout using `CreateFileMapping` + `MapViewOfFileEx`:

```
Xbox VA Range          Purpose                  Size
─────────────────────────────────────────────────────
0x00010000-0x00010BCC  XBE header               ~3 KB
0x00011000-0x002BBFFF  .text (code)             2.73 MB
0x0036B7C0-0x003B2363  .rdata (constants)       283 KB
0x003B2360-0x0076FFFF  .data (globals + BSS)    3.8 MB
0x00700000-0x0073FFFF  RW data copy             256 KB
0x00740000-0x0077FFFF  Kernel data exports      256 KB
0x00760000-0x0076FFFF  TLS area                 64 KB
0x00780000-0x00F7FFFF  Stack (8 MB, grows down) 8 MB
0x00F80000-0x03FFFFFF  Heap (bump allocator)    ~49 MB
```

### Why CreateFileMapping?

RAM is one file mapping. `CreateFileMapping` backs it, and `MapViewOfFileEx` places the view at the Xbox base, so the RAM the guest sees is a single set of pages.

The space above RAM is **not** a second copy of RAM. A virtual address on the console goes through page tables, and a page with no entry faults; the 26-bit bus wrap is a property of physical addresses, not of virtual ones. So the 28 map-sized slots above the mapped range are reserved with `PAGE_NOACCESS` and never mapped. A guest access there faults, and the fault handler names the address. Earlier builds mapped them as views of RAM, so a read at `0x08000000` returned `0x00000000`; titles built corrupt structures on that alias, and it is gone.

```c
// RAM: one file mapping, viewed at the Xbox base (a host address)
HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 64*1024*1024, NULL);
MapViewOfFileEx(h, FILE_MAP_ALL_ACCESS, 0, 0, XBOX_TOTAL_RAM, target_base);

// The 28 slots above the mapped range: reserved, never mapped
for (int i = 1; i <= 28; i++)
    VirtualAlloc(target_base + i * 64MB, 64MB, MEM_RESERVE, PAGE_NOACCESS);
```

### Global State

```c
// Defined in xbox_memory_layout.c, used everywhere
extern ptrdiff_t g_xbox_mem_offset;  // Add to Xbox VA → get native pointer

// Simulated x86 registers (the register model)
extern uint32_t g_eax, g_ecx, g_edx, g_esp;  // Volatile
extern uint32_t g_ebx, g_esi, g_edi;          // Callee-saved
extern uint32_t g_seh_ebp;                     // SEH frame pointer
```

### API

```c
// Initialize — call FIRST before anything else
BOOL xbox_MemoryLayoutInit(const void *xbe_data, size_t xbe_size);
void xbox_MemoryLayoutShutdown(void);

// Query
void     *xbox_GetMemoryBase(void);    // Native pointer to Xbox VA 0
ptrdiff_t xbox_GetMemoryOffset(void);  // g_xbox_mem_offset value
BOOL      xbox_IsXboxAddress(uintptr_t address);
HANDLE    xbox_GetMappingHandle(void); // File mapping behind the RAM view

// Heap (bump allocator, no free)
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);
void     xbox_HeapFree(uint32_t xbox_va);  // No-op currently
```

### Customizing for Your Game

Edit `xbox_memory_layout.h` to match your XBE's section layout:

```c
#define XBOX_TEXT_VA       0x00011000   // From XBE section headers
#define XBOX_TEXT_SIZE     2863616      // .text section size
#define XBOX_RDATA_VA      0x0036B7C0   // .rdata section
// ... etc
```

The `xbe_parser` tool outputs these values for any XBE.

## Kernel Thunk Table

The Xbox kernel exposes functions via ordinal numbers. Games import them through a thunk table at a fixed address. We reproduce this:

```c
// The XBE loader supplies this title's address/count; there is no title default.
xbox_kernel_set_thunk_address(thunk_va, thunk_count);

// Storage is bounded by the kernel export directory (378 slots).
extern ULONG_PTR xbox_kernel_thunk_table[XBOX_KERNEL_THUNK_TABLE_SIZE];
```

### Initialization

```c
xbox_kernel_init();         // Populate thunk table with ordinal → function mappings
xbox_kernel_bridge_init();  // Write thunk entries into Xbox memory, set up kernel data area
```

### Kernel Data Exports

Data exports use an owned page after the loaded image by default.
`XBOX_KERNEL_DATA_BASE` is a runtime address. Ports needing a fixed address
must call `xbox_ConfigureKernelData` before initialization; overlapping or
unbacked placement is rejected. Standalone bridge hosts must explicitly select
and map their page. Use `xbox_memory_layout.h` for the current offsets.

## File I/O

Xbox file I/O uses NT-style APIs (NtCreateFile, NtReadFile). We translate Xbox paths to Windows:

```
Xbox Path                    → Windows Path
───────────────────────────────────────────────
\Device\CdRom0\Data\foo.dat → <game_dir>\Data\foo.dat
D:\Data\foo.dat              → <game_dir>\Data\foo.dat
T:\save.dat                  → <save_dir>\save.dat
Z:\cache.dat                 → <save_dir>\cache\cache.dat
```

### Path Setup

```c
// Call early — before game tries to open files
xbox_path_init("Burnout 3 Takedown", "saves");
```

## Threading

Guest thread creation uses `kernel_bridge.c` with guest dispatch, owned guest
stacks and per-thread CPU/TIB state. Port configuration controls explicit inline
compatibility; default worker creation is independent and resource failures
are reported. See [memory/runtime ownership](../../docs/technical/memory-layout.md).

The native `xbox_PsCreateSystemThread`/`Ex` facade accepts host callbacks.
`PXBOX_START_ROUTINE` takes one context; `PXBOX_SYSTEM_ROUTINE` takes the start
routine and context. Ex uses a supplied system routine or calls the start
routine directly. It returns host handles/IDs and uses host suspension/stack
primitives. Native extension/TLS/debugger setup is unsupported and reported;
it does not accept raw guest function addresses. Callback types/signatures are
in `kernel.h`, matching the arity described by the
[nxdk kernel header](https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h).

## Synchronization

Full implementations of:

| Xbox API | Win32 Implementation |
|----------|---------------------|
| NtCreateEvent | CreateEvent |
| NtSetEvent / KeSetEvent | SetEvent |
| NtWaitForSingleObject | WaitForSingleObject |
| NtWaitForMultipleObjectsEx | WaitForMultipleObjects |
| NtCreateSemaphore | CreateSemaphore |
| NtReleaseSemaphore | ReleaseSemaphore |
| RtlInitializeCriticalSection | InitializeCriticalSection |
| RtlEnterCriticalSection | EnterCriticalSection |
| RtlLeaveCriticalSection | LeaveCriticalSection |

## NTSTATUS Codes

```c
#define STATUS_SUCCESS                0x00000000
#define STATUS_TIMEOUT                0x00000102
#define STATUS_PENDING                0x00000103
#define STATUS_NO_MEMORY              0xC0000017
#define STATUS_ACCESS_DENIED          0xC0000022
#define STATUS_OBJECT_NAME_NOT_FOUND  0xC0000034
#define STATUS_OBJECT_NAME_COLLISION  0xC0000035
// ... full list in kernel.h

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
```

## Calling Conventions

| Convention | Used By | Example |
|-----------|---------|---------|
| `__stdcall` | Most kernel functions | `xbox_NtCreateFile(...)` |
| `__fastcall` | Kf* and ObfDereference | `xbox_KfRaiseIrql(NewIrql)` |
| `__cdecl` | Variadic functions | `xbox_DbgPrint(format, ...)` |

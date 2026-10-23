# Historical kernel-helper claims

From fork f2bbe30. These comments are history, not current correctness evidence.

## src/kernel/kernel_ob.c

```c
/*
 * kernel_ob.c - Object Manager
 *
 * Implements object reference counting and handle-to-object resolution.
 *
 * The Xbox kernel uses a simplified NT object manager. Objects have reference
 * counts and can be looked up by handle or name. Since we use Win32 HANDLEs
 * directly, most of these are thin wrappers.
 *
 * ObfReferenceObject/ObfDereferenceObject use __fastcall on Xbox (parameter
 * in ECX register).
 */
```

## src/kernel/kernel_ob.c

```c
/* ============================================================================
 * Type Object Pointers
 *
 * Xbox kernel exports type objects for common types. Game code uses these
 * with ObReferenceObjectByHandle to validate handle types.
 * We provide dummy non-NULL pointers so type checks pass.
 * ============================================================================ */
```

## src/kernel/kernel_ob.c

```c
/* ============================================================================
 * Reference Counting
 *
 * On Xbox, kernel objects have embedded reference counts. On Windows,
 * HANDLEs manage their own lifetime, so we track references loosely.
 *
 * ObfDereferenceObject: Decrements ref count. If it reaches zero on Xbox,
 * the object is freed. We approximate by closing the handle on deref.
 * However, since game code may hold multiple references, we only close
 * when explicitly told via NtClose. These become approximate no-ops.
 * ============================================================================ */
```

## src/kernel/kernel_ob.c

```c
/*
     * Increment reference count. With Win32 handles, the OS manages
     * the underlying reference count via DuplicateHandle/CloseHandle.
     * We don't need to track this separately for correctness.
     */
```

## src/kernel/kernel_ob.c

```c
/*
     * Decrement reference count. We don't close the handle here because
     * game code may still use it. The handle is closed via NtClose.
     */
```

## src/kernel/kernel_pool.c

```c
/*
 * kernel_pool.c - Xbox Pool Allocator
 *
 * Implements ExAllocatePool, ExAllocatePoolWithTag, ExFreePool,
 * ExQueryPoolBlockSize using the Win32 process heap.
 *
 * On Xbox, pool memory is kernel-mode nonpaged/paged pool.
 * Since we run entirely in user mode on Windows, we just use HeapAlloc.
 */
```

## src/kernel/kernel_guest_io.h

```c
/* Fixed-width Xbox kernel I/O layouts. Sized against XboxDev/nxdk master at
 * 14d5ee97e73347c973f1f57b68b79ec08c9e77f2, compiled for its i386-pc-win32
 * target; see build/audit-continuation/reference/xbox-io-layout.md. These are
 * guest memory objects, not
 * host WDM structures: every pointer field is a 32-bit Xbox virtual address. */
```

## src/kernel/kernel_guest_io.h

```c
/* Xbox packet layout: a four-byte header followed by the complete 20-byte
 * parameter payload. The regression test records the title-binary byte
 * evidence for the observed parameter offsets; a Cxbx record-layout probe
 * places Parameters at +8 and is not used as the guest-layout authority. */
```

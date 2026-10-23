# Historical object-reference bridge

Source: XboxRecomp `081a77d433291b58663af73e886bc4b22ff1d3d4`, `src/kernel/kernel_bridge.c`.

This compatibility bridge fabricates one exited thread for every handle and
passes other tokens through. Its policy moves to the MM3 host; this is not
thread/object fidelity. The original normalized function follows.

```c
static void bridge_ObReferenceObjectByHandle(void)
{
    /* Xbox: NTSTATUS ObReferenceObjectByHandle(HANDLE Handle, PVOID ObjectType, PVOID* Object)
     * 3 args (not 6 like Windows NT) */
    static uint32_t thread_object;
    uint32_t handle = STACK_ARG(0);
    uint32_t obj_type = STACK_ARG(1);
    uint32_t object_ptr = STACK_ARG(2);

    if (!object_ptr) {
        g_eax = 0xC000000Du;                 /* STATUS_INVALID_PARAMETER */
    } else if (!handle || handle == UINT32_MAX) {
        BRIDGE_MEM32(object_ptr) = 0;
        g_eax = 0xC0000008u;                 /* STATUS_INVALID_HANDLE */
    } else if (obj_type == XBOX_KERNEL_DATA_BASE + KDATA_THREAD_OBJ_TYPE) {
        /* A thread handle is a host token, not a guest KTHREAD, and titles
         * read the object: a worker's ExitStatus can be polled
         * (+0x120) until it leaves STATUS_PENDING. Hand out one zeroed guest
         * thread object, signalled (+4) and finished (ExitStatus 0).
         * ponytail: every thread reads as exited; track per-handle objects
         * if a title polls a thread that is still meant to be running. */
        if (!thread_object) {
            thread_object = xbox_HeapAlloc(0x124u, 16u);
            if (thread_object) {
                memset(XBOX_TO_NATIVE(thread_object), 0, 0x124u);
                BRIDGE_MEM32(thread_object + 4u) = 1;
            }
        }
        BRIDGE_MEM32(object_ptr) = thread_object;
        g_eax = thread_object ? 0 : 0xC0000017u;    /* STATUS_NO_MEMORY */
    } else {
        /* Other objects: the guest token itself, which is what the bridge's
         * object calls take. NULL turned success into a null dereference. */
        BRIDGE_MEM32(object_ptr) = handle;
        g_eax = 0;                           /* STATUS_SUCCESS */
    }
}
```

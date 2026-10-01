#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#include <xbox/xboxrecomp.h>

#include "recomp_types.h"
#include "recomp_funcs.h"
#include "host_graphics.h"
#include <nv2a/nv2a_mmio_hook.h>

#define MM3_XBE_PATH "game_files/default.xbe"
#define MM3_GAME_DIR "game_files"
#define MM3_KERNEL_THUNK_ADDRESS 0x00361F00u
#define MM3_KERNEL_THUNK_COUNT 151u
#define MM3_MEMORY_MAP_SIZE (128u * 1024u * 1024u)
#define MM3_APU_BASE 0xFE800000u
#define MM3_APU_END  0xFE880000u

typedef struct MCPXAPUState MCPXAPUState;
extern MCPXAPUState *g_apu_state;
extern MCPXAPUState *mcpx_apu_init_standalone(uint8_t *ram_ptr);
extern void mcpx_apu_shutdown(MCPXAPUState *apu);
extern bool apu_hook_handle_mmio(PCONTEXT context, uintptr_t fault_address,
                                 uint32_t guest_address, int is_write);

static PVOID g_mm3_apu_veh;

static DWORD WINAPI mm3_wait_for_worker(LPVOID context)
{
    HANDLE done = (HANDLE)context;

    for (;;) {
        HANDLE guest_thread = (HANDLE)xbox_thread_debug_handle();
        HANDLE duplicate = NULL;

        if (guest_thread && DuplicateHandle(GetCurrentProcess(), guest_thread,
                                            GetCurrentProcess(), &duplicate,
                                            SYNCHRONIZE, FALSE, 0)) {
            WaitForSingleObject(duplicate, INFINITE);
            CloseHandle(duplicate);
            SetEvent(done);
            return 0;
        }

        Sleep(1);
    }
}

static LONG WINAPI mm3_crash_report(EXCEPTION_POINTERS *info)
{
    const EXCEPTION_RECORD *record = info ? info->ExceptionRecord : NULL;
    const CONTEXT *context = info ? info->ContextRecord : NULL;
    ULONG_PTR fault = record && record->NumberParameters > 1
        ? record->ExceptionInformation[1] : 0;
    uintptr_t module_base = (uintptr_t)GetModuleHandleW(NULL);
    fprintf(stderr, "[CRASH] code=0x%08lX address=%p guest_esp=0x%08X eax=0x%08X\n",
            record ? record->ExceptionCode : 0,
            record ? record->ExceptionAddress : NULL,
            g_esp, g_eax);
    fprintf(stderr, "[CRASH] fault=%p rip=%p rsp=%p rax=%llX rdx=%llX\n",
            (void *)fault,
            context ? (void *)context->Rip : NULL,
            context ? (void *)context->Rsp : NULL,
            context ? context->Rax : 0,
            context ? context->Rdx : 0);
    fprintf(stderr, "[CRASH] module=%p rip_rva=0x%llX\n",
            (void *)module_base,
            context ? (unsigned long long)((uintptr_t)context->Rip - module_base) : 0);
    fprintf(stderr, "[CRASH] xbox_mem_offset=%lld memory_base=%p\n",
            (long long)g_xbox_mem_offset, xbox_GetMemoryBase());
    fprintf(stderr, "[CRASH] guest fs=0x%08X ebp=0x%08X seh_ebp=0x%08X ecx=0x%08X ebx=0x%08X esi=0x%08X edi=0x%08X\n",
            g_fs_base, g_ebp, g_seh_ebp, g_ecx, g_ebx, g_esi, g_edi);
    fprintf(stderr, "[CRASH] frame+8=0x%08X frame+c=0x%08X frame+10=0x%08X obj+18=0x%08X obj+580=0x%08X callback=0x%08X fs+20=0x%08X fs20+250=0x%08X\n",
            MEM32(g_seh_ebp + 8), MEM32(g_seh_ebp + 0x0C),
            MEM32(g_seh_ebp + 0x10), MEM32(MEM32(g_seh_ebp + 8) + 0x18),
            MEM32(MEM32(g_seh_ebp + 8) + 0x580),
            MEM32(0x00362018),
            MEM32(g_fs_base + 0x20), MEM32(MEM32(g_fs_base + 0x20) + 0x250));
    fprintf(stderr, "[CRASH] stack=0x%08X +4=0x%08X +8=0x%08X\n",
            MEM32(g_esp), MEM32(g_esp + 4), MEM32(g_esp + 8));
    if (g_esi < MM3_MEMORY_MAP_SIZE - 0x580u) {
        fprintf(stderr, "[CRASH] heap=0x%08X bucket=%u\n", g_esi, g_edi);
        for (uint32_t i = 0; i < 5; ++i) {
            uint32_t bucket = g_esi + 0x180u + i * 8u;
            fprintf(stderr, "[CRASH] heap_bin[%u]=%08X/%08X\n",
                    i, MEM32(bucket), MEM32(bucket + 4));
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG CALLBACK mm3_apu_mmio_handler(PEXCEPTION_POINTERS info)
{
    uintptr_t fault_address;
    uint32_t guest_address;

    if (!g_apu_state || !info ||
        info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;

    fault_address = info->ExceptionRecord->ExceptionInformation[1];
    guest_address = (uint32_t)(fault_address - (uintptr_t)g_xbox_mem_offset);
    if (guest_address < MM3_APU_BASE || guest_address >= MM3_APU_END)
        return EXCEPTION_CONTINUE_SEARCH;

    return apu_hook_handle_mmio(
        info->ContextRecord, fault_address, guest_address,
        info->ExceptionRecord->ExceptionInformation[0] ? 1 : 0)
        ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

static int mm3_apu_init(void)
{
    if (!getenv("RECOMP_AC97_READY"))
        return 1;

    g_apu_state = mcpx_apu_init_standalone((uint8_t *)xbox_GetMemoryBase());
    if (!g_apu_state) {
        fprintf(stderr, "[APU] initialization failed\n");
        return 0;
    }

    g_mm3_apu_veh = AddVectoredExceptionHandler(1, mm3_apu_mmio_handler);
    if (!g_mm3_apu_veh) {
        fprintf(stderr, "[APU] could not install MMIO exception handler\n");
        mcpx_apu_shutdown(g_apu_state);
        g_apu_state = NULL;
        return 0;
    }

    fprintf(stderr, "[APU] emulated APU up; MMIO handler installed\n");
    return 1;
}

static void mm3_apu_shutdown(void)
{
    if (g_mm3_apu_veh) {
        RemoveVectoredExceptionHandler(g_mm3_apu_veh);
        g_mm3_apu_veh = NULL;
    }
    if (g_apu_state) {
        mcpx_apu_shutdown(g_apu_state);
        g_apu_state = NULL;
    }
}

static int load_file(const char *path, void **data, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;

    if (!file)
        return 0;
    if (fseek(file, 0, SEEK_END) != 0)
        goto fail;
    length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0)
        goto fail;

    *data = malloc((size_t)length);
    if (!*data || fread(*data, 1, (size_t)length, file) != (size_t)length) {
        free(*data);
        *data = NULL;
        goto fail;
    }

    *size = (size_t)length;
    fclose(file);
    return 1;

fail:
    fclose(file);
    return 0;
}

int main(void)
{
    void *xbe_data = NULL;
    size_t xbe_size = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    SetUnhandledExceptionFilter(mm3_crash_report);
    puts("MM3 static recompilation");

    if (!load_file(MM3_XBE_PATH, &xbe_data, &xbe_size)) {
        fprintf(stderr, "cannot load %s\n", MM3_XBE_PATH);
        return 1;
    }

    /* Keep the emulated RAM at 64 MB, but leave virtual address space above
     * it for MM3's large MEM_RESERVE requests. */
    xbox_SetMapSize(MM3_MEMORY_MAP_SIZE);
    if (!xbox_MemoryLayoutInit(xbe_data, xbe_size)) {
        fprintf(stderr, "xbox_MemoryLayoutInit failed\n");
        free(xbe_data);
        return 1;
    }

    g_xbox_mem_offset = xbox_GetMemoryOffset();
    xbox_kernel_set_thunk_address(MM3_KERNEL_THUNK_ADDRESS,
                                   MM3_KERNEL_THUNK_COUNT);
    xbox_kernel_init();
    xbox_path_init(MM3_GAME_DIR, getenv("MM3_SAVE_DIR"));
    xbox_kernel_bridge_init();
    if (xbox_Nv2aMirrorFence(0x00351F48u, 0x2Cu, 0x30u) != 0)
        fprintf(stderr, "NV2A fence mirror registration failed\n");
    if (!mm3_apu_init()) {
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    if (!mm3_graphics_init()) {
        mm3_apu_shutdown();
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    nv2a_hook_init(g_xbox_mem_offset);
    /* Retail behavior is SPAWN. INLINE is a bounded single-thread diagnostic
     * that removes cross-thread trace interleaving while locating startup. */
    xbox_SetThreadMode(getenv("MM3_THREAD_MODE") &&
                       _stricmp(getenv("MM3_THREAD_MODE"), "inline") == 0
                           ? XBOX_THREAD_MODE_INLINE
                           : XBOX_THREAD_MODE_SPAWN);

    if (!recomp_dispatch_init())
        fprintf(stderr, "dispatch table allocation failed; using binary lookup\n");

    HANDLE worker_done = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE worker_waiter = worker_done
        ? CreateThread(NULL, 0, mm3_wait_for_worker, worker_done, 0, NULL)
        : NULL;
    if (!worker_waiter) {
        fprintf(stderr, "cannot create MM3 worker monitor\n");
        mm3_apu_shutdown();
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }

    /* Optional RECOMP_WATCHDOG_SECS diagnostic; must run on the guest thread. */
    xbox_WatchdogStart();
    fprintf(stderr, "[CALLBACK_INIT] 392834=%08X 392838=%08X 391F10=%08X 391F14=%08X 391F18=%08X\n",
            MEM32(0x392834), MEM32(0x392838), MEM32(0x391F10),
            MEM32(0x391F14), MEM32(0x391F18));
    fprintf(stderr, "[CALLBACK_INIT] 391F00=%08X 391F04=%08X 391F08=%08X\n",
            MEM32(0x391F00), MEM32(0x391F04), MEM32(0x391F08));
    if (getenv("MM3_TRACE_BOOT_STATE")) {
        fprintf(stderr,
                "[BOOT_STATE] host_tid=%lu eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X fs=%08X stack=%08X,%08X,%08X fsdata=%08X,%08X,%08X,%08X kernel=%08X,%08X,%08X kdata=%08X\n",
                GetCurrentThreadId(), g_eax, g_ebx, g_ecx, g_edx, g_esi,
                g_edi, g_ebp, g_esp, g_fs_base, MEM32(g_esp),
                MEM32(g_esp + 4), MEM32(g_esp + 8), MEM32(g_fs_base),
                MEM32(g_fs_base + 4), MEM32(g_fs_base + 8),
                MEM32(g_fs_base + 0x1C), MEM32(0x10108), MEM32(0x10118),
                MEM32(0x10128), MEM32(MEM32(0x10118)));
    }
    puts("starting XBE entry point 0x00083C55");
    xbe_entry_point();
    WaitForSingleObject(worker_done, INFINITE);
    CloseHandle(worker_waiter);
    CloseHandle(worker_done);

    mm3_apu_shutdown();
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(xbe_data);
    return 0;
}

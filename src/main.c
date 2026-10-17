#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "platform/xbox_winnt.h"

#include <xbox/xboxrecomp.h>

#include "recomp_types.h"
#include "recomp_funcs.h"
#include "mods/teleport.h"
#include "mm3_asset_cache.h"
#include "mm3_frame_limit.h"
#include "mm3_performance_profile.h"
#include "mods/mods.h"
#include <xbox_host.h>
#include <xinput_xbox.h>
#include <video/fb_present.h>
#include <video/nv2a_d3d11.h>

#define MM3_XBE_PATH "game_files/default.xbe"
#define MM3_GAME_DIR "game_files"
#define MM3_KERNEL_THUNK_ADDRESS 0x00361F00u
#define MM3_KERNEL_THUNK_COUNT 151u
#define MM3_MEMORY_MAP_SIZE (128u * 1024u * 1024u)

static HANDLE g_mm3_main_worker;
static HANDLE g_mm3_main_worker_ready;
static int g_mm3_inline_threads;
extern void sub_00083A6C_original(void);
extern void mm3_configure_diagnostics(void);

static void mm3_close_game(void)
{
    /* Xbox guest workers do not observe a host WM_CLOSE. Freeing their shared
     * memory while they run is unsafe; end the process as the title's existing
     * firmware-exit bridge does. Windows releases its file/device handles. */
    fprintf(stderr, "[MM3_EXIT] close requested\n");
    fflush(stderr);
    ExitProcess(0);
}

static BOOL WINAPI mm3_console_control(DWORD event)
{
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT &&
        event != CTRL_CLOSE_EVENT)
        return FALSE;
    mm3_close_game();
    return TRUE;
}
int mm3_launcher(void);

/* Register the first XAPI worker before it can create background workers.
 * The runtime's debug handle changes on every spawn and cannot determine
 * the lifetime of the game. This wrapper preserves the guest register ABI. */
void sub_00083A6C(void)
{
    if (!g_mm3_inline_threads &&
        !InterlockedCompareExchangePointer((PVOID volatile *)&g_mm3_main_worker,
                                           NULL, NULL)) {
        HANDLE current = NULL;
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                             GetCurrentProcess(), &current,
                             SYNCHRONIZE, FALSE, 0)) {
            fprintf(stderr, "cannot register MM3 main worker: %lu\n", GetLastError());
            ExitProcess(1);
        }
        if (InterlockedCompareExchangePointer((PVOID volatile *)&g_mm3_main_worker,
                                              current, NULL)) {
            CloseHandle(current);
        } else {
            fprintf(stderr, "[MM3_MAIN_THREAD] tid=%lu entry=00083A6C\n",
                    GetCurrentThreadId());
            SetEvent(g_mm3_main_worker_ready);
        }
    }
    sub_00083A6C_original();
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
    {
        void *frames[16];
        USHORT count = CaptureStackBackTrace(0, 16, frames, NULL);
        USHORT i;
        for (i = 0; i < count; ++i)
            fprintf(stderr, "[CRASH_NATIVE] frame=%u rva=0x%llX\n",
                    (unsigned)i,
                    (unsigned long long)((uintptr_t)frames[i] - module_base));
        if (g_ebp >= 0x10000u && g_ebp < MM3_MEMORY_MAP_SIZE - 8u)
            fprintf(stderr, "[CRASH_CALLER] frame=%08X parent=%08X return=%08X\n",
                    g_ebp, MEM32(g_ebp), MEM32(g_ebp + 4u));
    }
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
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
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

    mm3_configure_diagnostics();
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Unbuffered redirected stderr made diagnostic formatting issue many
     * tiny Windows writes and stalled the renderer on the shared FILE lock.
     * Movie markers and crash reports explicitly flush when needed. */
    static char stderr_buffer[64 * 1024];
    const char *unbuffered = getenv("MM3_LOG_UNBUFFERED");
    if (unbuffered && *unbuffered && strcmp(unbuffered, "0") != 0)
        setvbuf(stderr, NULL, _IONBF, 0);
    else
        setvbuf(stderr, stderr_buffer, _IOFBF, sizeof stderr_buffer);
    SetUnhandledExceptionFilter(mm3_crash_report);
    xbox_FramebufferSetCloseHandler(mm3_close_game);
    SetConsoleCtrlHandler(mm3_console_control, TRUE);
    puts("MM3 static recompilation");
    /* Profiles seed settings before the launcher; saved choices, including
     * Auto resolution and window aspect, take precedence when Play exports them. */
    const char *profile_error = mm3_apply_performance_defaults(getenv("MM3_PERFORMANCE_PROFILE"));
    if (profile_error) {
        fprintf(stderr, "cannot select MM3 performance setting %s\n", profile_error);
        return 1;
    }

    /* The profile pins internal resolution while the window changes size.
     * Explicit aspect and auto settings take priority over the default. */
    const char *widescreen = getenv("MM3_MOD_WIDESCREEN");
    const char *default_aspect = widescreen && strcmp(widescreen, "0") == 0 ? "4:3" : "16:10";
    if (!getenv("RECOMP_ASPECT") &&
        _putenv_s("RECOMP_ASPECT", default_aspect) != 0) {
        fprintf(stderr, "cannot select MM3 display aspect\n");
        return 1;
    }

    /* Scripts set RECOMP_PB_EXEC themselves; a plain start gets the launcher. */
    const char *scripted = getenv("RECOMP_PB_EXEC");
    if ((!scripted || !*scripted) && !mm3_launcher())
        return 0;

    /* Apply the same bounded rate before either vblank or presentation
     * caches its setting, including launches that bypass scripts/play.py. */
    char frame_limit[16];
    snprintf(frame_limit, sizeof frame_limit, "%d",
             mm3_frame_limit(getenv("RECOMP_FPS_LIMIT")));
    if (_putenv_s("RECOMP_FPS_LIMIT", frame_limit) != 0) {
        fprintf(stderr, "cannot set MM3 frame-rate limit\n");
        return 1;
    }

    /* MM3 releases large model buffers through NtFreeVirtualMemory when a
     * scene is destroyed. The shared runtime leaves NT heap reclamation
     * opt-in; without it each race/menu round trip retains those buffers
     * until the guest heap is exhausted. Enable before its first allocation,
     * including direct executable launches. */
    if (_putenv_s("RECOMP_HEAP_RECLAIM", "1") != 0) {
        fprintf(stderr, "cannot enable MM3 guest memory reclamation\n");
        return 1;
    }

    /* The audio DSP programs the game runs are saved here; the next build
     * recompiles them to C (CMakeLists.txt, XBOXRECOMP_DSP_PROGRAM_DIR). */
    if (!getenv("RECOMP_DSP_PROGRAM_DIR") &&
        _putenv_s("RECOMP_DSP_PROGRAM_DIR", MM3_GAME_DIR "/dsp") != 0) {
        fprintf(stderr, "cannot set the DSP program folder\n");
        return 1;
    }

    fprintf(stderr, "[MM3_PERFORMANCE] profile=%s scale=%s msaa=%s distance=%s aniso=%s fps=%s\n",
            mm3_performance_profile(getenv("MM3_PERFORMANCE_PROFILE"))->name,
            getenv("RECOMP_RENDER_SCALE"), getenv("RECOMP_MSAA"),
            getenv("RECOMP_DRAW_DISTANCE"), getenv("RECOMP_ANISO"), getenv("RECOMP_FPS_LIMIT"));

    if (!load_file(MM3_XBE_PATH, &xbe_data, &xbe_size)) {
        fprintf(stderr, "cannot load %s\n", MM3_XBE_PATH);
        return 1;
    }

    /* MM3's DirectSoundCreate waits for the AC'97 codec-ready bit. Route the
     * request through the runtime APU/MMIO model so the codec and DSP handshake
     * are initialized together; otherwise DSERR_NODRIVER leaves its audio
     * engine object half-built and the title later dereferences a stale vtable. */
    if (_putenv_s("RECOMP_AC97_READY", "1") != 0) {
        fprintf(stderr, "cannot enable MM3 AC97/APU initialization\n");
        free(xbe_data);
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
    mm3_mods_init();
    /* XPP submits stack-backed USB requests whose callback signals an inline
     * KEVENT. Its explicit wait treats an invalid handle as completion, so
     * the request can outlive its stack frame. Permit only the verified XPP
     * setter, explicit wait/cancel wait and synchronous-submit wait sites. */
    {
        static const uint32_t usb_event_callers[] = {
            0x0035C348u, 0x0035D553u, 0x0035D56Cu, 0x0035C7F1u
        };
        if (xbox_kernel_set_inline_event_callers(usb_event_callers,
                sizeof(usb_event_callers) / sizeof(usb_event_callers[0])) != 0) {
            fprintf(stderr, "cannot register MM3 USB event bridges\n");
            xbox_MemoryLayoutShutdown();
            free(xbe_data);
            return 1;
        }
    }
    xbox_kernel_set_thunk_address(MM3_KERNEL_THUNK_ADDRESS,
                                   MM3_KERNEL_THUNK_COUNT);
    xbox_kernel_init();
    xbox_path_init(MM3_GAME_DIR, getenv("MM3_SAVE_DIR"));
    mm3_asset_cache_refresh();
    xbox_kernel_bridge_init();
    if (xbox_Nv2aMirrorFence(0x00351F48u, 0x2Cu, 0x30u) != 0 ||
        xbox_Nv2aCommandFence(0x00351F48u, 0xCu, 0u) != 0) {
        fprintf(stderr, "NV2A command fence registration failed\n");
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    /* D3D's GPU-notify KEVENT (device+0x196C, waited on in 0x344640),
     * raised by the NOP(5) it patches into the push buffer */
    xbox_Nv2aNotifyEvent(0x00351F48u, 0x196Cu, 5u);
    if (!xbox_HostHardwareInit()) {
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    if (!xbox_HostGraphicsInit("Midtown Madness 3 on PC improved and modded !!")) {
        xbox_HostHardwareShutdown();
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    /* Retail behavior is SPAWN. INLINE is a bounded single-thread diagnostic
     * that removes cross-thread trace interleaving while locating startup. */
    g_mm3_inline_threads = getenv("MM3_THREAD_MODE") &&
                          _stricmp(getenv("MM3_THREAD_MODE"), "inline") == 0;
    xbox_SetThreadMode(g_mm3_inline_threads ? XBOX_THREAD_MODE_INLINE : XBOX_THREAD_MODE_SPAWN);

    if (!recomp_dispatch_init())
        fprintf(stderr, "dispatch table allocation failed; using binary lookup\n");

    g_mm3_main_worker_ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!g_mm3_main_worker_ready) {
        fprintf(stderr, "cannot create MM3 worker monitor\n");
        xbox_HostHardwareShutdown();
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }

    /* Optional RECOMP_WATCHDOG_SECS diagnostic; must run on the guest thread. */
    xbox_WatchdogStart();
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
    if (!g_mm3_inline_threads) {
        WaitForSingleObject(g_mm3_main_worker_ready, INFINITE);
        WaitForSingleObject(g_mm3_main_worker, INFINITE);
        CloseHandle(g_mm3_main_worker);
    }
    CloseHandle(g_mm3_main_worker_ready);

    xbox_HostHardwareShutdown();
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(xbe_data);
    return 0;
}

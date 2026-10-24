#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "platform/xbox_winnt.h"

#include <xbox/xboxrecomp.h>

#include "recomp_types.h"
#include "recomp_funcs.h"
#include "crash_report.h"
#include "mods/teleport.h"
#include "mm3_asset_cache.h"
#include "nv2a_pb_recovery.h"
#include "mm3_performance_profile.h"
#include "mods/mods.h"
#include <xbox_host.h>
#include <xinput_xbox.h>
#include <video/fb_present.h>
#include <video/nv2a_d3d11.h>
#include <net/net.h>

#define MM3_XBE_PATH "game_files/default.xbe"
#define MM3_GAME_DIR "game_files"
#define MM3_KERNEL_THUNK_ADDRESS 0x00361F00u
#define MM3_KERNEL_THUNK_COUNT 151u
#define MM3_MEMORY_MAP_SIZE (128u * 1024u * 1024u)

/* Inherited MM3 guest identity is compatibility policy, not detected hardware.
 * Keep credential input in the port; the shared kernel copies export bytes. */
static void mm3_configure_kernel_identity(void)
{
    static char disk_model[]="XBOXRECOMP VIRTUAL HDD";
    static char disk_serial[]="XR0000000000";
    static char image_path[]="\\Device\\CdRom0\\default.xbe";
    xbox_HalDiskCachePartitionCount=3;
    xbox_HalDiskModelNumber=(XBOX_ANSI_STRING){sizeof(disk_model)-1,sizeof(disk_model),disk_model};
    xbox_HalDiskSerialNumber=(XBOX_ANSI_STRING){sizeof(disk_serial)-1,sizeof(disk_serial),disk_serial};
    xbox_XeImageFileName=(XBOX_ANSI_STRING){sizeof(image_path)-1,sizeof(image_path),image_path};
    static const struct { const char *name; UCHAR *bytes; } keys[] = {
        {"RECOMP_XBOX_HDKEY", xbox_HDKey},
        {"RECOMP_XBOX_SIGKEY", xbox_SignatureKey},
        {"RECOMP_XBOX_LANKEY", xbox_LANKey}
    };
    memset(&xbox_HardwareInfo,0,sizeof(xbox_HardwareInfo));
    xbox_HardwareInfo.GpuRevision=0xA1;
    xbox_HardwareInfo.McpRevision=0xB1;
    xbox_kernel_set_version(1,0,5849,0);
    for (unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);i++) {
        const char *text=getenv(keys[i].name);
        memset(keys[i].bytes,0,16);
        if (!text || !*text) continue;
        if (strlen(text)!=32 || strspn(text,"0123456789abcdefABCDEF")!=32) {
            fprintf(stderr,"ignoring invalid %s (expected 32 hex digits)\n",keys[i].name);
            continue;
        }
        for (unsigned n=0;n<16;n++) {
            unsigned byte;
            if (sscanf(text+2*n,"%2x",&byte)==1) keys[i].bytes[n]=(UCHAR)byte;
        }
    }
}

/* Inherited port compatibility: references expose one finished thread object,
 * not live per-handle KTHREAD state. Keep this approximation out of the runtime.
 * Reset its guest address before each host-owned memory generation. */
static uint32_t mm3_finished_thread_object;
static void mm3_reference_object_bridge(void)
{
    uint32_t handle=MEM32(g_esp),type=MEM32(g_esp+4),output=MEM32(g_esp+8);
    MEMORY_BASIC_INFORMATION region;
    uintptr_t pointer=(uintptr_t)output+g_xbox_mem_offset;
    if (output<0x1000 || (uint64_t)output+4>xbox_GetMappedSize() ||
        !VirtualQuery((void *)pointer,&region,sizeof(region)) ||
        region.State!=MEM_COMMIT || region.Protect!=PAGE_READWRITE ||
        pointer+4>(uintptr_t)region.BaseAddress+region.RegionSize) {
        g_eax=STATUS_INVALID_PARAMETER; return;
    }
    MEM32(output)=0;
    if (!handle || handle==UINT32_MAX) { g_eax=STATUS_INVALID_HANDLE; return; }
    if (type==XBOX_KERNEL_DATA_BASE+KDATA_THREAD_OBJ_TYPE) {
        if (!mm3_finished_thread_object) {
            mm3_finished_thread_object=xbox_HeapAlloc(0x124,16);
            if (mm3_finished_thread_object) {
                memset((void *)XBOX_PTR(mm3_finished_thread_object),0,0x124);
                MEM32(mm3_finished_thread_object+4)=1;
            }
        }
        MEM32(output)=mm3_finished_thread_object;
        g_eax=mm3_finished_thread_object?STATUS_SUCCESS:STATUS_NO_MEMORY;
    } else { MEM32(output)=handle; g_eax=STATUS_SUCCESS; }
}

/* Preserve the port's optional launch command-line interface. The identity
 * comes from this XBE's certificate, never from a different title's capture. */
static void mm3_configure_launch_data(const void *image, size_t size)
{
    static XBOX_LAUNCH_DATA_PAGE page;
    const char *cmdline = getenv("RECOMP_CMDLINE");
    uint32_t base, certificate, title_id;
    size_t offset, length;
    xbox_LaunchDataPage = NULL;
    if (!cmdline || !*cmdline || size < 0x11Cu) return;
    memcpy(&base, (const uint8_t *)image + 0x104, 4);
    memcpy(&certificate, (const uint8_t *)image + 0x118, 4);
    if (certificate < base) return;
    offset = (size_t)(certificate - base);
    if (offset > size || size - offset < 12) return;
    memcpy(&title_id, (const uint8_t *)image + offset + 8, 4);
    memset(&page, 0, sizeof(page));
    page.LaunchDataType = 3;
    page.TitleId = title_id;
    length = strlen(cmdline);
    if (length >= sizeof(page.LaunchData)) length = sizeof(page.LaunchData) - 1;
    memcpy(page.LaunchData, cmdline, length);
    xbox_LaunchDataPage = &page;
    fprintf(stderr, "[MM3_LAUNCH] title=0x%08X command-line bytes=%zu\n",
            (unsigned)title_id, length);
}

static HANDLE g_mm3_main_worker;
static HANDLE g_mm3_main_worker_ready;
static int g_mm3_inline_threads;
extern void sub_00083A6C_original(void);
extern void mm3_configure_diagnostics(void);
extern void mm3_host_video_init(void);

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
    /* Generic host, native and guest register lines come from the fork. */
    xbox_CrashReport(info);
    fprintf(stderr, "[CRASH] frame+8=0x%08X frame+c=0x%08X frame+10=0x%08X obj+18=0x%08X obj+580=0x%08X callback=0x%08X fs+20=0x%08X fs20+250=0x%08X\n",
            MEM32(g_seh_ebp + 8), MEM32(g_seh_ebp + 0x0C),
            MEM32(g_seh_ebp + 0x10), MEM32(MEM32(g_seh_ebp + 8) + 0x18),
            MEM32(MEM32(g_seh_ebp + 8) + 0x580),
            MEM32(0x00362018),
            MEM32(g_fs_base + 0x20), MEM32(MEM32(g_fs_base + 0x20) + 0x250));
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
    /* MM3 instances share read-only assets for system-link testing. */
    xbox_SetReadOnlyFileSharing(TRUE);
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

    /* Preserve MM3's inherited completion of authored partial mip chains.
     * The renderer's generic default retains guest levels. Configure before
     * the first texture upload; an explicit caller override takes priority. */
    if (!getenv("RECOMP_COMPLETE_MIPS") &&
        _putenv_s("RECOMP_COMPLETE_MIPS", "1") != 0) {
        fprintf(stderr, "cannot configure MM3 mip completion\n");
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

    /* MM3 keeps its inherited export-page address as explicit port policy. */
    if (!xbox_ConfigureKernelData(0x00740000u)) {
        fprintf(stderr, "cannot configure MM3 kernel data\n");
        free(xbe_data);
        return 1;
    }
    /* Preserve MM3's inherited worker headroom as an explicit minimum. */
    if (!xbox_ConfigureThreadStack(512u*1024u)) {
        fprintf(stderr,"cannot configure MM3 worker stack minimum\n");
        free(xbe_data); return 1;
    }
    /* MM3 retains its inherited main-stack placement/budget as port policy.
     * The shared runtime validates it and keeps callback slices separate. */
    if (!xbox_ConfigureMainStack(0x00780000u, 8u * 1024u * 1024u)) {
        fprintf(stderr, "cannot configure MM3 main stack\n");
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
    /* Preserve the inherited application scratch chain in the port, not the
     * generic loader. Its MM3 usage still needs a title-specific audit. */
    *(uint32_t *)((uintptr_t)g_xbox_mem_offset + XBOX_FS_BASE + 0x28u) = 0x00760000u;
    *(uint32_t *)((uintptr_t)g_xbox_mem_offset + 0x00760028u) = 0x00700000u;

    /* Preserve the inherited empty-section PE probe response in MM3 only. */
    void *kernel_probe = xbox_GetKernelMemoryBase();
    if (kernel_probe)
        *(uint32_t *)((uint8_t *)kernel_probe + 0x3Cu) = 0x80u;

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
    mm3_configure_kernel_identity();
    mm3_configure_launch_data(xbe_data, xbe_size);
    xbox_kernel_init();
    xbox_path_init(MM3_GAME_DIR, getenv("MM3_SAVE_DIR"));
    mm3_asset_cache_refresh();
    mm3_finished_thread_object=0;
    if (xbox_kernel_set_bridge_override(246,mm3_reference_object_bridge)!=0) {
        fprintf(stderr,"cannot register MM3 object-reference bridge\n");
        xbox_kernel_shutdown(); xbox_MemoryLayoutShutdown(); free(xbe_data); return 1;
    }
    xbox_kernel_bridge_init();
    nv2a_pb_set_stop_handler(nv2a_pb_recover_semaphore_stop);
    if (xbox_Nv2aMirrorFence(0x00351F48u, 0x2Cu, 0x30u) != 0 ||
        xbox_Nv2aCommandFence(0x00351F48u, 0xCu, 0u) != 0) {
        fprintf(stderr, "NV2A command fence registration failed\n");
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    /* MM3's statically linked D3D device/channel layout, for diagnostics only. */
    const XBOX_NV2A_CAPTURE_LAYOUT capture_layout = {
        0x17C0u, 0x40u, 0x44u, 0x4F8u, 0x24u, 0x28u
    };
    if (xbox_Nv2aCaptureLayout(0x00351F48u, &capture_layout) != 0) {
        fprintf(stderr, "NV2A capture layout registration failed\n");
        xbox_kernel_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    /* D3D's GPU-notify KEVENT (device+0x196C, waited on in 0x344640),
     * raised by the NOP(5) it patches into the push buffer */
    xbox_Nv2aNotifyEvent(0x00351F48u, 0x196Cu, 5u);
    mm3_host_video_init();
    /* Preserve MM3's existing online-service routing outside the runtime. */
    static const XboxDnsAlias service_dns[] = {
        {"macs.xboxlive.com", "macs.insig.uk"},
        {"as.xboxlive.com", "as.insig.uk"},
        {"tgs.xboxlive.com", "tgs.insig.uk"},
        {"xds.xboxlive.com", "xexds.xboxlive.com"},
    };
    net_backend_set_dns_aliases(service_dns, sizeof service_dns / sizeof service_dns[0]);
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

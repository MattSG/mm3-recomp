/* Graphics/clock boundaries are mocked; production control flow is extracted
 * by mm3_present_pacing_contract.py rather than copied into this test. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int UINT;
typedef uint32_t DWORD;
typedef int32_t HRESULT;
typedef struct LARGE_INTEGER { int64_t QuadPart; } LARGE_INTEGER;
typedef int ID3D11Texture2D, ID3D11RenderTargetView, ID3D11Resource;
typedef struct Surface { UINT iw, ih; uint32_t gen; int cpu_present_pending; } Surface;
typedef struct PresentConsts { int unused; } PresentConsts;
typedef struct XboxOverlay { int unused; } XboxOverlay;
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#define FAILED(hr) ((HRESULT)(hr) < 0)
#define S_OK ((HRESULT)0)
#define DXGI_STATUS_OCCLUDED ((HRESULT)0x087A0001)
#define DXGI_FORMAT_B8G8R8A8_UNORM 0
#define D3D11_BIND_RENDER_TARGET 0
#define RECOMP_PROFILE_BEGIN(name) ((void)0)
#define RECOMP_PROFILE_END() ((void)0)
#define RECOMP_PROFILE_FRAME() ((void)0)
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s (events=%s)\n", __LINE__, #x, events); exit(1); } } while (0)

enum { PROF_PACE, PROF_WRITEBACK, PROF_PRESENT };
static int s_prof_on, s_init_state, s_frame_bg_tex, s_frame_tag_window, s_wb_pending;
static int64_t s_prof[3];
static Surface *s_shown;
static DWORD s_last_present, s_last_flip;
static unsigned s_frame;
static FILE *s_frame_csv_file;
static LARGE_INTEGER s_frame_csv_freq = {1000};
static int64_t s_frame_csv_present, s_frame_csv_present_prev;
static int64_t s_frame_csv_present_interval;
static ID3D11Texture2D *s_cap_tex;
static ID3D11RenderTargetView *s_cap_rtv;
static UINT s_cap_w, s_cap_h, s_swap_w = 640, s_swap_h = 480;
static int s_swap, s_dev, IID_ID3D11Texture2D;

static Surface surface = {640, 480, 0, 0};
static int native_texture, native_view;
static char events[64];
static size_t event_count;
static unsigned pace_calls, present_calls, csv_calls, scans, texture_releases, view_releases;
static unsigned cpu_syncs, cpu_uploads;
static int cpu_write;
static int64_t clock_ticks, csv_ticks;
static DWORD test_now;
static int init_ok, surface_ok, scan_ok, swap_ok, buffer_ok, view_ok, overlay_ok;
static int cpu_scanout, capture_surface, after_policy;
static HRESULT present_result;

static void event(char value)
{
    CHECK(event_count + 1 < sizeof(events));
    events[event_count++] = value;
    events[event_count] = 0;
}

static char *test_getenv(const char *name)
{
    if (strcmp(name, "RECOMP_PACE_AFTER_PRESENT") == 0) return after_policy ? "1" : "0";
    if (strcmp(name, "RECOMP_CAPTURE_SURFACE") == 0) return capture_surface ? "1" : NULL;
    if (strcmp(name, "RECOMP_VSYNC") == 0) return "0";
    return NULL;
}

static int64_t qpc(void) { return clock_ticks; }
static int64_t hitch_trace_begin(void) { return 0; }
static void hitch_trace_report(const char *phase, int64_t begin)
{ (void)phase; CHECK(begin == 0); }
static DWORD GetTickCount(void) { return test_now; }
static void pace_vblank(void) { event('W'); pace_calls++; clock_ticks += 7; }
static int nv2a_d3d_init(void) { return init_ok; }
static uint32_t phys(uint32_t address) { return address; }
static Surface *surface_find(uint32_t address) { (void)address; return surface_ok ? &surface : NULL; }
static Surface *scan_surface(uint32_t address, uint32_t pitch)
{ (void)address; (void)pitch; scans++; return scan_ok ? &surface : NULL; }
static void surface_sync(Surface *image, int force)
{
    CHECK(image == &surface && force == 1);
    cpu_syncs++;
    if (cpu_write) { image->gen++; cpu_uploads++; cpu_write = 0; }
}
static void writeback_surfaces(void) { event('B'); clock_ticks += 2; }
static int ensure_swapchain(void) { return swap_ok; }
static void prepare_overlay(PresentConsts *pc) { (void)pc; }
static ID3D11Texture2D *make_texture(UINT w, UINT h, int a, int b, int format, int flags, int misc)
{ (void)w; (void)h; (void)a; (void)b; (void)format; (void)flags; (void)misc; return &native_texture; }
static HRESULT ID3D11Device_CreateRenderTargetView(int device, ID3D11Resource *texture, void *description,
                                                  ID3D11RenderTargetView **view)
{
    (void)device; (void)texture; (void)description;
    *view = view_ok ? &native_view : NULL;
    return view_ok ? 0 : -1;
}
static HRESULT IDXGISwapChain1_GetBuffer(int swap, int index, const int *iid, void **buffer)
{ (void)swap; (void)index; (void)iid; *buffer = &native_texture; return buffer_ok ? 0 : -1; }
static HRESULT IDXGISwapChain1_Present(int swap, UINT vsync, int flags)
{ (void)swap; CHECK(vsync == 0 && flags == 0); event('P'); present_calls++; clock_ticks += 3; return present_result; }
static void ID3D11RenderTargetView_Release(ID3D11RenderTargetView *view)
{ CHECK(view != NULL); view_releases++; }
static void ID3D11Texture2D_Release(ID3D11Texture2D *texture)
{ CHECK(texture != NULL); texture_releases++; }
static void composite(ID3D11RenderTargetView *view, UINT w, UINT h, Surface *image, PresentConsts *pc, int window)
{ CHECK(view != NULL); (void)w; (void)h; (void)image; (void)pc; (void)window; event('C'); clock_ticks += 4; }
static void capture_backbuffer(ID3D11Texture2D *texture) { (void)texture; event('K'); clock_ticks++; }
static void frame_csv_init(void) {}
static void contract_frame_csv_present(int64_t timestamp, HRESULT result);
static void contract_frame_csv(int64_t ticks);
static void frame_csv_present(int64_t timestamp, HRESULT result)
{
    CHECK(timestamp == clock_ticks && result == present_result);
    event('R');
    contract_frame_csv_present(timestamp, result);
}
static void frame_csv(int64_t ticks)
{ event('F'); csv_calls++; csv_ticks = ticks; contract_frame_csv(ticks); }
static void prof_report(void) {}
static void display_config(int immediate) { CHECK(immediate == 0); }
static void rdoc_poll(void) {}
static int xbox_FramebufferScanSource(uint32_t *address, uint32_t *pitch)
{ *address = 0x10000; *pitch = 2560; return cpu_scanout; }
static int xbox_FramebufferOverlay(XboxOverlay *overlay) { (void)overlay; return overlay_ok; }

#define getenv test_getenv
#include "production_pacing.inc"
#undef getenv

static void reset(void)
{
    memset(events, 0, sizeof(events)); event_count = 0;
    memset(s_prof, 0, sizeof(s_prof)); s_prof_on = 1;
    pace_calls = present_calls = csv_calls = scans = s_frame = 0;
    texture_releases = view_releases = 0;
    cpu_syncs = cpu_uploads = 0; cpu_write = s_wb_pending = 0;
    surface.gen = 1; surface.cpu_present_pending = 0;
    clock_ticks = 1000; csv_ticks = -1; test_now = 10000;
    init_ok = surface_ok = scan_ok = swap_ok = buffer_ok = view_ok = overlay_ok = 1;
    cpu_scanout = capture_surface = 0; present_result = 0;
    s_init_state = 1; s_last_present = s_last_flip = 0;
    s_shown = &surface;
    if (s_frame_csv_file) fclose(s_frame_csv_file);
    s_frame_csv_file = tmpfile(); CHECK(s_frame_csv_file != NULL);
    s_frame_csv_present = s_frame_csv_present_prev = s_frame_csv_present_interval = 0;
    s_cap_tex = NULL; s_cap_rtv = NULL; s_cap_w = s_cap_h = 0;
}

static void successful_flip(const char *expected)
{
    nv2a_d3d_flip(0x10000, 2560);
    CHECK(strcmp(events, expected) == 0);
    CHECK(pace_calls == 1 && present_calls == 1 && csv_calls == 1 && csv_ticks == 7);
    CHECK(s_prof[PROF_PACE] == 7 && s_prof[PROF_WRITEBACK] == 2);
    CHECK(s_frame == 1 && s_frame_tag_window == 1 && s_frame_bg_tex == 0);
}

static void flip_cases(void)
{
    const char *normal = after_policy ? "BCKPRWF" : "BCKWPRF";
    reset(); successful_flip(normal); CHECK(s_prof[PROF_PRESENT] == 8);
    reset(); present_result = -1; successful_flip(normal); CHECK(s_prof[PROF_PRESENT] == 8);
    reset(); capture_surface = 1;
    successful_flip(after_policy ? "BCKCPRWF" : "BCKCWPRF");
    CHECK(s_prof[PROF_PRESENT] == 12);

    reset(); surface_ok = scan_ok = 0;
    nv2a_d3d_flip(0x10000, 0);
    CHECK(strcmp(events, "BWF") == 0 && pace_calls == 1 && present_calls == 0 && csv_ticks == 7);
    CHECK(scans == 0 && s_frame == 1);
    reset(); surface_ok = scan_ok = 0;
    nv2a_d3d_flip(0x10000, 2560);
    CHECK(strcmp(events, "BWF") == 0 && pace_calls == 1 && scans == 1);
    reset(); swap_ok = 0;
    nv2a_d3d_flip(0x10000, 2560);
    CHECK(strcmp(events, "BWF") == 0 && pace_calls == 1 && present_calls == 0);
    reset(); buffer_ok = 0;
    nv2a_d3d_flip(0x10000, 2560);
    CHECK(strcmp(events, "BWF") == 0 && pace_calls == 1 && present_calls == 0);
    reset(); view_ok = 0;
    nv2a_d3d_flip(0x10000, 2560);
    CHECK(strcmp(events, "BWF") == 0 && pace_calls == 1 && present_calls == 0 && csv_calls == 1);
    CHECK(texture_releases == 1 && view_releases == 0);
    CHECK(s_prof[PROF_PRESENT] == 0 && s_prof[PROF_PACE] == 7);

    reset(); s_prof_on = 0;
    nv2a_d3d_flip(0x10000, 2560);
    CHECK(strcmp(events, normal) == 0 && pace_calls == 1 && csv_ticks == 7);
    CHECK(s_prof[PROF_PACE] == 0 && s_prof[PROF_PRESENT] == 0);
    reset(); nv2a_d3d_flip(0x10000, 2560); nv2a_d3d_flip(0x10000, 2560);
    CHECK(pace_calls == 2 && present_calls == 2 && csv_calls == 2 && s_frame == 2);
    CHECK(s_prof[PROF_PACE] == 14 && s_prof[PROF_PRESENT] == 16);
    reset(); init_ok = 0;
    nv2a_d3d_flip(0x10000, 2560);
    CHECK(!events[0] && !pace_calls && !present_calls && !s_frame);
}

static void tick_cases(void)
{
    reset(); cpu_scanout = 1;
    nv2a_d3d_tick();
    CHECK(strcmp(events, "CKP") == 0 && present_calls == 1 && s_frame == 1 && scans == 1);
    CHECK(!pace_calls && !csv_calls && s_prof[PROF_PACE] == 0);
    CHECK(!s_frame_csv_present && !s_frame_csv_present_prev);
    reset(); nv2a_d3d_tick();
    CHECK(strcmp(events, "CKP") == 0 && present_calls == 1 && !s_frame);
    CHECK(!pace_calls && !csv_calls && s_prof[PROF_PACE] == 0);
    CHECK(!s_frame_csv_present && !s_frame_csv_present_prev);
    reset(); overlay_ok = 0;
    nv2a_d3d_tick(); CHECK(!events[0] && !pace_calls && s_last_present == test_now);
    reset(); s_last_present = test_now - 15;
    nv2a_d3d_tick(); CHECK(!events[0] && !pace_calls);
    reset(); s_last_flip = test_now - 50;
    nv2a_d3d_tick(); CHECK(!events[0] && !pace_calls);
    reset(); init_ok = 0; s_init_state = 0;
    nv2a_d3d_tick(); CHECK(!events[0] && !pace_calls);
}

static void idle_cpu_cases(void)
{
    int failure;
    reset(); overlay_ok = 0; cpu_write = 1;
    nv2a_d3d_tick();
    CHECK(cpu_syncs == 1 && cpu_uploads == 1 && present_calls == 1);
    CHECK(!surface.cpu_present_pending && strcmp(events, "CKP") == 0);
    CHECK(!s_frame && !pace_calls && !csv_calls && !scans);
    CHECK(!s_frame_csv_present && !s_frame_csv_present_prev);
    CHECK(s_prof[PROF_PACE] == 0);

    /* Same submission epoch, same surface: a later CPU animation frame is
     * checked again; an unchanged frame produces neither upload nor Present. */
    test_now += 16; nv2a_d3d_tick();
    CHECK(cpu_syncs == 2 && cpu_uploads == 1 && present_calls == 1);
    cpu_write = 1; test_now += 16; nv2a_d3d_tick();
    CHECK(cpu_syncs == 3 && cpu_uploads == 2 && present_calls == 2);
    CHECK(!surface.cpu_present_pending && !pace_calls && !csv_calls && !s_frame);

    /* Uploading the guest bytes must wait until queued GPU writeback has
     * finished using the old shadow to protect those very CPU writes. */
    reset(); overlay_ok = 0; cpu_write = s_wb_pending = 1;
    nv2a_d3d_tick();
    CHECK(!cpu_syncs && !cpu_uploads && !present_calls && cpu_write);
    s_wb_pending = 0; test_now += 16; nv2a_d3d_tick();
    CHECK(cpu_syncs == 1 && cpu_uploads == 1 && present_calls == 1);
    CHECK(!surface.cpu_present_pending && !pace_calls && !csv_calls);

    reset(); overlay_ok = 0; cpu_write = 1; s_last_flip = test_now - 50;
    nv2a_d3d_tick(); CHECK(!cpu_syncs && !cpu_uploads && !present_calls && cpu_write);
    test_now++; nv2a_d3d_tick();
    CHECK(cpu_syncs == 1 && cpu_uploads == 1 && present_calls == 1);
    reset(); overlay_ok = 0; cpu_write = 1; s_last_present = test_now - 15;
    nv2a_d3d_tick(); CHECK(!cpu_syncs && !cpu_uploads && !present_calls && cpu_write);

    /* An upload is not proof that the image reached the window. Retry missing
     * swap-chain/resources, failed Present and occlusion without another write. */
    for (failure = 0; failure < 5; failure++) {
        unsigned failed_attempts;
        reset(); overlay_ok = 0; cpu_write = 1;
        if (failure == 0) swap_ok = 0;
        if (failure == 1) buffer_ok = 0;
        if (failure == 2) view_ok = 0;
        if (failure == 3) present_result = -1;
        if (failure == 4) present_result = DXGI_STATUS_OCCLUDED;
        nv2a_d3d_tick();
        CHECK(cpu_syncs == 1 && cpu_uploads == 1 && surface.cpu_present_pending);
        failed_attempts = present_calls;
        CHECK(failed_attempts == (failure >= 3 ? 1u : 0u));
        swap_ok = buffer_ok = view_ok = 1; present_result = S_OK;
        test_now += 16; nv2a_d3d_tick();
        CHECK(cpu_syncs == 2 && cpu_uploads == 1);
        CHECK(present_calls == failed_attempts + 1 && !surface.cpu_present_pending);
        test_now += 16; nv2a_d3d_tick();
        CHECK(cpu_uploads == 1 && present_calls == failed_attempts + 1);
        CHECK(!pace_calls && !csv_calls && !s_frame && !scans);
        CHECK(!s_frame_csv_present && !s_frame_csv_present_prev);
        CHECK(s_prof[PROF_PACE] == 0);
    }
}

static void csv_cases(void)
{
    int failure;
    for (failure = 0; failure < 4; failure++) {
        int64_t previous_guest;
        char row[256], last[256] = {0};
        reset();
        nv2a_d3d_flip(0x10000, 2560);
        previous_guest = s_frame_csv_present_prev;
        CHECK(previous_guest > 0 && !s_frame_csv_present);

        /* A successful unpaced movie/scanout must neither become the pending
         * guest timestamp nor replace the preceding successful guest flip. */
        clock_ticks += 100; test_now += 100; cpu_scanout = 1;
        nv2a_d3d_tick();
        CHECK(present_calls == 2 && csv_calls == 1 && pace_calls == 1);
        CHECK(!s_frame_csv_present && s_frame_csv_present_prev == previous_guest);

        if (failure == 0) surface_ok = scan_ok = 0;
        if (failure == 1) buffer_ok = 0;
        if (failure == 2) view_ok = 0;
        if (failure == 3) present_result = -1;
        nv2a_d3d_flip(0x10000, 2560);
        CHECK(csv_calls == 2 && pace_calls == 2 && !s_frame_csv_present);
        CHECK(s_frame_csv_present_prev == previous_guest);
        rewind(s_frame_csv_file);
        while (fgets(row, sizeof(row), s_frame_csv_file)) strcpy(last, row);
        CHECK(strlen(last) >= 3 && strcmp(last + strlen(last) - 3, ",,\n") == 0);

        /* Recovery computes its interval from the last successful guest
         * submission, skipping both the movie tick and failed guest flip. */
        surface_ok = scan_ok = buffer_ok = view_ok = 1; present_result = 0;
        CHECK(fseek(s_frame_csv_file, 0, SEEK_END) == 0);
        nv2a_d3d_flip(0x10000, 2560);
        CHECK(s_frame_csv_present_prev > previous_guest && !s_frame_csv_present);
        CHECK(s_frame_csv_present_interval == s_frame_csv_present_prev - previous_guest);
    }
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    after_policy = strcmp(argv[1], "after") == 0;
    flip_cases(); tick_cases(); idle_cpu_cases(); csv_cases();
    fclose(s_frame_csv_file);
    printf("PASS: %s-present pacing; successful/failed/captured flips, missing resources/RTV failure, once-per-flip fallback, unpaced movie/idle CPU ticks, writeback ordering, failed/occluded display retries, guest-only CSV state and profiler accounting\n",
           after_policy ? "after" : "before");
    return 0;
}

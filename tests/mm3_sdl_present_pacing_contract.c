/* Compile real SDL present/tick bodies with deterministic GPU mocks. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef struct SDL_GPUTexture { int unused; } SDL_GPUTexture;
typedef struct SDL_Window { int unused; } SDL_Window;
typedef struct SDL_GPUCommandBuffer { int unused; } SDL_GPUCommandBuffer;
typedef struct SDL_GPUBlitInfo {
    struct { SDL_GPUTexture *texture; Uint32 w, h; } source, destination;
    int load_op, filter;
} SDL_GPUBlitInfo;
typedef struct Surface { uint32_t gen; int cpu_present_pending; } Surface;
typedef struct XboxOverlay { int unused; } XboxOverlay;

#define SDL_GPU_LOADOP_DONT_CARE 0
#define SDL_GPU_FILTER_LINEAR 0
#define RECOMP_PROFILE_BEGIN(name) ((void)0)
#define RECOMP_PROFILE_END() ((void)0)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s (events=%s)\n",__LINE__,#x,events); exit(1); } } while (0)

enum { PROF_PACE };
static int s_prof_on;
static int64_t s_prof[1];
static char events[64];
static size_t event_count;
static unsigned pace_calls, submit_calls;
static int after_policy, s_init_state = 1;
static int64_t clock_ticks;
static uint64_t test_now = 100;
static SDL_GPUTexture kept_texture, swap_texture;
static SDL_Window window;
static SDL_GPUCommandBuffer command;
static SDL_Window *s_window = &window;
static SDL_GPUCommandBuffer *s_cmd = &command;
static int s_dev;
static SDL_GPUTexture *s_kept = &kept_texture;
static uint32_t s_kept_w = 640, s_kept_h = 480;
static uint64_t s_last_present;
static FILE *s_frame_csv_file;
static int s_repeat, s_interp_req;
static float s_interp_t, s_loading;
static unsigned s_frame;
static uint64_t s_last_flip;
static Surface shown_surface = {0, 0};
static Surface *s_shown = &shown_surface;
static int s_wb_pending;

static void event(char c) { events[event_count++] = c; events[event_count] = 0; }
static char *test_getenv(const char *name)
{ return strcmp(name,"RECOMP_PACE_AFTER_PRESENT")==0 && after_policy ? "1" : NULL; }
static int nv2a_d3d_init(void) { return 1; }
static void fail(const char *name) { (void)name; }
static void fbw_sdl_pump(void) {}
static int SDL_GetAtomicInt(int *p) { return *p; }
static void SDL_SetAtomicInt(int *p,int v) { *p=v; }
static int64_t qpc(void) { return clock_ticks; }
static Uint64 SDL_GetTicks(void) { return test_now; }
static int SDL_WaitAndAcquireGPUSwapchainTexture(SDL_GPUCommandBuffer *c, SDL_Window *w,
                                                  SDL_GPUTexture **t, Uint32 *x, Uint32 *y)
{ CHECK(c==&command && w==&window); *t=&swap_texture; *x=640; *y=480; event('A'); return 1; }
static void copy_end(void) {}
static void flush_batch(void) {}
static SDL_GPUCommandBuffer *cmd(void) { return &command; }
static void SDL_BlitGPUTexture(SDL_GPUCommandBuffer *c,const SDL_GPUBlitInfo *b)
{ CHECK(c==&command && b->source.texture==s_kept); event('B'); }
static void xbox_FramebufferRenderHostOverlay(int d, SDL_GPUCommandBuffer *c, SDL_GPUTexture *t)
{ (void)d; CHECK(c==&command && t==&swap_texture); event('O'); }
static void frame_csv_init(void) {}
static void frame_csv_present(int64_t timestamp) { (void)timestamp; }
static void submit(void *f) { (void)f; submit_calls++; event('S'); }
static void pace_vblank(void) { pace_calls++; event('W'); clock_ticks+=7; }
static void frame_csv(int64_t ticks) { CHECK(ticks==7); event('F'); }
static void present_surface(Surface *s,void *p) { (void)s; (void)p; event('X'); }
static int nv2a_pb_replay(float t) { (void)t; return 1; }
static int xbox_FramebufferOverlay(XboxOverlay *o) { (void)o; return 0; }
static int xbox_FramebufferScanSource(uint32_t *v,uint32_t *p) { (void)v;(void)p; return 0; }
static Surface *scan_surface(uint32_t v,uint32_t p) { (void)v;(void)p; return NULL; }
static void surface_sync(Surface *s,int f) { (void)s;(void)f; }

#define getenv test_getenv
#include "production_pacing.inc"
#undef getenv

static void reset(void)
{
    memset(events,0,sizeof events); event_count=0; pace_calls=submit_calls=0;
    clock_ticks=1000; test_now=100; s_last_present=0; s_frame=0;
    s_repeat=s_interp_req=0; s_wb_pending=0; s_init_state=1;
}
static void real_flip(void)
{
    FlipPacing pacing={0,0};
    CHECK(present_kept_image(&pacing));
    pace_flip(&pacing);
}
int main(int argc,char **argv)
{
    CHECK(argc==2); after_policy=strcmp(argv[1],"after")==0;
    reset(); real_flip();
    CHECK(strcmp(events,after_policy?"ABOSW":"ABOWS")==0);
    CHECK(pace_calls==1 && submit_calls==1);
    reset(); s_repeat=1; nv2a_d3d_tick();
    CHECK(strcmp(events,after_policy?"ABOSWF":"ABOWSF")==0);
    CHECK(pace_calls==1 && submit_calls==1 && !s_repeat);
    puts("PASS: real flip and repeat submit pacing order");
    return 0;
}

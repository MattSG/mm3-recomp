#include <math.h>
#include <stdio.h>
#include <string.h>
#include "render_fog.h"
#if defined(XBOXRECOMP_SDL3)
#include "render_fog_sdlgpu.h"
#elif defined(_WIN32)
#include "render_fog_d3d11.h"
#else
static int mm3_fog_draw(const NvHostTarget *target,const float params[12],int mask)
{ (void)target; (void)params; (void)mask; return 0; }
#endif

typedef struct { float scale,start,end; uint32_t argb,flags; const float *sky; } FogArgs;
static int fog_target(const NvHostTarget *target,void *user)
{
    const FogArgs *a = user;
    float params[12] = {a->start,1.f/(a->end-a->start),
        a->scale*(target->depth_bits == 16 ? 65536.f : 16777216.f),
        (a->flags & MM3_FOG_SKY_FILL) ? 1.f : 0.f,
        ((a->argb>>16)&255)/255.f,((a->argb>>8)&255)/255.f,(a->argb&255)/255.f,0};
    if (a->flags & MM3_FOG_SKY_FADE) {
        params[8] = 2.f*a->sky[0]/target->width;
        params[9] = -2.f*a->sky[1]/target->height;
        params[10] = a->sky[2]-a->sky[0]+a->sky[1]; params[11] = a->sky[3];
    }
    if (!isfinite(params[1]) || !isfinite(params[2]) || !isfinite(params[8]) ||
        !isfinite(params[9]) || !isfinite(params[10])) return 0;
    int drawn = mm3_fog_draw(target,params,(a->flags & MM3_FOG_SKY_MASK) != 0);
    static unsigned logged;
    if (drawn && !logged++) fprintf(stderr,"[MM3_FOG] pass %ux%u depth-bits=%u samples=%u\n",
        target->width,target->height,target->depth_bits,target->samples);
    return drawn;
}
void mm3_render_fog(const NvD3DState *state,float depth_to_world,float far_clip,
                    float start,float end,uint32_t argb,uint32_t flags,const float sky[4])
{
    if ((flags & MM3_FOG_SKY_FADE) && (!sky || !isfinite(sky[0]) || !isfinite(sky[1]) ||
        !isfinite(sky[2]) || !isfinite(sky[3]) || sky[3] <= 0)) return;
    if (!isfinite(depth_to_world) || !isfinite(far_clip) || !isfinite(start) || !isfinite(end) ||
        depth_to_world <= 0 || far_clip <= 0 || start < 0 || end <= start || end > far_clip || !state) return;
    FogArgs args = {depth_to_world,start,end,argb,flags,sky};
    nv2a_d3d_host_draw(state,fog_target,&args);
}

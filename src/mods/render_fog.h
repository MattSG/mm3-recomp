#ifndef MM3_RENDER_FOG_H
#define MM3_RENDER_FOG_H
#include "nv2a_d3d11.h"
#define MM3_FOG_SKY_MASK 1u
#define MM3_FOG_SKY_FILL 2u
#define MM3_FOG_SKY_FADE 4u
void mm3_render_fog(const NvD3DState *state,float depth_to_world,float far_clip,
                    float start,float end,uint32_t argb,uint32_t flags,const float sky_fade[4]);
#endif

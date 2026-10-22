#ifndef MM3_RENDER_POLICY_H
#define MM3_RENDER_POLICY_H
#include "nv2a_d3d11.h"
int mm3_render_force_depth(const NvD3DState *, uint32_t source_width, uint32_t source_height);
int mm3_render_cubic_filter(const NvD3DState *, int source_is_target, uint32_t levels);
int mm3_render_idle(uint32_t now_ms, uint32_t idle_ms, int movie);
void mm3_render_vertex_alignment(uint32_t width, uint32_t height, float *snap, float *offset);
float mm3_loading_phase(void);
#endif

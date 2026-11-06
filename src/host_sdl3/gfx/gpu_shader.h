/**
 * Run-time HLSL -> SDL_GPU shader compilation (DXC + SPIRV-Cross through
 * SDL_shadercross). The Xbox shaders are generated as text while the game
 * runs, so this cannot be a build step.
 *
 * Resource spaces follow SDL_GPU's SPIR-V convention, and the HLSL must
 * declare them: vertex uniform buffers register(bN, space1); fragment
 * textures/samplers register(tN/sN, space2) and uniform buffers
 * register(bN, space3). Vertex textures use space0.
 */
#ifndef XBOX_GPU_SHADER_H
#define XBOX_GPU_SHADER_H

#include <SDL3/SDL_gpu.h>

/* Returns NULL on failure (the error is in SDL_GetError / stderr). */
SDL_GPUShader *xgpu_compile_hlsl(SDL_GPUDevice *device, const char *hlsl,
                                 const char *entry, int is_fragment,
                                 const char *name);

/* The same, also reporting how many samplers and uniform buffers the shader
 * declares: SDL_GPU wants exactly those bound, no more and no fewer. */
typedef struct XGpuShader {
    SDL_GPUShader *shader;
    Uint32         num_samplers;
    Uint32         num_uniform_buffers;
} XGpuShader;

int xgpu_compile_hlsl_ex(SDL_GPUDevice *device, const char *hlsl, const char *entry,
                         int is_fragment, const char *name, XGpuShader *out);
void xgpu_release_shader(SDL_GPUDevice *device, XGpuShader *s);

#endif

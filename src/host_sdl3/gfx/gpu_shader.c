#include "gpu_shader.h"

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <stdio.h>
#include <string.h>

static SDL_GPUShader *compile(SDL_GPUDevice *device, const char *hlsl,
                              const char *entry, int is_fragment,
                              const char *name, XGpuShader *out)
{
    static int inited;
    SDL_ShaderCross_ShaderStage stage = is_fragment
        ? SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT : SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
    SDL_ShaderCross_HLSL_Info hi;
    SDL_ShaderCross_SPIRV_Info si;
    SDL_ShaderCross_GraphicsShaderMetadata *meta;
    SDL_GPUShader *shader;
    size_t size = 0;
    void *spirv;

    if (!inited) {
        if (!SDL_ShaderCross_Init()) {
            fprintf(stderr, "[gpu] shadercross init failed: %s\n", SDL_GetError());
            return NULL;
        }
        inited = 1;
    }

    SDL_zero(hi);
    hi.source = hlsl;
    hi.entrypoint = entry;
    hi.shader_stage = stage;
    spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hi, &size);
    if (!spirv) {
        fprintf(stderr, "[gpu] HLSL compile failed (%s): %s\n",
                name ? name : entry, SDL_GetError());
        return NULL;
    }

    meta = SDL_ShaderCross_ReflectGraphicsSPIRV(spirv, size, 0);
    if (!meta) {
        fprintf(stderr, "[gpu] reflect failed (%s): %s\n",
                name ? name : entry, SDL_GetError());
        SDL_free(spirv);
        return NULL;
    }

    SDL_zero(si);
    si.bytecode = spirv;
    si.bytecode_size = size;
    si.entrypoint = entry;
    si.shader_stage = stage;
    shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device, &si, &meta->resource_info, 0);
    if (shader && out) {
        out->shader = shader;
        out->num_samplers = meta->resource_info.num_samplers;
        out->num_uniform_buffers = meta->resource_info.num_uniform_buffers;
    }
    if (!shader)
        fprintf(stderr, "[gpu] shader create failed (%s): %s\n",
                name ? name : entry, SDL_GetError());
    SDL_free(meta);
    SDL_free(spirv);
    return shader;
}

SDL_GPUShader *xgpu_compile_hlsl(SDL_GPUDevice *device, const char *hlsl,
                                 const char *entry, int is_fragment,
                                 const char *name)
{
    return compile(device, hlsl, entry, is_fragment, name, NULL);
}

int xgpu_compile_hlsl_ex(SDL_GPUDevice *device, const char *hlsl, const char *entry,
                         int is_fragment, const char *name, XGpuShader *out)
{
    memset(out, 0, sizeof(*out));
    return compile(device, hlsl, entry, is_fragment, name, out) != NULL;
}

void xgpu_release_shader(SDL_GPUDevice *device, XGpuShader *s)
{
    if (s && s->shader)
        SDL_ReleaseGPUShader(device, s->shader);
    if (s)
        memset(s, 0, sizeof(*s));
}

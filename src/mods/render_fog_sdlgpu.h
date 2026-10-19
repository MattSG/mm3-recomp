#include <SDL3/SDL.h>
#include "mm3_fog_vs.spirv.h"
#include "mm3_fog_ps.spirv.h"
#ifdef _WIN32
#include "mm3_fog_vs.dxil.h"
#include "mm3_fog_ps.dxil.h"
#endif
static SDL_GPUShader *fog_vs, *fog_ps;
static SDL_GPUGraphicsPipeline *fog_pipe[2];
static SDL_GPUSampler *fog_sampler;
static SDL_GPUShader *fog_shader(SDL_GPUDevice *device,int fragment)
{
    SDL_GPUShaderCreateInfo info = {0};
    info.entrypoint = fragment ? "fog_ps" : "fog_vs";
    info.stage = fragment ? SDL_GPU_SHADERSTAGE_FRAGMENT : SDL_GPU_SHADERSTAGE_VERTEX;
    info.num_samplers = info.num_uniform_buffers = fragment ? 1 : 0;
#ifdef _WIN32
    if (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = fragment ? mm3_fog_ps_dxil : mm3_fog_vs_dxil;
        info.code_size = fragment ? sizeof mm3_fog_ps_dxil : sizeof mm3_fog_vs_dxil;
    } else
#endif
    {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = fragment ? mm3_fog_ps_spirv : mm3_fog_vs_spirv;
        info.code_size = fragment ? sizeof mm3_fog_ps_spirv : sizeof mm3_fog_vs_spirv;
    }
    return SDL_CreateGPUShader(device,&info);
}
static int fog_init(SDL_GPUDevice *device)
{
    if (fog_pipe[0] && fog_pipe[1]) return 1;
    SDL_GPUSamplerCreateInfo sampler = {0};
    sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    if (!(fog_vs = fog_shader(device,0)) || !(fog_ps = fog_shader(device,1)) ||
        !(fog_sampler = SDL_CreateGPUSampler(device,&sampler))) goto fail;
    for (int mask = 0; mask < 2; ++mask) {
        SDL_GPUGraphicsPipelineCreateInfo info = {0};
        SDL_GPUColorTargetDescription colour = {0};
        colour.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        colour.blend_state.enable_blend = !mask;
        colour.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colour.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colour.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        colour.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colour.blend_state.color_blend_op = colour.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        colour.blend_state.enable_color_write_mask = true;
        colour.blend_state.color_write_mask = mask ? SDL_GPU_COLORCOMPONENT_A :
            SDL_GPU_COLORCOMPONENT_R|SDL_GPU_COLORCOMPONENT_G|SDL_GPU_COLORCOMPONENT_B|SDL_GPU_COLORCOMPONENT_A;
        info.vertex_shader = fog_vs; info.fragment_shader = fog_ps;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions = &colour;
        info.target_info.num_color_targets = 1;
        if (!(fog_pipe[mask] = SDL_CreateGPUGraphicsPipeline(device,&info))) goto fail;
    }
    return 1;
fail:
    fprintf(stderr,"[MM3_FOG] SDL GPU init failed: %s\n",SDL_GetError());
    for (int i = 0; i < 2; ++i) {
        if (fog_pipe[i]) SDL_ReleaseGPUGraphicsPipeline(device,fog_pipe[i]);
        fog_pipe[i] = NULL;
    }
    if (fog_sampler) SDL_ReleaseGPUSampler(device,fog_sampler);
    if (fog_vs) SDL_ReleaseGPUShader(device,fog_vs);
    if (fog_ps) SDL_ReleaseGPUShader(device,fog_ps);
    fog_sampler = NULL; fog_vs = fog_ps = NULL;
    return 0;
}
static int mm3_fog_draw(const NvHostTarget *target,const float input[12],int mask)
{
    SDL_GPUDevice *device = target->device;
    SDL_GPUCommandBuffer *commands = target->context;
    if (!fog_init(device)) return 0;
    SDL_GPUColorTargetInfo colour = {0};
    colour.texture = target->colour;
    colour.load_op = SDL_GPU_LOADOP_LOAD; colour.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands,&colour,1,NULL);
    if (!pass) return 0;
    SDL_GPUViewport vp = {0,0,(float)target->width,(float)target->height,0,1};
    SDL_Rect scissor = {0,0,(int)target->width,(int)target->height};
    SDL_GPUTextureSamplerBinding binding = {target->depth,fog_sampler};
    SDL_SetGPUViewport(pass,&vp); SDL_SetGPUScissor(pass,&scissor);
    SDL_BindGPUFragmentSamplers(pass,0,&binding,1);
    SDL_BindGPUGraphicsPipeline(pass,fog_pipe[0]);
    SDL_PushGPUFragmentUniformData(commands,0,input,12*sizeof(float));
    SDL_DrawGPUPrimitives(pass,3,1,0,0);
    if (mask) {
        float params[12]; memcpy(params,input,sizeof params); params[7] = 1;
        SDL_BindGPUGraphicsPipeline(pass,fog_pipe[1]);
        SDL_PushGPUFragmentUniformData(commands,0,params,sizeof params);
        SDL_DrawGPUPrimitives(pass,3,1,0,0);
    }
    SDL_EndGPURenderPass(pass);
    return 1;
}

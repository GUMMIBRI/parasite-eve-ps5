/* SPDX-License-Identifier: MIT
 * Native final-image blit replacement. Included with device/presenter state.
 * Standalone SPIR-V modules: no shader-role selector or existing role reuse. */
#include "display_filter_shaders.h"
static int display_filter_requested,display_filter_failed;
static SDL_GPUGraphicsPipeline *display_filter_pipeline;
static SDL_GPUSampler *display_filter_samplers[2];
static SDL_GPUTextureFormat display_filter_format;
void Psyz_VideoSetDisplayFilter(int mode) {
    if(mode<0||mode>2)mode=0;
    if(mode!=display_filter_requested){
        display_filter_failed=0;
        INFOF("Display filter requested=%d 0Off-1Scanlines-2CRT ui=unfiltered observed_pixels=0",mode);
    }
    display_filter_requested=mode;
}
static void DisplayFilterDestroy(void) {
    if(display_filter_pipeline)SDL_ReleaseGPUGraphicsPipeline(device,display_filter_pipeline);
    for(unsigned i=0;i<2;i++)if(display_filter_samplers[i])SDL_ReleaseGPUSampler(device,display_filter_samplers[i]);
    display_filter_pipeline=NULL;display_filter_samplers[0]=display_filter_samplers[1]=NULL;
    display_filter_format=SDL_GPU_TEXTUREFORMAT_INVALID;display_filter_failed=0;
}
static bool DisplayFilterPrepare(SDL_GPUTextureFormat format) {
    if(display_filter_pipeline&&display_filter_format!=format){
        SDL_ReleaseGPUGraphicsPipeline(device,display_filter_pipeline);display_filter_pipeline=NULL;
    }
    if(!display_filter_pipeline){
        SDL_GPUShaderCreateInfo si={0};si.code=cpe_display_filter_vert;si.code_size=sizeof(cpe_display_filter_vert);
        si.entrypoint="main";si.format=SDL_GPU_SHADERFORMAT_SPIRV;si.stage=SDL_GPU_SHADERSTAGE_VERTEX;
        SDL_GPUShader *vs=SDL_CreateGPUShader(device,&si);
        si.code=cpe_display_filter_frag;si.code_size=sizeof(cpe_display_filter_frag);
        si.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;si.num_samplers=1;si.num_uniform_buffers=1;
        SDL_GPUShader *fs=SDL_CreateGPUShader(device,&si);
        if(vs&&fs){
            SDL_GPUColorTargetDescription ct={0};ct.format=format;
            SDL_GPUGraphicsPipelineCreateInfo pi={0};pi.vertex_shader=vs;pi.fragment_shader=fs;
            pi.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pi.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
            pi.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
            pi.target_info.color_target_descriptions=&ct;pi.target_info.num_color_targets=1;
            display_filter_pipeline=SDL_CreateGPUGraphicsPipeline(device,&pi);
        }
        if(vs)SDL_ReleaseGPUShader(device,vs);if(fs)SDL_ReleaseGPUShader(device,fs);
        if(!display_filter_pipeline)return false;
        display_filter_format=format;
    }
    for(unsigned i=0;i<2;i++)if(!display_filter_samplers[i]){
        SDL_GPUSamplerCreateInfo si={0};si.min_filter=si.mag_filter=i?SDL_GPU_FILTER_LINEAR:SDL_GPU_FILTER_NEAREST;
        si.address_mode_u=si.address_mode_v=si.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        display_filter_samplers[i]=SDL_CreateGPUSampler(device,&si);
        if(!display_filter_samplers[i])return false;
    }
    return true;
}
static bool DisplayFilterPresent(SDL_GPUCommandBuffer *cmd,const SDL_GPUBlitInfo *blit) {
    /* Returning false executes the exact prior blit. No resource work for Off,
     * debug VRAM or capture: original pixel-mask admission remains the caller's. */
    if(!display_filter_requested||debug_show_vram||ws_capture_active||display_filter_failed)return false;
    const SDL_GPUBlitRegion s=blit->source,d=blit->destination;
    if(!s.texture||s.texture==d.texture||!s.w||!s.h||!d.w||!d.h||
       s.w>8192||s.h>8192||d.w>8192||d.h>8192)return false;
    SDL_GPUTextureFormat format=SDL_GetGPUSwapchainTextureFormat(device,sdl3_window);
    if(!DisplayFilterPrepare(format))goto failed;
    SDL_GPUColorTargetInfo target={0};target.texture=d.texture;target.load_op=blit->load_op;
    target.clear_color=blit->clear_color;target.store_op=SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass=SDL_BeginGPURenderPass(cmd,&target,1,NULL);
    if(!pass)goto failed;
    SDL_GPUViewport view={(float)d.x,(float)d.y,(float)d.w,(float)d.h,0,1};
    SDL_Rect clip={(int)d.x,(int)d.y,(int)d.w,(int)d.h};
    SDL_SetGPUViewport(pass,&view);SDL_SetGPUScissor(pass,&clip);
    SDL_BindGPUGraphicsPipeline(pass,display_filter_pipeline);
    SDL_GPUTextureSamplerBinding binding={s.texture,display_filter_samplers[blit->filter==SDL_GPU_FILTER_LINEAR]};
    SDL_BindGPUFragmentSamplers(pass,0,&binding,1);
    const float u[12]={(float)s.x,(float)s.y,(float)s.w,(float)s.h,
        (float)d.x,(float)d.y,(float)d.w,(float)d.h,
        (float)SDL_max(1,display_size.y),(float)display_filter_requested,0,0};
    SDL_PushGPUFragmentUniformData(cmd,0,u,sizeof(u));
    SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);return true;
failed:
    display_filter_failed=1;WARNF("Display filter original-blit fallback (latched until selection change/device teardown): %s",SDL_GetError());
    return false;
}

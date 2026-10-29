#include "pe_hd_lookup.h"
#include "synthetic_png.h"
#include "pe_hd_vram.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static SDL_AtomicInt reads,releases;
static uint16_t vram[1024 * 512];
static SDL_ThreadID owner;
static void done(void *user) {
    (void)user; assert(SDL_GetCurrentThreadID()!=owner); SDL_AddAtomicInt(&releases,1);
}
static bool read_bytes(void *user,const char *path,size_t limit,pe_hd_index_bytes *out) {
    (void)user; (void)path; assert(limit>=sizeof(synthetic_png));
    assert(SDL_GetCurrentThreadID()!=owner); SDL_AddAtomicInt(&reads,1);
    *out=(pe_hd_index_bytes){synthetic_png,sizeof(synthetic_png),sizeof(synthetic_png),done,NULL};
    return true;
}
static pe_hd_lookup_result wait_for(pe_hd_lookup *c,pe_hd_lookup_token t,uint64_t state,pe_hd_handle *h) {
    pe_hd_lookup_result r=PE_HD_LOOKUP_PENDING;
    for(unsigned i=0;i<3000;i++) {
        r=pe_hd_lookup_acquire(c,t,state,h);
        if(r!=PE_HD_LOOKUP_PENDING && r!=PE_HD_LOOKUP_BUSY) break;
        SDL_Delay(1);
    }
    return r;
}
static void release_handle(pe_hd_lookup *c,pe_hd_handle *h) {
    pe_hd_lookup_result r=PE_HD_LOOKUP_BUSY;
    for(unsigned i=0;i<3000 && r==PE_HD_LOOKUP_BUSY;i++) {
        r=pe_hd_lookup_release(c,h); if(r==PE_HD_LOOKUP_BUSY) SDL_Delay(1);
    }
    assert(r==PE_HD_LOOKUP_READY && !h->owner);
}
int main(void) {
    const char *name="texpage-P4-0123456789abcdef-0000000000000000-1x1-0-0-1x1-P0-15.png";
    pe_hd_index_record rec={name,strlen(name)};
    pe_hd_pipeline_config cfg={{32,4096,65536,4096},{0,1024*1024,0,4096}};
    cfg.cache.total_bytes=pe_hd_cache_metadata_bytes()+cfg.cache.decode_bytes+4096;
    pe_hd_pipeline *p=NULL; pe_hd_lookup *c=NULL; PeHdFilename key;
    pe_hd_lookup_token t,alias,old; pe_hd_handle h={0},h2={0};
    pe_hd_generation tracker; pe_hd_generation_init(&tracker);
    pe_hd_lookup_proof proof={&tracker,tracker.epoch,1,2,{
        {0,0,1,1,PE_HD_GENERATION_CLIP},{32,0,16,1,PE_HD_GENERATION_CLIP}}};
    assert(PeHd_ParseFilename(name,strlen(name),&key));
    pe_hd_lookup_observation o={key,1,1,0};
    /* Extract synthetic VRAM under the same owner as dependency capture. */
    pe_hd_vram_request sample={0,2,0,0,1,1,0};
    pe_hd_vram_texel texel; pe_hd_vram_metadata meta;
    vram[0]=1; vram[33]=31;
    assert(pe_hd_vram_extract(vram,1024*512,1024,&sample,&texel,1,
        sizeof(texel)+sizeof(meta),&meta)==PE_HD_VRAM_OK);
    assert(texel.color==31 && meta.palette_count==16);
    o.output_width=sample.width; o.output_height=sample.height;
    owner=SDL_GetCurrentThreadID();
    assert(pe_hd_pipeline_create(&cfg,&rec,1,read_bytes,NULL,&p)==PE_HD_PIPELINE_READY);
    assert(pe_hd_lookup_create(p,&c)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&alias)==PE_HD_LOOKUP_PENDING);
    assert(t.serial==alias.serial && t.slot==alias.slot);
    assert(wait_for(c,t,1,&h)==PE_HD_LOOKUP_READY && h.rgba[0]==12);
    assert(wait_for(c,alias,1,&h2)==PE_HD_LOOKUP_READY);
    assert(SDL_GetAtomicInt(&reads)==1); release_handle(c,&h2);
    pe_hd_generation_rect write={32,0,1,1,PE_HD_GENERATION_CLIP};
    vram[33]=0x8000; /* mutation in the same conservatively tracked CLUT tile */
    assert(pe_hd_generation_touch(&tracker,&write)==PE_HD_GENERATION_OK);
    assert(pe_hd_lookup_acquire(c,t,1,&h2)==PE_HD_LOOKUP_FALLBACK && !h2.owner);
    assert(h.rgba[0]==12); old=t;
    assert(pe_hd_lookup_generation(c,2)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_acquire(c,old,1,&h2)==PE_HD_LOOKUP_FALLBACK);
    release_handle(c,&h); proof.checkpoint=tracker.epoch;
    /* State mismatch is terminal, and explicit retirement prevents retry storms. */
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    assert(pe_hd_lookup_acquire(c,t,2,&h)==PE_HD_LOOKUP_FALLBACK);
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&alias)==PE_HD_LOOKUP_FALLBACK);
    assert(pe_hd_lookup_retire(c,t)==PE_HD_LOOKUP_READY);
    PeHdFilename missing=key; missing.texture_hash++;
    assert(pe_hd_lookup_prepare(c,&missing,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    assert(wait_for(c,t,1,&h)==PE_HD_LOOKUP_FALLBACK && !h.owner);
    assert(pe_hd_lookup_retire(c,t)==PE_HD_LOOKUP_READY);
    /* Real decode size mismatch never exposes pixels and drains hidden lease. */
    o.output_width=o.output_height=2;
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    assert(wait_for(c,t,1,&h)==PE_HD_LOOKUP_FALLBACK && !h.owner);
    pe_hd_lookup_result r=PE_HD_LOOKUP_BUSY;
    for(unsigned i=0;i<3000 && r==PE_HD_LOOKUP_BUSY;i++) {
        r=pe_hd_lookup_destroy(c); if(r==PE_HD_LOOKUP_BUSY) SDL_Delay(1);
    }
    assert(r==PE_HD_LOOKUP_READY);
    assert(pe_hd_pipeline_close(p)==PE_HD_PIPELINE_CLOSED);
    /* External lease survives cancellation, coordinator destroy and close. */
    assert(pe_hd_pipeline_create(&cfg,&rec,1,read_bytes,NULL,&p)==PE_HD_PIPELINE_READY);
    assert(pe_hd_lookup_create(p,&c)==PE_HD_LOOKUP_READY);
    o.output_width=o.output_height=1; proof.checkpoint=tracker.epoch;
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    assert(wait_for(c,t,1,&h)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_generation(c,2)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_destroy(c)==PE_HD_LOOKUP_READY);
    assert(pe_hd_pipeline_close(p)==PE_HD_PIPELINE_BUSY);
    assert(h.rgba[0]==12 && h.rgba[3]==78);
    pe_hd_pipeline_result pr=PE_HD_PIPELINE_BUSY;
    for(unsigned i=0;i<3000 && pr==PE_HD_PIPELINE_BUSY;i++) {
        pr=pe_hd_pipeline_release(p,&h); if(pr==PE_HD_PIPELINE_BUSY) SDL_Delay(1);
    }
    assert(pr==PE_HD_PIPELINE_READY && !h.owner);
    assert(pe_hd_pipeline_close(p)==PE_HD_PIPELINE_CLOSED);
    assert(SDL_GetAtomicInt(&reads)==SDL_GetAtomicInt(&releases));
    printf("coordinator metadata: %zu bytes, %u slots, %u dependencies/slot\n",
        pe_hd_lookup_metadata_bytes(),PE_HD_LOOKUP_SLOTS,PE_HD_LOOKUP_DEPENDENCIES);
    puts("PASS real lookup: coalescing, leases, CLUT invalidation, scene/state cancellation, size mismatch, cleanup");
    return 0;
}


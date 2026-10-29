#include "pe_hd_lookup.h"
#include "synthetic_lookup_png.h"
#include "pe_hd_snapshot.h"
#include "pe_hd_vram.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static SDL_AtomicInt reads,releases;
static uint16_t vram[1024 * 512];
static SDL_ThreadID owner;
static const unsigned char *png_bytes;
static size_t png_size;
static void done(void *user) {
    (void)user; assert(SDL_GetCurrentThreadID()!=owner); SDL_AddAtomicInt(&releases,1);
}
static bool read_bytes(void *user,const char *path,size_t limit,pe_hd_index_bytes *out) {
    (void)user; (void)path; assert(limit>=png_size);
    assert(SDL_GetCurrentThreadID()!=owner); SDL_AddAtomicInt(&reads,1);
    *out=(pe_hd_index_bytes){png_bytes,png_size,png_size,done,NULL};
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
static void run(unsigned depth,unsigned scale) {
    SDL_SetAtomicInt(&reads,0); SDL_SetAtomicInt(&releases,0);
    char name[200];
    pe_hd_pipeline_config cfg={{32,4096,65536,4096},{0,4*1024*1024,0,4096}};
    cfg.cache.total_bytes=pe_hd_cache_metadata_bytes()+cfg.cache.decode_bytes+4*1024*1024;
    pe_hd_pipeline *p=NULL; pe_hd_lookup *c=NULL;
    pe_hd_lookup_token t,alias,old; pe_hd_handle h={0},h2={0};
    pe_hd_generation tracker; pe_hd_generation_init(&tracker);
    pe_hd_snapshot snapshot;
    pe_hd_vram_request sample={(uint16_t)(depth<<7),(uint16_t)(400<<6),0,0,256,256,0};
    memset(vram,0,sizeof(vram)); vram[0]=0x3210; vram[400*1024+1]=31;
    assert(pe_hd_snapshot_page(vram,1024*512,1024,&sample,&tracker,66048,&snapshot)==PE_HD_IDENTITY_OK);
    PeHdFilename key=snapshot.key, parsed;
    snprintf(name,sizeof(name),"texpage-P%u-%016llx-%016llx-%ux%u-0-0-256x256-P0-%u.png",
        depth?8:4,(unsigned long long)key.texture_hash,(unsigned long long)key.palette_hash,
        key.source_width,key.source_height,key.palette_last);
    assert(PeHd_ParseFilename(name,strlen(name),&parsed) && pe_hd_index_key_equal(&key,&parsed));
    pe_hd_index_record rec={name,strlen(name)};
    pe_hd_lookup_proof proof={&tracker,snapshot.checkpoint,1,2,{snapshot.dependencies[0],snapshot.dependencies[1]}};
    pe_hd_lookup_observation o={key,256*scale,256*scale,0};
    png_bytes=scale==1?lookup_png_256:lookup_png_512;
    png_size=scale==1?sizeof(lookup_png_256):sizeof(lookup_png_512);
    owner=SDL_GetCurrentThreadID();
    assert(pe_hd_pipeline_create(&cfg,&rec,1,read_bytes,NULL,&p)==PE_HD_PIPELINE_READY);
    assert(pe_hd_lookup_create(p,&c)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&alias)==PE_HD_LOOKUP_PENDING);
    assert(t.serial==alias.serial && t.slot==alias.slot);
    assert(wait_for(c,t,1,&h)==PE_HD_LOOKUP_READY && h.rgba[0]==12);
    assert(h.width==256*scale && h.height==256*scale &&
        h.bytes==(size_t)h.width*h.height*4 && h.rgba[h.bytes-1]==78);
    assert(wait_for(c,alias,1,&h2)==PE_HD_LOOKUP_READY);
    assert(SDL_GetAtomicInt(&reads)==1); release_handle(c,&h2);
    pe_hd_generation_rect write={1,400,1,1,PE_HD_GENERATION_CLIP};
    vram[400*1024+1]=0x8000; /* mutation in the same conservatively tracked CLUT tile */
    assert(pe_hd_generation_touch(&tracker,&write)==PE_HD_GENERATION_OK);
    assert(pe_hd_lookup_acquire(c,t,1,&h2)==PE_HD_LOOKUP_FALLBACK && !h2.owner);
    assert(h.rgba[0]==12); old=t;
    assert(pe_hd_lookup_generation(c,2)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_acquire(c,old,1,&h2)==PE_HD_LOOKUP_FALLBACK);
    release_handle(c,&h);
    /* Restore content, then derive a fresh identity/proof together. Never pair
     * an old content hash with a newer tracker epoch after a palette mutation. */
    vram[400*1024+1]=31;
    assert(pe_hd_generation_touch(&tracker,&write)==PE_HD_GENERATION_OK);
    assert(pe_hd_snapshot_page(vram,1024*512,1024,&sample,&tracker,66048,&snapshot)==PE_HD_IDENTITY_OK);
    assert(pe_hd_index_key_equal(&key,&snapshot.key));
    proof.checkpoint=snapshot.checkpoint;
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
    o.output_width=o.output_height=256*(scale==1?2:1);
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
    o.output_width=o.output_height=256*scale; proof.checkpoint=tracker.epoch;
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
}
int main(void) {
    for(unsigned depth=0;depth<2;depth++) for(unsigned scale=1;scale<=2;scale++) run(depth,scale);
    return 0;
}


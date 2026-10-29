/* Synthetic integration only. No renderer hook or private asset inputs. */
#include "pe_hd_manifest.h"
#include "pe_hd_pack.h"
#include "pe_hd_lookup.h"
#include "pe_hd_snapshot.h"
#include "pe_hd_vram.h"
#include "synthetic_lookup_png.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t vram[1024*512];
static SDL_ThreadID owner;
static SDL_AtomicInt reads;
static bool pack_read(void *u,const char *path,size_t limit,pe_hd_index_bytes *out) {
    assert(SDL_GetCurrentThreadID()!=owner);
    SDL_AddAtomicInt(&reads,1);
    return pe_hd_pack_read(u,path,limit,out);
}
typedef struct pin_job { pe_hd_index *index; PeHdFilename key; pe_hd_source source; } pin_job;
static int pin_worker(void *u) {
    pin_job *j=u;
    assert(pe_hd_index_pin(j->index,&j->key,&j->source)==PE_HD_INDEX_OK);
    return 0;
}
typedef struct pack_job { pe_hd_pack *pack; const char *path; pe_hd_index_bytes bytes; } pack_job;
static int pack_worker(void *u) {
    pack_job *j=u;
    assert(pack_read(j->pack,j->path,sizeof lookup_png_256,&j->bytes));
    return 0;
}
int main(void) {
    owner=SDL_GetCurrentThreadID();
    char root[1024],path[1600],name[256],manifest[512];
    char *cwd=SDL_GetCurrentDirectory(); assert(cwd);
    for(char *p=cwd;*p;p++) if(*p=='\\') *p='/';
    assert(SDL_snprintf(root,sizeof root,"%shd-e2e-%llu",cwd,
        (unsigned long long)SDL_GetTicksNS())<(int)sizeof root);
    SDL_free(cwd); assert(SDL_CreateDirectory(root));
    pe_hd_generation tracker; pe_hd_generation_init(&tracker);
    pe_hd_snapshot snapshot;
    pe_hd_vram_request sample={0,(uint16_t)(400<<6),0,0,256,256,0};
    vram[0]=0x3210; vram[400*1024+1]=31;
    assert(pe_hd_snapshot_page(vram,1024*512,1024,&sample,&tracker,66048,&snapshot)==PE_HD_IDENTITY_OK);
    const PeHdFilename *key=&snapshot.key;
    assert(SDL_snprintf(name,sizeof name,"texpage-P4-%016llx-%016llx-%ux%u-0-0-256x256-P0-%u.png",
        (unsigned long long)key->texture_hash,(unsigned long long)key->palette_hash,
        key->source_width,key->source_height,key->palette_last)<(int)sizeof name);
    assert(SDL_snprintf(path,sizeof path,"%s/%s",root,name)<(int)sizeof path);
    FILE *file=fopen(path,"wb"); assert(file);
    assert(fwrite(lookup_png_256,1,sizeof lookup_png_256,file)==sizeof lookup_png_256);
    assert(fclose(file)==0);
    pe_hd_pack *pack=NULL;
    pe_hd_pack_config pack_cfg={root,2*sizeof lookup_png_256,PE_HD_PACK_TRUSTED_IMMUTABLE_ROOT};
    assert(pe_hd_pack_create(&pack_cfg,&pack)==PE_HD_PACK_OK);
    pe_hd_manifest_config mc={sizeof manifest,PE_HD_MANIFEST_MAX_LINE,4096,{4,1024,65536,2*sizeof lookup_png_256}};
    int length=SDL_snprintf(manifest,sizeof manifest,"PE-HD-MANIFEST\t1\nfile\t%s\n",name);
    assert(length>0 && length<(int)sizeof manifest);
    pe_hd_index *index=NULL;
    assert(pe_hd_manifest_create(&mc,manifest,(size_t)length,pack_read,pack,&index)==PE_HD_INDEX_OK);
    memset(manifest,0,sizeof manifest); assert(SDL_GetAtomicInt(&reads)==0);
    pe_hd_pipeline_config pc={mc.index,{0,4*1024*1024,0,2*sizeof lookup_png_256}};
    pc.cache.total_bytes=pe_hd_cache_metadata_bytes()+pc.cache.decode_bytes+pc.cache.borrowed_encoded_bytes;
    pe_hd_pipeline *pipeline=NULL; pe_hd_lookup *lookup=NULL;
    pe_hd_index *original=index;
    assert(pe_hd_pipeline_adopt(NULL,&index,&pipeline)==PE_HD_PIPELINE_INVALID);
    assert(index==original && !pipeline);
    assert(pe_hd_pipeline_adopt(&pc.cache,&index,NULL)==PE_HD_PIPELINE_INVALID);
    assert(index==original);
    pe_hd_cache_config bad={0};
    assert(pe_hd_pipeline_adopt(&bad,&index,&pipeline)==PE_HD_PIPELINE_FULL);
    assert(index==original && !pipeline);
    /* Failed construction retains a usable index and its provider contract. */
    pin_job job={index,*key,{0}};
    SDL_Thread *thread=SDL_CreateThread(pin_worker,"manifest-pin",&job); assert(thread);
    SDL_WaitThread(thread,NULL);
    assert(job.source.length==sizeof lookup_png_256);
    job.source.done(job.source.user); memset(&job.source,0,sizeof job.source);
    /* That index has been used: destroy it, adopt only a fresh one. */
    assert(pe_hd_index_destroy(index)==PE_HD_INDEX_OK); index=NULL;
    length=SDL_snprintf(manifest,sizeof manifest,"PE-HD-MANIFEST\t1\nfile\t%s\n",name);
    assert(pe_hd_manifest_create(&mc,manifest,(size_t)length,pack_read,pack,&index)==PE_HD_INDEX_OK);
    memset(manifest,0,sizeof manifest);
    assert(pe_hd_pipeline_adopt(&pc.cache,&index,&pipeline)==PE_HD_PIPELINE_READY);
    assert(!index && pipeline);
    pe_hd_pipeline *saved=pipeline;
    assert(pe_hd_pipeline_adopt(&pc.cache,&index,&pipeline)==PE_HD_PIPELINE_INVALID);
    assert(!index && pipeline==saved);
    assert(pe_hd_lookup_create(pipeline,&lookup)==PE_HD_LOOKUP_READY);
    pe_hd_lookup_proof proof={&tracker,snapshot.checkpoint,1,2,{snapshot.dependencies[0],snapshot.dependencies[1]}};
    pe_hd_lookup_observation observed={*key,256,256,0}; pe_hd_lookup_token token;
    assert(pe_hd_lookup_prepare(lookup,key,&observed,&proof,&token)==PE_HD_LOOKUP_PENDING);
    pe_hd_handle lease={0}; pe_hd_lookup_result result=PE_HD_LOOKUP_PENDING;
    for(unsigned i=0;i<3000;i++) {
        result=pe_hd_lookup_acquire(lookup,token,1,&lease);
        if(result!=PE_HD_LOOKUP_BUSY && result!=PE_HD_LOOKUP_PENDING) break;
        SDL_Delay(1);
    }
    assert(result==PE_HD_LOOKUP_READY && lease.width==256 && lease.height==256);
    assert(lease.rgba[0]==12 && lease.rgba[lease.bytes-1]==78);
    assert(SDL_GetAtomicInt(&reads)==2);
    /* Independent provider pin, not illicit access to the adopted index. */
    pack_job held={pack,name,{0}};
    thread=SDL_CreateThread(pack_worker,"provider-pin",&held); assert(thread);
    SDL_WaitThread(thread,NULL);
    assert(SDL_GetAtomicInt(&reads)==3);
    assert(pe_hd_lookup_generation(lookup,2)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_destroy(lookup)==PE_HD_LOOKUP_READY);
    assert(pe_hd_pipeline_close(pipeline)==PE_HD_PIPELINE_BUSY);
    /* Decoded lease and independent provider pin survive cancellation. */
    assert(pe_hd_pack_close(pack)==PE_HD_PACK_BUSY);
    assert(lease.rgba[0]==12);
    pe_hd_pipeline_result released=PE_HD_PIPELINE_BUSY;
    for(unsigned i=0;i<3000 && released==PE_HD_PIPELINE_BUSY;i++) {
        released=pe_hd_pipeline_release(pipeline,&lease);
        if(released==PE_HD_PIPELINE_BUSY) SDL_Delay(1);
    }
    assert(released==PE_HD_PIPELINE_READY && !lease.owner);
    assert(pe_hd_pipeline_close(pipeline)==PE_HD_PIPELINE_CLOSED);
    assert(pe_hd_pack_close(pack)==PE_HD_PACK_BUSY);
    held.bytes.release(held.bytes.user); memset(&held.bytes,0,sizeof held.bytes);
    /* Success proves provider has zero outstanding encoded pin slots/bytes. */
    assert(pe_hd_pack_close(pack)==PE_HD_PACK_OK);
    assert(SDL_RemovePath(path)); assert(SDL_RemovePath(root));
    puts("PASS e2e: snapshot/TSV/index/pack/pipeline/cache/lookup; encoded pin and decoded lease drain independently");
    return 0;
}



/* Synthetic host-only constructor fault injection. No production API changes.
 * Only pipeline/cache translation units call these forwarding shims. Index and
 * SDL internals retain their real allocation/synchronization functions. */
#include "pe_hd_pipeline.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ALLOC, MUTEX, CONDITION, THREAD, KINDS };
static unsigned calls[KINDS], live[KINDS], fail_kind, fail_at, hits;
static void *allocations[4];
static int fails(unsigned kind) {
    ++calls[kind];
    if(kind==fail_kind && calls[kind]==fail_at) { ++hits; return 1; }
    return 0;
}
void *fault_calloc(size_t n,size_t bytes) {
    if(fails(ALLOC)) return NULL;
    void *p=calloc(n,bytes);
    assert(p);
    for(unsigned i=0;i<4;i++) if(!allocations[i]) {
        allocations[i]=p; ++live[ALLOC]; return p;
    }
    abort();
}
void fault_free(void *p) {
    if(p) {
        unsigned i;
        for(i=0;i<4 && allocations[i]!=p;i++) {}
        /* No requests are submitted: only constructor objects are allocated. */
        assert(i<4 && live[ALLOC]); allocations[i]=NULL; --live[ALLOC];
    }
    free(p);
}
SDL_Mutex * SDLCALL fault_CreateMutex(void) {
    if(fails(MUTEX)) return NULL;
    SDL_Mutex *p=SDL_CreateMutex(); assert(p); ++live[MUTEX]; return p;
}
void SDLCALL fault_DestroyMutex(SDL_Mutex *p) {
    assert(p && live[MUTEX]); SDL_DestroyMutex(p); --live[MUTEX];
}
SDL_Condition * SDLCALL fault_CreateCondition(void) {
    if(fails(CONDITION)) return NULL;
    SDL_Condition *p=SDL_CreateCondition(); assert(p); ++live[CONDITION]; return p;
}
void SDLCALL fault_DestroyCondition(SDL_Condition *p) {
    assert(p && live[CONDITION]); SDL_DestroyCondition(p); --live[CONDITION];
}
SDL_Thread * SDLCALL fault_CreateThreadRuntime(SDL_ThreadFunction fn,
    const char *name,void *data,SDL_FunctionPointer begin,SDL_FunctionPointer end) {
    if(fails(THREAD)) return NULL;
    SDL_Thread *p=SDL_CreateThreadRuntime(fn,name,data,begin,end);
    assert(p); ++live[THREAD]; return p;
}
void SDLCALL fault_WaitThread(SDL_Thread *p,int *status) {
    assert(p && live[THREAD]); SDL_WaitThread(p,status); --live[THREAD];
}
static unsigned reads,releases;
static const unsigned char bytes[]={1,2,3,4};
static void unpin(void *user) { assert(user==&reads); ++releases; }
static bool read_bytes(void *user,const char *path,size_t limit,pe_hd_index_bytes *out) {
    assert(user==&reads && strstr(path,"texpage-") && limit>=sizeof bytes);
    ++reads; *out=(pe_hd_index_bytes){bytes,sizeof bytes,sizeof bytes,unpin,user}; return true;
}
int main(void) {
    const char *name="texpage-P4-0123456789abcdef-0000000000000000-64x64-0-0-32x32-P0-15.png";
    pe_hd_index_record record={name,strlen(name)};
    pe_hd_index_config ic={4,1024,65536,4096};
    pe_hd_cache_config cc={0,1024*1024,0,4096};
    cc.total_bytes=pe_hd_cache_metadata_bytes()+cc.decode_bytes+cc.borrowed_encoded_bytes;
    PeHdFilename key; assert(PeHd_ParseFilename(name,strlen(name),&key));
    unsigned cases=0;
    for(unsigned kind=0;kind<KINDS;kind++) for(unsigned nth=1;nth<=2;nth++) {
        pe_hd_index *index=NULL;
        assert(pe_hd_index_create(&ic,&record,1,read_bytes,&reads,&index)==PE_HD_INDEX_OK);
        pe_hd_index *original=index;
        /* Valid non-owning sentinel address: never dereferenced. */
        max_align_t sentinel;
        pe_hd_pipeline *output=(pe_hd_pipeline *)(void *)&sentinel;
        memset(calls,0,sizeof calls); fail_kind=kind; fail_at=nth; hits=0;
        assert(pe_hd_pipeline_adopt(&cc,&index,&output)==PE_HD_PIPELINE_FULL);
        assert(hits==1 && index==original && output==(pe_hd_pipeline *)(void *)&sentinel);
        for(unsigned i=0;i<KINDS;i++) assert(!live[i]);
        pe_hd_source source={0};
        assert(pe_hd_index_pin(index,&key,&source)==PE_HD_INDEX_OK);
        assert(source.length==sizeof bytes && !memcmp(source.png,bytes,sizeof bytes));
        assert(pe_hd_index_destroy(index)==PE_HD_INDEX_BUSY);
        source.done(source.user);
        assert(reads==releases && pe_hd_index_destroy(index)==PE_HD_INDEX_OK);
        ++cases;
    }
    /* Recovery control: unarmed shims forward both worker lifecycles. */
    fail_kind=KINDS; fail_at=0; memset(calls,0,sizeof calls);
    pe_hd_index *index=NULL; pe_hd_pipeline *pipeline=NULL;
    assert(pe_hd_index_create(&ic,&record,1,read_bytes,&reads,&index)==PE_HD_INDEX_OK);
    assert(pe_hd_pipeline_adopt(&cc,&index,&pipeline)==PE_HD_PIPELINE_READY && !index);
    for(unsigned i=0;i<KINDS;i++) assert(calls[i]==2 && live[i]==2);
    assert(pe_hd_pipeline_close(pipeline)==PE_HD_PIPELINE_CLOSED);
    for(unsigned i=0;i<KINDS;i++) assert(!live[i]);
    printf("adoption faults: %u failures + recovery PASS; reads=%u releases=%u\n",cases,reads,releases);
    return 0;
}


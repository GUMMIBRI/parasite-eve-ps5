#include "pe_hd_pipeline.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

enum { FREE, QUEUED, READING, SUBMITTED, FAILED, CANCELLED };
typedef struct request {
    unsigned state;
    uint64_t serial, generation;
    PeHdFilename key;
    pe_hd_ticket ticket;
} request;
struct pe_hd_pipeline {
    SDL_Mutex *mutex;
    SDL_Condition *wake;
    SDL_Thread *worker;
    pe_hd_index *index;
    pe_hd_cache *cache;
    uint64_t generation, serial;
    int closing;
    request requests[PE_HD_PIPELINE_REQUESTS];
};
static int read_worker(void *arg)
{
    pe_hd_pipeline *p=arg;
    SDL_LockMutex(p->mutex);
    for (;;) {
        unsigned i;
        request *r;
        PeHdFilename key;
        uint64_t generation;
        pe_hd_source source={0};
        pe_hd_ticket ticket={0};
        pe_hd_cache_result result=PE_HD_CACHE_INVALID;
        if(p->closing) break;
        for(i=0;i<PE_HD_PIPELINE_REQUESTS && p->requests[i].state!=QUEUED;i++) {}
        if(i==PE_HD_PIPELINE_REQUESTS) {
            SDL_WaitCondition(p->wake,p->mutex); continue;
        }
        r=&p->requests[i]; r->state=READING;
        key=r->key; generation=r->generation;
        SDL_UnlockMutex(p->mutex);
        if(pe_hd_index_pin(p->index,&key,&source)==PE_HD_INDEX_OK) {
            /* Submit preflight and decode occur only on workers. A concurrent
             * generation transition rejects stale submits or invalidates them.
             * PENDING alone transfers pin; every failure releases exactly once. */
            result=pe_hd_cache_submit(p->cache,generation,&source,0,&ticket);
            if(result!=PE_HD_CACHE_PENDING) source.done(source.user);
        }
        SDL_LockMutex(p->mutex);
        if(p->closing || r->state==CANCELLED || generation!=p->generation)
            memset(r,0,sizeof(*r));
        else if(result==PE_HD_CACHE_PENDING) { r->ticket=ticket; r->state=SUBMITTED; }
        else r->state=FAILED;
    }
    SDL_UnlockMutex(p->mutex); return 0;
}
size_t pe_hd_pipeline_metadata_bytes(void) { return sizeof(pe_hd_pipeline); }
pe_hd_pipeline_result pe_hd_pipeline_create(const pe_hd_pipeline_config *cfg,
    const pe_hd_index_record *records,size_t count,pe_hd_index_read read,void *user,
    pe_hd_pipeline **out)
{
    pe_hd_pipeline *p;
    pe_hd_index_result indexed;
    if(!cfg || !out || !read) return PE_HD_PIPELINE_INVALID;
    p=calloc(1,sizeof(*p));
    if(!p) return PE_HD_PIPELINE_FULL;
    indexed=pe_hd_index_create(&cfg->index,records,count,read,user,&p->index);
    if(indexed!=PE_HD_INDEX_OK) {
        free(p);
        return indexed==PE_HD_INDEX_LIMIT || indexed==PE_HD_INDEX_MEMORY?
            PE_HD_PIPELINE_FULL:PE_HD_PIPELINE_INVALID;
    }
    p->cache=pe_hd_cache_create(&cfg->cache);
    p->mutex=SDL_CreateMutex(); p->wake=SDL_CreateCondition(); p->generation=1;
    if(p->cache && p->mutex && p->wake)
        p->worker=SDL_CreateThread(read_worker,"pe-hd-provider",p);
    if(!p->worker) {
        if(p->cache) (void)pe_hd_cache_close(p->cache);
        if(p->wake) SDL_DestroyCondition(p->wake);
        if(p->mutex) SDL_DestroyMutex(p->mutex);
        (void)pe_hd_index_destroy(p->index); free(p); return PE_HD_PIPELINE_FULL;
    }
    *out=p; return PE_HD_PIPELINE_READY;
}
pe_hd_pipeline_result pe_hd_pipeline_request(pe_hd_pipeline *p,uint64_t generation,
    const PeHdFilename *key,pe_hd_request *out)
{
    unsigned i;
    if(!p || !key || !out) return PE_HD_PIPELINE_INVALID;
    SDL_LockMutex(p->mutex);
    if(p->closing) { SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_CLOSED; }
    if(generation!=p->generation || p->serial==UINT64_MAX) {
        SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_INVALID;
    }
    for(i=0;i<PE_HD_PIPELINE_REQUESTS && p->requests[i].state!=FREE;i++) {}
    if(i==PE_HD_PIPELINE_REQUESTS) { SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_FULL; }
    p->requests[i]=(request){QUEUED,++p->serial,generation,*key,{0}};
    *out=(pe_hd_request){p->serial,i};
    SDL_SignalCondition(p->wake); SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_PENDING;
}
static request *find(pe_hd_pipeline *p,pe_hd_request t)
{
    if(t.slot>=PE_HD_PIPELINE_REQUESTS || !t.serial) return NULL;
    request *r=&p->requests[t.slot];
    return r->serial==t.serial && r->state!=FREE && r->state!=CANCELLED?r:NULL;
}
pe_hd_pipeline_result pe_hd_pipeline_forget(pe_hd_pipeline *p,pe_hd_request t)
{
    request *r;
    if(!p) return PE_HD_PIPELINE_INVALID;
    SDL_LockMutex(p->mutex);
    if(p->closing) { SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_CLOSED; }
    r=find(p,t);
    if(!r) { SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_INVALID; }
    if(r->state==READING) r->state=CANCELLED; else memset(r,0,sizeof(*r));
    SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_READY;
}
pe_hd_pipeline_result pe_hd_pipeline_generation(pe_hd_pipeline *p,uint64_t next)
{
    if(!p) return PE_HD_PIPELINE_INVALID;
    SDL_LockMutex(p->mutex);
    if(p->closing) { SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_CLOSED; }
    if(next<=p->generation || pe_hd_cache_generation(p->cache,next)!=PE_HD_CACHE_READY) {
        SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_INVALID;
    }
    p->generation=next;
    for(unsigned i=0;i<PE_HD_PIPELINE_REQUESTS;i++) {
        request *r=&p->requests[i];
        if(r->state==READING || r->state==CANCELLED) r->state=CANCELLED;
        else memset(r,0,sizeof(*r));
    }
    SDL_UnlockMutex(p->mutex); return PE_HD_PIPELINE_READY;
}
static pe_hd_pipeline_result mapped(pe_hd_cache_result r)
{
    switch(r) {
    case PE_HD_CACHE_READY: return PE_HD_PIPELINE_READY;
    case PE_HD_CACHE_PENDING: return PE_HD_PIPELINE_PENDING;
    case PE_HD_CACHE_BUSY: return PE_HD_PIPELINE_BUSY;
    case PE_HD_CACHE_FULL: return PE_HD_PIPELINE_FULL;
    case PE_HD_CACHE_CLOSED: return PE_HD_PIPELINE_CLOSED;
    default: return PE_HD_PIPELINE_FALLBACK;
    }
}
pe_hd_pipeline_result pe_hd_pipeline_poll(pe_hd_pipeline *p,pe_hd_request t,pe_hd_handle *out)
{
    request *r;
    pe_hd_pipeline_result result;
    if(!p || !out || out->rgba || out->serial || out->owner) return PE_HD_PIPELINE_INVALID;
    if(!SDL_TryLockMutex(p->mutex)) return PE_HD_PIPELINE_BUSY;
    r=find(p,t);
    if(p->closing) result=PE_HD_PIPELINE_CLOSED;
    else if(!r || r->state==FAILED) result=PE_HD_PIPELINE_FALLBACK;
    else if(r->state==SUBMITTED) result=mapped(pe_hd_cache_acquire(p->cache,r->ticket,out));
    else result=PE_HD_PIPELINE_PENDING;
    SDL_UnlockMutex(p->mutex); return result;
}
pe_hd_pipeline_result pe_hd_pipeline_release(pe_hd_pipeline *p,pe_hd_handle *h)
{
    pe_hd_cache_result result;
    if(!p) return PE_HD_PIPELINE_INVALID;
    result=pe_hd_cache_release(p->cache,h);
    return result==PE_HD_CACHE_INVALID?PE_HD_PIPELINE_INVALID:mapped(result);
}
pe_hd_pipeline_result pe_hd_pipeline_close(pe_hd_pipeline *p)
{
    pe_hd_cache_result result;
    if(!p) return PE_HD_PIPELINE_INVALID;
    SDL_LockMutex(p->mutex); p->closing=1;
    SDL_SignalCondition(p->wake); SDL_UnlockMutex(p->mutex);
    if(p->worker) { SDL_WaitThread(p->worker,NULL); p->worker=NULL; }
    result=pe_hd_cache_close(p->cache);
    if(result!=PE_HD_CACHE_CLOSED) return mapped(result);
    /* Cache joined its worker, so every transferred pin is now released. */
    (void)pe_hd_index_destroy(p->index);
    SDL_DestroyCondition(p->wake); SDL_DestroyMutex(p->mutex); free(p);
    return PE_HD_PIPELINE_CLOSED;
}


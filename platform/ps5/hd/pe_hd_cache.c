#include "pe_hd_cache.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

enum { EMPTY, QUEUED, DECODING, COMPLETE, READY, FAILED };
typedef struct entry {
    uint64_t serial, generation, age;
    unsigned state, refs;
    int copied;
    pe_hd_source source;
    pe_hd_image image;
} entry;
typedef struct lease { uint64_t serial; unsigned slot, state; } lease;
struct pe_hd_cache {
    SDL_Mutex *mutex;
    SDL_Condition *wake;
    SDL_Thread *worker;
    pe_hd_context decode;
    pe_hd_cache_config config;
    size_t owned, borrowed;
    uint64_t generation, serial, age;
    int closing, reclaim;
    unsigned requests[PE_HD_CACHE_SLOTS], completions[PE_HD_CACHE_SLOTS];
    unsigned releases[PE_HD_CACHE_LEASES];
    unsigned nr, nc, nl;
    entry entries[PE_HD_CACHE_SLOTS];
    lease leases[PE_HD_CACHE_LEASES];
};
static uint32_t be32(const unsigned char *p)
{ return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
/* Same public PNG envelope policy as adapter; no decoder/hash reimplementation. */
static int preflight(const pe_hd_source *s)
{
    static const unsigned char sig[8]={137,80,78,71,13,10,26,10};
    const unsigned char *p;
    size_t bytes;
    if (!s || !(p=s->png) || s->length<33 ||
        s->length>PE_HD_MAX_ENCODED_BYTES || s->length>INT_MAX) return 0;
    return !memcmp(p,sig,8) && be32(p+8)==13 && !memcmp(p+12,"IHDR",4) &&
        p[24]==8 && p[25]==6 && !p[26] && !p[27] && p[28]<=1 &&
        pe_hd_rgba_size(be32(p+16),be32(p+20),&bytes)==PE_HD_OK;
}
static void drain_releases(pe_hd_cache *c)
{
    while(c->nl) {
        lease *l=&c->leases[c->releases[--c->nl]];
        --c->entries[l->slot].refs;
        memset(l,0,sizeof(*l));
    }
}
static void retire(entry *e)
{
    pe_hd_image_release(&e->image); /* always worker */
    memset(e,0,sizeof(*e));
}
static void publish(pe_hd_cache *c)
{
    while(c->nc) {
        entry *v=&c->entries[c->completions[--c->nc]];
        if(v->state==COMPLETE) v->state=v->image.rgba?READY:FAILED;
    }
}
static int worker(void *arg)
{
    pe_hd_cache *c=arg;
    SDL_LockMutex(c->mutex);
    for (;;) {
        unsigned i, selected=PE_HD_CACHE_SLOTS;
        int held=0;
        drain_releases(c);
        publish(c);
        for(i=0;i<PE_HD_CACHE_SLOTS;i++) {
            entry *e=&c->entries[i];
            held |= e->refs!=0;
            if (!e->refs && e->state>=COMPLETE &&
                (c->closing || e->generation!=c->generation)) retire(e);
        }
        if(c->reclaim) {
            unsigned oldest=PE_HD_CACHE_SLOTS;
            for(i=0;i<PE_HD_CACHE_SLOTS;i++) {
                entry *v=&c->entries[i];
                if(v->state>=READY && !v->refs &&
                    (oldest==PE_HD_CACHE_SLOTS || v->age<c->entries[oldest].age)) oldest=i;
            }
            if(oldest<PE_HD_CACHE_SLOTS) retire(&c->entries[oldest]);
            c->reclaim=0;
        }
        if (c->nr) {
            selected=c->requests[0];
            memmove(c->requests,c->requests+1,--c->nr*sizeof(c->requests[0]));
        } else if(c->closing && !held) break;
        if(selected==PE_HD_CACHE_SLOTS) { SDL_WaitCondition(c->wake,c->mutex); continue; }
        {
            entry *e=&c->entries[selected];
            pe_hd_source source=e->source;
            int decode=!c->closing && e->generation==c->generation;
            e->state=DECODING;
            /* Worker-only LRU: reclaim oldest unheld retained results before
             * decode. Conservative eviction leaves pinned outputs charged. */
            for(i=0;i<PE_HD_CACHE_SLOTS;i++) {
                unsigned j, oldest=PE_HD_CACHE_SLOTS;
                if(c->decode.live_bytes<=c->decode.budget/2) break;
                for(j=0;j<PE_HD_CACHE_SLOTS;j++) {
                    entry *v=&c->entries[j];
                    if(v->state==READY && !v->refs &&
                       (oldest==PE_HD_CACHE_SLOTS || v->age<c->entries[oldest].age)) oldest=j;
                }
                if(oldest==PE_HD_CACHE_SLOTS) break;
                retire(&c->entries[oldest]);
            }
            SDL_UnlockMutex(c->mutex);
            if(decode) (void)pe_hd_image_decode(&c->decode,source.png,source.length,&e->image);
            if(e->copied) free((void *)source.png);
            else source.done(source.user);
            SDL_LockMutex(c->mutex);
            if(e->copied) c->owned-=source.length; else c->borrowed-=source.retained_bytes;
            memset(&e->source,0,sizeof(e->source));
            if(c->closing || e->generation!=c->generation) retire(e);
            else {
                /* At most one completion per occupied entry; bounded by slots. */
                e->state=COMPLETE;
                c->completions[c->nc++]=selected;
            }
        }
    }
    SDL_UnlockMutex(c->mutex);
    return 0;
}
size_t pe_hd_cache_metadata_bytes(void) { return sizeof(pe_hd_cache); }
pe_hd_cache *pe_hd_cache_create(const pe_hd_cache_config *cfg)
{
    pe_hd_cache *c;
    size_t left;
    if(!cfg || cfg->total_bytes>PE_HD_MAX_TOTAL_BYTES || cfg->total_bytes<sizeof(*c)) return NULL;
    left=cfg->total_bytes-sizeof(*c);
    if(!cfg->decode_bytes || cfg->decode_bytes>left) return NULL;
    left-=cfg->decode_bytes;
    if(cfg->owned_encoded_bytes>left) return NULL;
    left-=cfg->owned_encoded_bytes;
    if(cfg->borrowed_encoded_bytes>left) return NULL;
    c=calloc(1,sizeof(*c));
    if(!c) return NULL;
    c->config=*cfg; c->generation=1;
    if(pe_hd_context_init(&c->decode,cfg->decode_bytes)!=PE_HD_OK) { free(c); return NULL; }
    c->mutex=SDL_CreateMutex(); c->wake=SDL_CreateCondition();
    if(c->mutex && c->wake) c->worker=SDL_CreateThread(worker,"pe-hd-cache",c);
    if(!c->worker) {
        if(c->wake) SDL_DestroyCondition(c->wake);
        if(c->mutex) SDL_DestroyMutex(c->mutex);
        free(c); return NULL;
    }
    return c;
}
pe_hd_cache_result pe_hd_cache_submit(pe_hd_cache *c,uint64_t generation,
    const pe_hd_source *s,int copy,pe_hd_ticket *out)
{
    unsigned i;
    entry *e;
    pe_hd_source source;
    size_t *used, cap, charge;
    if(!c || !out || !preflight(s) ||
       (!copy && (!s->done || s->retained_bytes<s->length))) return PE_HD_CACHE_INVALID;
    SDL_LockMutex(c->mutex);
    if(c->closing) { SDL_UnlockMutex(c->mutex); return PE_HD_CACHE_CLOSED; }
    if(generation!=c->generation || c->serial==UINT64_MAX) { SDL_UnlockMutex(c->mutex); return PE_HD_CACHE_INVALID; }
    for(i=0;i<PE_HD_CACHE_SLOTS && c->entries[i].state!=EMPTY;i++) {}
    used=copy?&c->owned:&c->borrowed;
    cap=copy?c->config.owned_encoded_bytes:c->config.borrowed_encoded_bytes;
    charge=copy?s->length:s->retained_bytes;
    if(i==PE_HD_CACHE_SLOTS || charge>cap-*used) {
        if(i==PE_HD_CACHE_SLOTS) { c->reclaim=1; SDL_SignalCondition(c->wake); }
        SDL_UnlockMutex(c->mutex); return PE_HD_CACHE_FULL;
    }
    source=*s;
    if(copy) {
        void *p=malloc(s->length);
        if(!p) { SDL_UnlockMutex(c->mutex); return PE_HD_CACHE_FULL; }
        memcpy(p,s->png,s->length); source.png=p;
    }
    *used+=charge;
    e=&c->entries[i]; e->source=source; e->copied=!!copy;
    e->generation=generation; e->serial=++c->serial; e->state=QUEUED;
    c->requests[c->nr++]=i;
    out->slot=i; out->serial=e->serial;
    SDL_SignalCondition(c->wake); SDL_UnlockMutex(c->mutex);
    return PE_HD_CACHE_PENDING;
}
pe_hd_cache_result pe_hd_cache_acquire(pe_hd_cache *c,pe_hd_ticket t,pe_hd_handle *out)
{
    entry *e;
    unsigned i;
    pe_hd_cache_result result=PE_HD_CACHE_INVALID;
    if(!c || !out || out->rgba || out->serial || out->owner || t.slot>=PE_HD_CACHE_SLOTS) return result;
    if(!SDL_TryLockMutex(c->mutex)) return PE_HD_CACHE_BUSY;
    /* Completed indices may have been retired; only COMPLETE entries publish. */
    publish(c);
    e=&c->entries[t.slot];
    if(c->closing) result=PE_HD_CACHE_CLOSED;
    else if(e->serial==t.serial && t.serial && e->generation==c->generation) {
        if(e->state==QUEUED || e->state==DECODING) result=PE_HD_CACHE_PENDING;
        else if(e->state==READY) {
            for(i=0;i<PE_HD_CACHE_LEASES && c->leases[i].state;i++) {}
            result=PE_HD_CACHE_FULL;
            if(i<PE_HD_CACHE_LEASES && c->serial!=UINT64_MAX && c->age!=UINT64_MAX) {
                lease *l=&c->leases[i]; l->slot=t.slot; l->serial=++c->serial; l->state=1;
                ++e->refs; e->age=++c->age;
                out->rgba=e->image.rgba; out->width=e->image.width; out->height=e->image.height;
                out->bytes=e->image.bytes; out->lease=i; out->serial=l->serial;
                out->owner=c;
                result=PE_HD_CACHE_READY;
            }
        }
    }
    SDL_UnlockMutex(c->mutex); return result;
}
pe_hd_cache_result pe_hd_cache_release(pe_hd_cache *c,pe_hd_handle *h)
{
    lease *l;
    if(!c || !h || h->owner!=c || !h->serial || h->lease>=PE_HD_CACHE_LEASES) return PE_HD_CACHE_INVALID;
    if(!SDL_TryLockMutex(c->mutex)) return PE_HD_CACHE_BUSY;
    l=&c->leases[h->lease];
    if(l->serial!=h->serial || l->state!=1) { SDL_UnlockMutex(c->mutex); return PE_HD_CACHE_INVALID; }
    l->state=2; c->releases[c->nl++]=h->lease;
    memset(h,0,sizeof(*h));
    SDL_SignalCondition(c->wake); SDL_UnlockMutex(c->mutex); return PE_HD_CACHE_READY;
}
pe_hd_cache_result pe_hd_cache_generation(pe_hd_cache *c,uint64_t next)
{
    pe_hd_cache_result r=PE_HD_CACHE_READY;
    if(!c) return PE_HD_CACHE_INVALID;
    SDL_LockMutex(c->mutex);
    if(c->closing) r=PE_HD_CACHE_CLOSED;
    else if(next<=c->generation) r=PE_HD_CACHE_INVALID;
    else { c->generation=next; SDL_SignalCondition(c->wake); }
    SDL_UnlockMutex(c->mutex); return r;
}
pe_hd_cache_result pe_hd_cache_close(pe_hd_cache *c)
{
    unsigned i;
    int held=0;
    if(!c) return PE_HD_CACHE_INVALID;
    SDL_LockMutex(c->mutex); c->closing=1;
    for(i=0;i<PE_HD_CACHE_LEASES;i++) held |= c->leases[i].state==1;
    SDL_SignalCondition(c->wake); SDL_UnlockMutex(c->mutex);
    if(held) return PE_HD_CACHE_BUSY;
    SDL_WaitThread(c->worker,NULL);
    SDL_DestroyCondition(c->wake); SDL_DestroyMutex(c->mutex); free(c);
    return PE_HD_CACHE_CLOSED;
}


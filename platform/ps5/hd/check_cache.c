/* Bounded synthetic lifecycle fixture. Actual SDL worker, no platform stubs.
 * Compiled/linked for PS5; run only on host. No pack/game inputs. */
#include "pe_hd_cache.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <string.h>
#include "synthetic_png.h"
static SDL_AtomicInt returned;
static void done(void *user) { (void)user; SDL_AddAtomicInt(&returned,1); }
typedef struct gate { SDL_Semaphore *entered, *proceed; } gate;
static void gated_done(void *user)
{
    gate *g=user;
    if(SDL_AddAtomicInt(&returned,1)==0) {
        SDL_SignalSemaphore(g->entered);
        assert(SDL_WaitSemaphoreTimeout(g->proceed,3000));
    }
}
static pe_hd_cache_result acquire_until(pe_hd_cache *c, pe_hd_ticket t, pe_hd_handle *h)
{
    unsigned n;
    pe_hd_cache_result r=PE_HD_CACHE_PENDING;
    for(n=0;n<3000;n++) {
        r=pe_hd_cache_acquire(c,t,h);
        if(r!=PE_HD_CACHE_PENDING && r!=PE_HD_CACHE_BUSY) break;
        SDL_Delay(1);
    }
    return r;
}
static void release_until(pe_hd_cache *c, pe_hd_handle *h)
{
    unsigned n;
    pe_hd_cache_result r=PE_HD_CACHE_BUSY;
    for(n=0;n<3000 && r==PE_HD_CACHE_BUSY;n++) {
        r=pe_hd_cache_release(c,h);
        if(r==PE_HD_CACHE_BUSY) SDL_Delay(1);
    }
    assert(r==PE_HD_CACHE_READY && !h->rgba && !h->serial && !h->owner);
}
int main(void)
{
    pe_hd_cache_config cfg={0};
    pe_hd_cache *c;
    pe_hd_source s={synthetic_png,sizeof(synthetic_png),sizeof(synthetic_png),done,NULL};
    pe_hd_ticket t={0}, stale;
    pe_hd_handle h[PE_HD_CACHE_LEASES]={{0}}, extra={0}, duplicate;
    unsigned i;
    cfg.decode_bytes=1024*1024;
    cfg.owned_encoded_bytes=4096;
    cfg.borrowed_encoded_bytes=4096;
    cfg.total_bytes=pe_hd_cache_metadata_bytes()+cfg.decode_bytes+8192;
    { pe_hd_cache_config bad=cfg; --bad.total_bytes; assert(!pe_hd_cache_create(&bad)); }
    c=pe_hd_cache_create(&cfg); assert(c);
    { pe_hd_source bad=s; bad.length=32;
      assert(pe_hd_cache_submit(c,1,&bad,0,&t)==PE_HD_CACHE_INVALID); }
    assert(SDL_GetAtomicInt(&returned)==0);
    { pe_hd_source bad=s; bad.length=SIZE_MAX;
      assert(pe_hd_cache_submit(c,1,&bad,1,&t)==PE_HD_CACHE_INVALID); }
    { pe_hd_source bad=s; bad.retained_bytes=4097;
      assert(pe_hd_cache_submit(c,1,&bad,0,&t)==PE_HD_CACHE_FULL);
      bad.retained_bytes=1;
      assert(pe_hd_cache_submit(c,1,&bad,0,&t)==PE_HD_CACHE_INVALID); }
    assert(pe_hd_cache_submit(c,0,&s,0,&t)==PE_HD_CACHE_INVALID);
    assert(pe_hd_cache_submit(c,1,&s,0,&t)==PE_HD_CACHE_PENDING);
    assert(acquire_until(c,t,&h[0])==PE_HD_CACHE_READY);
    assert(SDL_GetAtomicInt(&returned)==1);
    assert(h[0].bytes==4 && h[0].width==1 && h[0].height==1);
    assert(h[0].rgba[0]==12 && h[0].rgba[3]==78);
    for(i=1;i<PE_HD_CACHE_LEASES;i++) assert(acquire_until(c,t,&h[i])==PE_HD_CACHE_READY);
    assert(acquire_until(c,t,&extra)==PE_HD_CACHE_FULL);
    assert(pe_hd_cache_generation(c,2)==PE_HD_CACHE_READY);
    assert(acquire_until(c,t,&extra)==PE_HD_CACHE_INVALID);
    assert(h[0].rgba[1]==34); /* cancellation cannot free leased pixels */
    duplicate=h[0]; release_until(c,&h[0]);
    { pe_hd_cache_result r=PE_HD_CACHE_BUSY;
      unsigned n;
      for(n=0;n<3000 && r==PE_HD_CACHE_BUSY;n++) {
          r=pe_hd_cache_release(c,&duplicate);
          if(r==PE_HD_CACHE_BUSY) SDL_Delay(1);
      }
      assert(r==PE_HD_CACHE_INVALID); }
    assert(pe_hd_cache_submit(c,2,&s,1,&t)==PE_HD_CACHE_PENDING);
    /* A copied input never invokes provider done; stale work still cleans up. */
    stale=t;
    assert(pe_hd_cache_generation(c,3)==PE_HD_CACHE_READY);
    assert(acquire_until(c,stale,&extra)==PE_HD_CACHE_INVALID);
    assert(pe_hd_cache_close(c)==PE_HD_CACHE_BUSY);
    assert(pe_hd_cache_submit(c,3,&s,0,&t)==PE_HD_CACHE_CLOSED);
    assert(h[1].rgba[2]==56); /* BUSY close retains context/pixels/worker */
    for(i=1;i<PE_HD_CACHE_LEASES;i++) release_until(c,&h[i]);
    assert(pe_hd_cache_close(c)==PE_HD_CACHE_CLOSED);
    assert(SDL_GetAtomicInt(&returned)==1);
    /* Actual decoder pressure returns fallback and closes cleanly. */
    cfg.decode_bytes=1; cfg.total_bytes=pe_hd_cache_metadata_bytes()+8193;
    c=pe_hd_cache_create(&cfg); assert(c);
    assert(pe_hd_cache_submit(c,1,&s,1,&t)==PE_HD_CACHE_PENDING);
    assert(acquire_until(c,t,&extra)==PE_HD_CACHE_INVALID);
    assert(pe_hd_cache_close(c)==PE_HD_CACHE_CLOSED);
    /* Seventeen tiny entries exercise worker LRU slot reuse and stale serial
     * rejection without a stress loop or any large allocations. */
    {
        pe_hd_ticket first={0};
        cfg.decode_bytes=1024*1024;
        cfg.total_bytes=pe_hd_cache_metadata_bytes()+cfg.decode_bytes+8192;
        c=pe_hd_cache_create(&cfg); assert(c);
        for(i=0;i<=PE_HD_CACHE_SLOTS;i++) {
            unsigned n;
            pe_hd_cache_result r=PE_HD_CACHE_FULL;
            for(n=0;n<3000 && r==PE_HD_CACHE_FULL;n++) {
                r=pe_hd_cache_submit(c,1,&s,1,&t);
                if(r==PE_HD_CACHE_FULL) SDL_Delay(1);
            }
            assert(r==PE_HD_CACHE_PENDING);
            if(!i) first=t;
            assert(acquire_until(c,t,&extra)==PE_HD_CACHE_READY);
            release_until(c,&extra);
        }
        assert(acquire_until(c,first,&extra)==PE_HD_CACHE_INVALID);
        assert(pe_hd_cache_close(c)==PE_HD_CACHE_CLOSED);
    }
    /* Deterministic finite queue bound: one worker parked in a bounded test
     * provider callback, fifteen queued pins, then FULL with no ownership change.
     * Generation cancellation + join must return all sixteen accepted pins. */
    {
        gate g={SDL_CreateSemaphore(0),SDL_CreateSemaphore(0)};
        pe_hd_ticket unchanged={123,7};
        assert(g.entered && g.proceed);
        SDL_SetAtomicInt(&returned,0);
        cfg.decode_bytes=1024*1024;
        cfg.borrowed_encoded_bytes=PE_HD_CACHE_SLOTS*sizeof(synthetic_png);
        cfg.total_bytes=pe_hd_cache_metadata_bytes()+cfg.decode_bytes+
            cfg.owned_encoded_bytes+cfg.borrowed_encoded_bytes;
        c=pe_hd_cache_create(&cfg); assert(c);
        s.done=gated_done; s.user=&g;
        assert(pe_hd_cache_submit(c,1,&s,0,&t)==PE_HD_CACHE_PENDING);
        assert(SDL_WaitSemaphoreTimeout(g.entered,3000));
        for(i=1;i<PE_HD_CACHE_SLOTS;i++)
            assert(pe_hd_cache_submit(c,1,&s,0,&t)==PE_HD_CACHE_PENDING);
        assert(pe_hd_cache_submit(c,1,&s,0,&unchanged)==PE_HD_CACHE_FULL);
        assert(unchanged.serial==123 && unchanged.slot==7);
        assert(pe_hd_cache_generation(c,2)==PE_HD_CACHE_READY);
        SDL_SignalSemaphore(g.proceed);
        assert(pe_hd_cache_close(c)==PE_HD_CACHE_CLOSED);
        assert(SDL_GetAtomicInt(&returned)==PE_HD_CACHE_SLOTS);
        SDL_DestroySemaphore(g.entered); SDL_DestroySemaphore(g.proceed);
    }
    return 0;
}


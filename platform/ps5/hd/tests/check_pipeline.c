#include "pe_hd_pipeline.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "synthetic_png.h"
static SDL_AtomicInt reads, releases, mode;
static SDL_Semaphore *entered,*proceed;
static SDL_ThreadID owner;
static void done(void *u) { (void)u; assert(SDL_GetCurrentThreadID()!=owner); SDL_AddAtomicInt(&releases,1); }
static bool read_bytes(void *u,const char *path,size_t limit,pe_hd_index_bytes *out) {
 (void)u; assert(strstr(path,"texpage-")); assert(SDL_GetCurrentThreadID()!=owner);
 SDL_AddAtomicInt(&reads,1);
 int m=SDL_GetAtomicInt(&mode);
 if(m==3) { SDL_SignalSemaphore(entered); assert(SDL_WaitSemaphoreTimeout(proceed,3000)); }
 if(m==1) return false;
 assert(limit>=sizeof(synthetic_png));
 *out=(pe_hd_index_bytes){synthetic_png,m==2?32:sizeof(synthetic_png),sizeof(synthetic_png),done,NULL}; return true;
}
static pe_hd_pipeline_result poll_until(pe_hd_pipeline *p,pe_hd_request t,pe_hd_handle *h) {
 pe_hd_pipeline_result r=PE_HD_PIPELINE_PENDING;
 for(unsigned i=0;i<3000;i++) { r=pe_hd_pipeline_poll(p,t,h); if(r!=PE_HD_PIPELINE_PENDING && r!=PE_HD_PIPELINE_BUSY) break; SDL_Delay(1); } return r;
}
static void release_until(pe_hd_pipeline *p,pe_hd_handle *h) {
 pe_hd_pipeline_result r=PE_HD_PIPELINE_BUSY;
 for(unsigned i=0;i<3000 && r==PE_HD_PIPELINE_BUSY;i++) { r=pe_hd_pipeline_release(p,h); if(r==PE_HD_PIPELINE_BUSY) SDL_Delay(1); }
 assert(r==PE_HD_PIPELINE_READY && !h->rgba);
}
int main(void) {
 const char *name="texpage-P4-0123456789abcdef-0000000000000000-64x64-0-0-32x32-P0-15.png";
 pe_hd_index_record rec={name,strlen(name)};
 pe_hd_pipeline_config cfg={{64,4096,65536,4096},{0,1024*1024,0,4096}};
 cfg.cache.total_bytes=pe_hd_cache_metadata_bytes()+cfg.cache.decode_bytes+4096;
 pe_hd_pipeline *p=NULL; pe_hd_request t,stale,slots[PE_HD_PIPELINE_REQUESTS]; pe_hd_handle h={0},extra={0}; PeHdFilename key,missing;
 assert(PeHd_ParseFilename(name,strlen(name),&key)); missing=key; missing.texture_hash++;
 owner=SDL_GetCurrentThreadID(); entered=SDL_CreateSemaphore(0); proceed=SDL_CreateSemaphore(0); assert(entered && proceed);
 assert(pe_hd_pipeline_create(&cfg,&rec,1,read_bytes,NULL,&p)==PE_HD_PIPELINE_READY);
 assert(pe_hd_pipeline_request(p,0,&key,&t)==PE_HD_PIPELINE_INVALID);
 assert(pe_hd_pipeline_request(p,1,&key,&t)==PE_HD_PIPELINE_PENDING);
 assert(poll_until(p,t,&h)==PE_HD_PIPELINE_READY && h.width==1 && h.height==1 && h.bytes==4);
 assert(h.rgba[0]==12 && h.rgba[1]==34 && h.rgba[2]==56 && h.rgba[3]==78);
 assert(poll_until(p,t,&extra)==PE_HD_PIPELINE_READY); release_until(p,&extra);
 stale=t; assert(pe_hd_pipeline_generation(p,2)==PE_HD_PIPELINE_READY);
 assert(poll_until(p,stale,&extra)==PE_HD_PIPELINE_FALLBACK);
 assert(h.rgba[0]==12); release_until(p,&h);
 assert(pe_hd_pipeline_request(p,2,&missing,&t)==PE_HD_PIPELINE_PENDING);
 assert(poll_until(p,t,&h)==PE_HD_PIPELINE_FALLBACK); assert(pe_hd_pipeline_forget(p,t)==PE_HD_PIPELINE_READY);
 for(int m=1;m<=2;m++) {
  SDL_SetAtomicInt(&mode,m); assert(pe_hd_pipeline_request(p,2,&key,&t)==PE_HD_PIPELINE_PENDING);
  assert(poll_until(p,t,&h)==PE_HD_PIPELINE_FALLBACK); assert(pe_hd_pipeline_forget(p,t)==PE_HD_PIPELINE_READY);
 }
 SDL_SetAtomicInt(&mode,3); assert(pe_hd_pipeline_request(p,2,&key,&t)==PE_HD_PIPELINE_PENDING);
 assert(SDL_WaitSemaphoreTimeout(entered,3000));
 /* While read blocks, queue remains bounded and poll cannot wait on I/O. */
 for(unsigned i=0;i<PE_HD_PIPELINE_REQUESTS-1;i++) assert(pe_hd_pipeline_request(p,2,&key,&slots[i])==PE_HD_PIPELINE_PENDING);
 assert(pe_hd_pipeline_request(p,2,&key,&slots[15])==PE_HD_PIPELINE_FULL);
 assert(pe_hd_pipeline_poll(p,t,&h)==PE_HD_PIPELINE_PENDING);
 assert(pe_hd_pipeline_forget(p,t)==PE_HD_PIPELINE_READY);
 assert(pe_hd_pipeline_generation(p,3)==PE_HD_PIPELINE_READY);
 SDL_SetAtomicInt(&mode,0); SDL_SignalSemaphore(proceed);
 assert(pe_hd_pipeline_request(p,3,&key,&t)==PE_HD_PIPELINE_PENDING);
 assert(poll_until(p,t,&h)==PE_HD_PIPELINE_READY);
 assert(pe_hd_pipeline_close(p)==PE_HD_PIPELINE_BUSY);
 assert(pe_hd_pipeline_request(p,3,&key,&stale)==PE_HD_PIPELINE_CLOSED);
 release_until(p,&h); assert(pe_hd_pipeline_close(p)==PE_HD_PIPELINE_CLOSED);
 assert(SDL_GetAtomicInt(&releases)==SDL_GetAtomicInt(&reads)-1);
 /* Borrowed cache budget rejection must return provider pin exactly once. */
 cfg.cache.borrowed_encoded_bytes=1;
 assert(pe_hd_pipeline_create(&cfg,&rec,1,read_bytes,NULL,&p)==PE_HD_PIPELINE_READY);
 int before=SDL_GetAtomicInt(&releases);
 assert(pe_hd_pipeline_request(p,1,&key,&t)==PE_HD_PIPELINE_PENDING);
 assert(poll_until(p,t,&h)==PE_HD_PIPELINE_FALLBACK);
 assert(SDL_GetAtomicInt(&releases)==before+1);
 assert(pe_hd_pipeline_release(p,&h)==PE_HD_PIPELINE_INVALID);
 assert(pe_hd_pipeline_close(p)==PE_HD_PIPELINE_CLOSED);
 SDL_DestroySemaphore(entered); SDL_DestroySemaphore(proceed);
 puts("PASS pipeline real SDL workers: decode, leases, fallback, worker reads/releases, bounded queue, cancellation, generation, close/retry"); return 0;
}



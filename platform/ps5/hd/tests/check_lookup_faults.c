/* Deterministic pipeline boundary faults; real workers tested separately. */
#include "pe_hd_lookup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned requests,forgets,leases,released,busy,dimension=1;
static uint64_t scene=1;
static pe_hd_pipeline_result poll_result=PE_HD_PIPELINE_READY;
static unsigned char pixel[4];
pe_hd_pipeline_result pe_hd_pipeline_request(pe_hd_pipeline *p,uint64_t g,const PeHdFilename *k,pe_hd_request *out) {
    (void)p;(void)k; assert(g==scene); *out=(pe_hd_request){++requests,0}; return PE_HD_PIPELINE_PENDING;
}
pe_hd_pipeline_result pe_hd_pipeline_poll(pe_hd_pipeline *p,pe_hd_request t,pe_hd_handle *h) {
    (void)p;(void)t;
    if(poll_result!=PE_HD_PIPELINE_READY) return poll_result;
    *h=(pe_hd_handle){pixel,dimension,dimension,4,++leases,0,(const pe_hd_cache *)pixel};
    return PE_HD_PIPELINE_READY;
}
pe_hd_pipeline_result pe_hd_pipeline_release(pe_hd_pipeline *p,pe_hd_handle *h) {
    (void)p; assert(h->owner);
    if(busy) { busy--; return PE_HD_PIPELINE_BUSY; }
    released++; memset(h,0,sizeof(*h)); return PE_HD_PIPELINE_READY;
}
pe_hd_pipeline_result pe_hd_pipeline_forget(pe_hd_pipeline *p,pe_hd_request t) {
    (void)p;(void)t; forgets++; return PE_HD_PIPELINE_READY;
}
pe_hd_pipeline_result pe_hd_pipeline_generation(pe_hd_pipeline *p,uint64_t next) {
    (void)p; assert(next>scene); scene=next; return PE_HD_PIPELINE_READY;
}
int main(void) {
    pe_hd_pipeline *p=(pe_hd_pipeline *)pixel; pe_hd_lookup *c=NULL;
    PeHdFilename key={PE_HD_NAME_P4,1,2,1,1,0,0,1,1,0,15};
    pe_hd_lookup_observation o={key,1,1,0};
    pe_hd_generation tracker; pe_hd_generation_init(&tracker);
    pe_hd_lookup_proof proof={&tracker,1,1,1,{{0,0,1,1,PE_HD_GENERATION_CLIP}}};
    pe_hd_lookup_token t={0},other={0},slots[PE_HD_LOOKUP_SLOTS]; pe_hd_handle h={0};
    assert(pe_hd_lookup_create(p,&c)==PE_HD_LOOKUP_READY);
    /* Every unsupported form leaves token/output untouched and never requests. */
    for(unsigned i=0;i<8;i++) {
        pe_hd_lookup_observation bad=o; PeHdFilename k=key;
        switch(i) {
        case 0: bad.unsupported=1; break;
        case 1: k.mode=bad.texture.mode=PE_HD_NAME_STP4; break;
        case 2: k.x=bad.texture.x=1; break;
        case 3: k.palette_last=bad.texture.palette_last=14; break;
        case 4: bad.texture.source_width=2; break;
        case 5: bad.output_width=2; break;
        case 6: bad.output_width=bad.output_height=8193; break;
        default: k.source_width=bad.texture.source_width=0; break;
        }
        assert(pe_hd_lookup_prepare(c,&k,&bad,&proof,&t)==PE_HD_LOOKUP_FALLBACK && !t.serial);
    }
    proof.dependencies[0].x=1024;
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_FALLBACK);
    proof.dependencies[0].x=0; proof.count=5;
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_FALLBACK);
    proof.count=1; assert(!requests);
    /* Fieldwise coalescing ignores observation hashes and struct padding. */
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    o.texture.texture_hash=999;
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&other)==PE_HD_LOOKUP_PENDING);
    assert(requests==1 && t.serial==other.serial);
    poll_result=PE_HD_PIPELINE_BUSY;
    assert(pe_hd_lookup_acquire(c,t,1,&h)==PE_HD_LOOKUP_BUSY && !h.owner);
    poll_result=PE_HD_PIPELINE_FALLBACK;
    assert(pe_hd_lookup_acquire(c,t,1,&h)==PE_HD_LOOKUP_FALLBACK);
    poll_result=PE_HD_PIPELINE_READY;
    assert(pe_hd_lookup_acquire(c,t,1,&h)==PE_HD_LOOKUP_FALLBACK && !leases);
    assert(pe_hd_lookup_retire(c,t)==PE_HD_LOOKUP_READY);
    /* Hidden mismatch lease survives BUSY and is not overwritten/released twice. */
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    dimension=2; busy=3;
    assert(pe_hd_lookup_acquire(c,t,1,&h)==PE_HD_LOOKUP_FALLBACK && !h.owner);
    assert(pe_hd_lookup_generation(c,2)==PE_HD_LOOKUP_BUSY && scene==1);
    assert(pe_hd_lookup_destroy(c)==PE_HD_LOOKUP_BUSY);
    assert(pe_hd_lookup_generation(c,2)==PE_HD_LOOKUP_READY && released==1);
    assert(pe_hd_lookup_acquire(c,t,1,&h)==PE_HD_LOOKUP_FALLBACK);
    dimension=1;
    /* Unrelated tiles preserve proof; source writes invalidate it permanently. */
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    pe_hd_generation_rect write={64,0,1,1,PE_HD_GENERATION_CLIP};
    assert(pe_hd_generation_touch(&tracker,&write)==PE_HD_GENERATION_OK);
    assert(pe_hd_lookup_acquire(c,t,1,&h)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_release(c,&h)==PE_HD_LOOKUP_READY);
    write.x=0; assert(pe_hd_generation_touch(&tracker,&write)==PE_HD_GENERATION_OK);
    assert(pe_hd_lookup_acquire(c,t,1,&h)==PE_HD_LOOKUP_FALLBACK);
    assert(pe_hd_lookup_retire(c,t)==PE_HD_LOOKUP_READY);
    proof.checkpoint=tracker.epoch;
    /* P8 full palette accepted; a partial palette is unsupported. */
    key.mode=o.texture.mode=PE_HD_NAME_P8;
    key.palette_last=o.texture.palette_last=255;
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&t)==PE_HD_LOOKUP_PENDING);
    assert(pe_hd_lookup_retire(c,t)==PE_HD_LOOKUP_READY);
    /* Fixed capacity, separate keys; explicit generation cancels all identities. */
    for(unsigned i=0;i<PE_HD_LOOKUP_SLOTS;i++) {
        key.texture_hash=i+1;
        assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&slots[i])==PE_HD_LOOKUP_PENDING);
    }
    key.texture_hash=100;
    assert(pe_hd_lookup_prepare(c,&key,&o,&proof,&other)==PE_HD_LOOKUP_FULL);
    assert(pe_hd_lookup_acquire(c,slots[0],1,&h)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_generation(c,3)==PE_HD_LOOKUP_READY);
    assert(h.rgba==pixel && pe_hd_lookup_acquire(c,slots[0],1,&(pe_hd_handle){0})==PE_HD_LOOKUP_FALLBACK);
    busy=1; assert(pe_hd_lookup_release(c,&h)==PE_HD_LOOKUP_BUSY && h.owner);
    assert(pe_hd_lookup_release(c,&h)==PE_HD_LOOKUP_READY && !h.owner);
    assert(pe_hd_lookup_generation(c,3)==PE_HD_LOOKUP_INVALID);
    assert(pe_hd_lookup_generation(c,UINT64_MAX)==PE_HD_LOOKUP_READY);
    assert(pe_hd_lookup_generation(c,1)==PE_HD_LOOKUP_INVALID);
    assert(pe_hd_lookup_destroy(c)==PE_HD_LOOKUP_READY && released==leases);
    puts("PASS lookup fault boundary: unsupported/bounds, coalescing, terminal failures, BUSY hidden lease, capacity, generation, rollover");
    return 0;
}


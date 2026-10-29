#include "pe_hd_lookup.h"
#include <stdlib.h>
#include <string.h>

typedef struct lookup_slot {
    uint64_t serial;
    PeHdFilename key;
    pe_hd_lookup_observation observed;
    pe_hd_lookup_proof proof;
    pe_hd_request request;
    pe_hd_handle rejected;
    bool failed;
} lookup_slot;
struct pe_hd_lookup {
    pe_hd_pipeline *pipeline;
    uint64_t generation, serial;
    lookup_slot slots[PE_HD_LOOKUP_SLOTS];
};
static pe_hd_lookup_result mapped(pe_hd_pipeline_result r)
{
    switch(r) {
    case PE_HD_PIPELINE_READY: return PE_HD_LOOKUP_READY;
    case PE_HD_PIPELINE_PENDING: return PE_HD_LOOKUP_PENDING;
    case PE_HD_PIPELINE_BUSY: return PE_HD_LOOKUP_BUSY;
    case PE_HD_PIPELINE_FULL: return PE_HD_LOOKUP_FULL;
    default: return PE_HD_LOOKUP_FALLBACK;
    }
}
static bool live(const pe_hd_lookup_proof *p)
{
    if(!p->tracker || !p->state || !p->count || p->count>PE_HD_LOOKUP_DEPENDENCIES)
        return false;
    for(unsigned i=0;i<p->count;i++) {
        bool changed;
        const pe_hd_generation_rect *r=&p->dependencies[i];
        /* A clipped empty rectangle is valid to the tracker but cannot prove
         * any texture/CLUT coverage for a lookup. Avoid signed overflow. */
        if(r->mode==PE_HD_GENERATION_CLIP &&
            ((int64_t)r->x+r->width<=0 || (int64_t)r->y+r->height<=0 ||
             r->x>=1024 || r->y>=512)) return false;
        if(pe_hd_generation_changed(p->tracker,&p->dependencies[i],p->checkpoint,
            &changed)!=PE_HD_GENERATION_OK || changed) return false;
    }
    return true;
}
static bool observation_equal(const pe_hd_lookup_observation *a,
    const pe_hd_lookup_observation *b)
{
    PeHdFilename x=a->texture,y=b->texture;
    x.texture_hash=x.palette_hash=y.texture_hash=y.palette_hash=0;
    return pe_hd_index_key_equal(&x,&y) && a->output_width==b->output_width &&
        a->output_height==b->output_height && a->unsupported==b->unsupported;
}
static bool supported(const PeHdFilename *key,const pe_hd_lookup_observation *o)
{
    pe_hd_lookup_observation expected=*o;
    expected.texture=*key;
    if(!observation_equal(o,&expected) || o->unsupported ||
        (key->mode!=PE_HD_NAME_P4 && key->mode!=PE_HD_NAME_P8) ||
        key->source_width!=(key->mode==PE_HD_NAME_P4?64:128) ||
        key->source_height!=256 || key->x || key->y ||
        key->width!=256 || key->height!=256 ||
        key->palette_first || key->palette_last!=(key->mode==PE_HD_NAME_P4?15:255) ||
        !o->output_width || !o->output_height ||
        o->output_width>PE_HD_MAX_DIMENSION || o->output_height>PE_HD_MAX_DIMENSION)
        return false;
    return o->output_width%key->width==0 && o->output_height%key->height==0 &&
        o->output_width/key->width==o->output_height/key->height &&
        (uint64_t)o->output_width*o->output_height*4<=PE_HD_MAX_IMAGE_BYTES;
}
static bool proof_equal(const pe_hd_lookup_proof *a,const pe_hd_lookup_proof *b)
{
    if(a->tracker!=b->tracker || a->checkpoint!=b->checkpoint ||
        a->state!=b->state || a->count!=b->count) return false;
    for(unsigned i=0;i<a->count;i++) {
        const pe_hd_generation_rect *x=&a->dependencies[i],*y=&b->dependencies[i];
        if(x->x!=y->x || x->y!=y->y || x->width!=y->width ||
            x->height!=y->height || x->mode!=y->mode) return false;
    }
    return true;
}
static lookup_slot *find(pe_hd_lookup *c,pe_hd_lookup_token t)
{
    return t.serial && t.slot<PE_HD_LOOKUP_SLOTS &&
        c->slots[t.slot].serial==t.serial?&c->slots[t.slot]:NULL;
}
size_t pe_hd_lookup_metadata_bytes(void) { return sizeof(pe_hd_lookup); }
pe_hd_lookup_result pe_hd_lookup_create(pe_hd_pipeline *p,pe_hd_lookup **out)
{
    if(!p || !out) return PE_HD_LOOKUP_INVALID;
    pe_hd_lookup *c=calloc(1,sizeof(*c));
    if(!c) return PE_HD_LOOKUP_FULL;
    c->pipeline=p; c->generation=1; *out=c; return PE_HD_LOOKUP_READY;
}
pe_hd_lookup_result pe_hd_lookup_prepare(pe_hd_lookup *c,const PeHdFilename *key,
    const pe_hd_lookup_observation *o,const pe_hd_lookup_proof *p,pe_hd_lookup_token *out)
{
    unsigned free_slot=PE_HD_LOOKUP_SLOTS;
    pe_hd_request request;
    if(!c || !key || !o || !p || !out) return PE_HD_LOOKUP_INVALID;
    if(!supported(key,o) || !live(p)) return PE_HD_LOOKUP_FALLBACK;
    for(unsigned i=0;i<PE_HD_LOOKUP_SLOTS;i++) {
        lookup_slot *s=&c->slots[i];
        if(!s->serial) { if(free_slot==PE_HD_LOOKUP_SLOTS) free_slot=i; continue; }
        if(pe_hd_index_key_equal(key,&s->key) && observation_equal(o,&s->observed) &&
            proof_equal(p,&s->proof)) {
            *out=(pe_hd_lookup_token){s->serial,i};
            return s->failed?PE_HD_LOOKUP_FALLBACK:PE_HD_LOOKUP_PENDING;
        }
    }
    if(free_slot==PE_HD_LOOKUP_SLOTS || c->serial==UINT64_MAX) return PE_HD_LOOKUP_FULL;
    pe_hd_pipeline_result r=pe_hd_pipeline_request(c->pipeline,c->generation,key,&request);
    if(r!=PE_HD_PIPELINE_PENDING) return mapped(r);
    lookup_slot *s=&c->slots[free_slot];
    s->serial=++c->serial; s->key=*key; s->observed=*o; s->proof=*p; s->request=request;
    *out=(pe_hd_lookup_token){s->serial,free_slot}; return PE_HD_LOOKUP_PENDING;
}
pe_hd_lookup_result pe_hd_lookup_acquire(pe_hd_lookup *c,pe_hd_lookup_token t,
    uint64_t state,pe_hd_handle *out)
{
    if(!c || !out || out->rgba || out->serial || out->owner) return PE_HD_LOOKUP_INVALID;
    lookup_slot *s=find(c,t);
    if(!s || s->failed) return PE_HD_LOOKUP_FALLBACK;
    if(state!=s->proof.state || !live(&s->proof)) {
        s->failed=true; return PE_HD_LOOKUP_FALLBACK;
    }
    pe_hd_handle h={0};
    pe_hd_pipeline_result r=pe_hd_pipeline_poll(c->pipeline,s->request,&h);
    if(r==PE_HD_PIPELINE_READY) {
        if(h.width!=s->observed.output_width || h.height!=s->observed.output_height ||
            h.bytes!=(size_t)h.width*h.height*4) {
            /* Keep ownership even when a try-release is BUSY. Drain in staging. */
            s->rejected=h; s->failed=true;
            (void)pe_hd_pipeline_release(c->pipeline,&s->rejected);
            return PE_HD_LOOKUP_FALLBACK;
        }
        *out=h;
    } else if(r!=PE_HD_PIPELINE_PENDING && r!=PE_HD_PIPELINE_BUSY) s->failed=true;
    return mapped(r);
}
pe_hd_lookup_result pe_hd_lookup_release(pe_hd_lookup *c,pe_hd_handle *h)
{
    if(!c || !h) return PE_HD_LOOKUP_INVALID;
    pe_hd_pipeline_result r=pe_hd_pipeline_release(c->pipeline,h);
    return r==PE_HD_PIPELINE_INVALID?PE_HD_LOOKUP_INVALID:mapped(r);
}
static pe_hd_lookup_result drain(pe_hd_lookup *c,lookup_slot *s)
{
    if(!s->rejected.owner) return PE_HD_LOOKUP_READY;
    pe_hd_pipeline_result r=pe_hd_pipeline_release(c->pipeline,&s->rejected);
    return r==PE_HD_PIPELINE_READY?PE_HD_LOOKUP_READY:
        r==PE_HD_PIPELINE_BUSY?PE_HD_LOOKUP_BUSY:PE_HD_LOOKUP_INVALID;
}
pe_hd_lookup_result pe_hd_lookup_retire(pe_hd_lookup *c,pe_hd_lookup_token t)
{
    if(!c) return PE_HD_LOOKUP_INVALID;
    lookup_slot *s=find(c,t);
    if(!s) return PE_HD_LOOKUP_INVALID;
    pe_hd_lookup_result d=drain(c,s);
    if(d!=PE_HD_LOOKUP_READY) return d;
    pe_hd_pipeline_result r=pe_hd_pipeline_forget(c->pipeline,s->request);
    if(r!=PE_HD_PIPELINE_READY) return PE_HD_LOOKUP_INVALID;
    memset(s,0,sizeof(*s)); return PE_HD_LOOKUP_READY;
}
pe_hd_lookup_result pe_hd_lookup_generation(pe_hd_lookup *c,uint64_t next)
{
    if(!c || next<=c->generation) return PE_HD_LOOKUP_INVALID;
    for(unsigned i=0;i<PE_HD_LOOKUP_SLOTS;i++) {
        pe_hd_lookup_result d=drain(c,&c->slots[i]);
        if(d!=PE_HD_LOOKUP_READY) return d;
    }
    if(pe_hd_pipeline_generation(c->pipeline,next)!=PE_HD_PIPELINE_READY)
        return PE_HD_LOOKUP_INVALID;
    memset(c->slots,0,sizeof(c->slots)); c->generation=next; return PE_HD_LOOKUP_READY;
}
pe_hd_lookup_result pe_hd_lookup_destroy(pe_hd_lookup *c)
{
    if(!c) return PE_HD_LOOKUP_INVALID;
    for(unsigned i=0;i<PE_HD_LOOKUP_SLOTS;i++) if(c->slots[i].serial) {
        pe_hd_lookup_result r=pe_hd_lookup_retire(c,
            (pe_hd_lookup_token){c->slots[i].serial,i});
        if(r!=PE_HD_LOOKUP_READY) return r;
    }
    free(c); return PE_HD_LOOKUP_READY;
}


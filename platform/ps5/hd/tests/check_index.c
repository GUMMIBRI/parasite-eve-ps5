#include "pe_hd_index.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
static SDL_AtomicInt releases; static unsigned reads;
static int behavior;
static unsigned char data[16];
static void release(void *u) { assert(u == data); SDL_AddAtomicInt(&releases,1); }
static bool read_bytes(void *u, const char *path, size_t limit, pe_hd_index_bytes *out) {
    assert(u == data); assert(strstr(path,"texpage-") != NULL); ++reads;
    if (behavior == 1) return false;
    assert(limit > 0);
    *out = (pe_hd_index_bytes){data, 8, 16, release, data};
    if (behavior == 2) out->length = 17;
    if (behavior == 3) out->retained_bytes = limit + 1;
    return true;
}
static int worker(void *v) { pe_hd_source *s = v; s->done(s->user); return 0; }
static const char *base = "texpage-P4-0123456789abcdef-0000000000000000-64x64-0-0-32x32-P0-15.png";
int main(void) {
    pe_hd_index_config cfg = {64, 4096, 65536, 1024};
    pe_hd_index_record records[2];
    pe_hd_index *idx = NULL, *sentinel = (pe_hd_index *)(uintptr_t)1;
    PeHdFilename key, same, missing;
    pe_hd_source pins[PE_HD_INDEX_PINS], out;
    char path[1024], second[512];
    assert(PeHd_ParseFilename(base, strlen(base), &key));
    memset(&same, 0xa5, sizeof same);
    same.mode=key.mode; same.texture_hash=key.texture_hash; same.palette_hash=key.palette_hash;
    same.source_width=key.source_width; same.source_height=key.source_height;
    same.x=key.x; same.y=key.y; same.width=key.width; same.height=key.height;
    same.palette_first=key.palette_first; same.palette_last=key.palette_last;
    assert(pe_hd_index_key_equal(&key,&same));
#define DIFFER(field) do { same=key; ++same.field; assert(!pe_hd_index_key_equal(&key,&same)); } while(0)
    DIFFER(mode); DIFFER(texture_hash); DIFFER(palette_hash); DIFFER(source_width);
    DIFFER(source_height); DIFFER(x); DIFFER(y); DIFFER(width); DIFFER(height);
    DIFFER(palette_first); DIFFER(palette_last);
    records[0]=(pe_hd_index_record){base,strlen(base)};
    assert(pe_hd_index_create(&cfg, records, 1, read_bytes, data, &idx)==PE_HD_INDEX_OK);
    missing=key; ++missing.texture_hash;
    memset(&out,0x55,sizeof out); pe_hd_source old=out;
    assert(pe_hd_index_pin(idx,&missing,&out)==PE_HD_INDEX_MISSING);
    assert(memcmp(&old,&out,sizeof out)==0 && reads==0);
    for(unsigned i=0;i<PE_HD_INDEX_PINS;++i) assert(pe_hd_index_pin(idx,&key,&pins[i])==PE_HD_INDEX_OK);
    assert(pe_hd_index_pin(idx,&key,&out)==PE_HD_INDEX_BUSY);
    assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_BUSY);
    SDL_Thread *thread=SDL_CreateThread(worker,"index-release",&pins[0]); assert(thread);
    SDL_WaitThread(thread,NULL);
    assert(pe_hd_index_pin(idx,&key,&pins[0])==PE_HD_INDEX_OK);
    for(unsigned i=0;i<PE_HD_INDEX_PINS;++i) pins[i].done(pins[i].user);
        assert(SDL_GetAtomicInt(&releases)==33);
    for(unsigned n=0;n<200;++n) {
        assert(pe_hd_index_pin(idx,&key,&pins[0])==PE_HD_INDEX_OK);
        thread=SDL_CreateThread(worker,"index-release",&pins[0]); assert(thread);
        assert(pe_hd_index_pin(idx,&key,&pins[1])==PE_HD_INDEX_OK);
        SDL_WaitThread(thread,NULL);
        pins[1].done(pins[1].user);
    }
    assert(SDL_GetAtomicInt(&releases)==433); assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_OK);
    cfg.pinned_bytes=16;
    assert(pe_hd_index_create(&cfg, records, 1, read_bytes, data, &idx)==PE_HD_INDEX_OK);
    assert(pe_hd_index_pin(idx,&key,&pins[0])==PE_HD_INDEX_OK);
    assert(pe_hd_index_pin(idx,&key,&out)==PE_HD_INDEX_LIMIT);
    pins[0].done(pins[0].user);
    behavior=1; assert(pe_hd_index_pin(idx,&key,&out)==PE_HD_INDEX_READ);
    behavior=2; assert(pe_hd_index_pin(idx,&key,&out)==PE_HD_INDEX_INVALID);
    behavior=3; assert(pe_hd_index_pin(idx,&key,&out)==PE_HD_INDEX_LIMIT);
    assert(memcmp(&old,&out,sizeof out)==0);
    assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_OK); behavior=0;
    const char *bad[]={"/","../","x/../","./","x//","C:/","x\\","x./","x/./","a:/","x /","~/"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;++i) {
        snprintf(path,sizeof path,"%s%s",bad[i],base); records[0]=(pe_hd_index_record){path,strlen(path)};
        idx=sentinel; assert(pe_hd_index_create(&cfg,records,1,read_bytes,data,&idx)==PE_HD_INDEX_INVALID); assert(idx==sentinel);
    }
    snprintf(path,sizeof path,"safe/sub-dir_1/%s",base); records[0]=(pe_hd_index_record){path,strlen(path)};
    assert(pe_hd_index_create(&cfg,records,1,read_bytes,data,&idx)==PE_HD_INDEX_OK);
    memset(path,'x',strlen(path)); /* copied path and parsed key are immutable */
    assert(pe_hd_index_pin(idx,&key,&out)==PE_HD_INDEX_OK); out.done(out.user);
    assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_OK);
    records[0]=(pe_hd_index_record){base,strlen(base)};
    snprintf(second,sizeof second,"alias/%s",base); records[1]=(pe_hd_index_record){second,strlen(second)};
    idx=sentinel; assert(pe_hd_index_create(&cfg,records,2,read_bytes,data,&idx)==PE_HD_INDEX_DUPLICATE); assert(idx==sentinel);
    /* Textually distinct hex and decimal aliases also collide fieldwise. */
    records[1].path="texpage-P4-0123456789ABCDEF-0000000000000000-064x64-0-0-32x32-P0-15.png";
    records[1].length=strlen(records[1].path);
    assert(pe_hd_index_create(&cfg,records,2,read_bytes,data,&idx)==PE_HD_INDEX_DUPLICATE);
        /* Positive sorted lookup for a distinct second key in reverse input order. */
    records[1].path="texpage-P8-0000000000000000-0000000000000000-64x64-0-0-32x32-P0-15.png";
    records[1].length=strlen(records[1].path);
    assert(pe_hd_index_create(&cfg,records,2,read_bytes,data,&idx)==PE_HD_INDEX_OK);
    PeHdFilename key2; assert(PeHd_ParseFilename(records[1].path,records[1].length,&key2));
    assert(pe_hd_index_pin(idx,&key2,&out)==PE_HD_INDEX_OK); out.done(out.user);
    assert(pe_hd_index_pin(idx,&key,&out)==PE_HD_INDEX_OK); out.done(out.user);
    assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_OK);
    records[1].path=base; records[1].length=strlen(base);
    cfg.max_items=1; assert(pe_hd_index_create(&cfg,records,2,read_bytes,data,&idx)==PE_HD_INDEX_LIMIT);
    cfg.max_items=64; cfg.path_bytes=1; assert(pe_hd_index_create(&cfg,records,1,read_bytes,data,&idx)==PE_HD_INDEX_LIMIT);
    cfg.path_bytes=4096; cfg.storage_bytes=1; assert(pe_hd_index_create(&cfg,records,1,read_bytes,data,&idx)==PE_HD_INDEX_LIMIT);
    cfg.storage_bytes=65536;
    assert(pe_hd_index_create(&cfg,NULL,0,read_bytes,data,&idx)==PE_HD_INDEX_OK);
    assert(pe_hd_index_pin(idx,&key,&out)==PE_HD_INDEX_MISSING); assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_OK);
    puts("PASS: fields, manifest aliases, paths, immutable copies, bounded pins, worker release, read failures, budgets, empty index");
    return 0;
}





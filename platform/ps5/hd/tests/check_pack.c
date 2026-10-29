#include "pe_hd_pack.h"
#include "pe_hd_pipeline.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "synthetic_png.h"

static SDL_ThreadID owner;
typedef struct read_job {
    pe_hd_pack *pack;
    const char *path;
    size_t limit;
    pe_hd_index_bytes bytes;
    bool ok;
} read_job;
static int read_worker(void *u) {
    read_job *j = u;
    assert(SDL_GetCurrentThreadID() != owner);
    j->ok = pe_hd_pack_read(j->pack, j->path, j->limit, &j->bytes);
    return 0;
}
static bool read_one(pe_hd_pack *pack, const char *path, size_t limit, pe_hd_index_bytes *out) {
    read_job j = {pack, path, limit, {0}, false};
    SDL_Thread *t = SDL_CreateThread(read_worker, "pack-check-read", &j);
    assert(t); SDL_WaitThread(t, NULL); *out = j.bytes; return j.ok;
}
static int release_worker(void *u) {
    pe_hd_index_bytes *b = u; b->release(b->user); return 0;
}
static bool pipeline_read(void *u, const char *path, size_t limit, pe_hd_index_bytes *out) {
    assert(SDL_GetCurrentThreadID() != owner);
    return pe_hd_pack_read(u, path, limit, out);
}
static void write_fixture(const char *root, const char *name, const void *data, size_t size) {
    char path[2048];
    assert(SDL_snprintf(path, sizeof path, "%s/%s", root, name) < (int)sizeof path);
    FILE *f = fopen(path, "wb"); assert(f);
    assert(fwrite(data, 1, size, f) == size); assert(!fclose(f));
}
static void remove_fixture(const char *root, const char *name) {
    char path[2048]; SDL_snprintf(path, sizeof path, "%s/%s", root, name);
    assert(SDL_RemovePath(path));
}
int main(void) {
    char root[1024];
    char *cwd = SDL_GetCurrentDirectory(); assert(cwd);
    for (char *s = cwd; *s; ++s) if (*s == '\\') *s = '/';
    assert(SDL_snprintf(root, sizeof root, "%spack-synthetic-%llu", cwd,
        (unsigned long long)SDL_GetTicksNS()) < (int)sizeof root);
    SDL_free(cwd); assert(SDL_CreateDirectory(root)); owner = SDL_GetCurrentThreadID();
    const char *name = "texpage-P4-0123456789abcdef-0000000000000000-64x64-0-0-32x32-P0-15.png";
    write_fixture(root, name, synthetic_png, sizeof synthetic_png);
    write_fixture(root, "empty.png", "", 0);
    pe_hd_pack_config cfg = {root, 65536, PE_HD_PACK_POLICY_UNSET};
    pe_hd_pack *p = NULL;
    assert(pe_hd_pack_create(&cfg, &p) == PE_HD_PACK_INVALID && !p);
    cfg.policy = PE_HD_PACK_TRUSTED_IMMUTABLE_ROOT;
    cfg.pinned_bytes = 0;
    assert(pe_hd_pack_create(&cfg, &p) == PE_HD_PACK_LIMIT && !p);
    cfg.pinned_bytes = 65536;
    assert(pe_hd_pack_create(&cfg, &p) == PE_HD_PACK_OK);
    assert(pe_hd_pack_metadata_bytes() < 16384);
    pe_hd_index_bytes b = {0}, pins[PE_HD_PACK_PINS];
    const char *bad[] = {"../outside.png", "/absolute.png", "C:/absolute.png", "a/../b",
        "a//b", "a/./b", "a\\b", "a/", "a.", "", "missing.png", "empty.png"};
    for (size_t i=0; i<sizeof bad/sizeof bad[0]; ++i) {
        memset(&b, 0x55, sizeof b);
        assert(!read_one(p, bad[i], 65536, &b));
        assert(!b.data && !b.length && !b.retained_bytes && !b.release && !b.user);
    }
    assert(!read_one(p, name, sizeof synthetic_png-1, &b));
    char oversized_path[PE_HD_INDEX_MAX_PATH+1];
    memset(oversized_path,'a',sizeof oversized_path-1); oversized_path[sizeof oversized_path-1]=0;
    assert(!read_one(p, oversized_path, 65536, &b));
    /* A directory cannot be mistaken for encoded bytes. */
    char directory[1200]; SDL_snprintf(directory,sizeof directory,"%s/subdir",root);
    assert(SDL_CreateDirectory(directory));
    assert(!read_one(p,"subdir",65536,&b)); assert(SDL_RemovePath(directory));
    for (size_t i=0; i<PE_HD_PACK_PINS; ++i) {
        assert(read_one(p, name, 65536, &pins[i]));
        assert(pins[i].length == sizeof synthetic_png && pins[i].retained_bytes == sizeof synthetic_png);
        assert(!memcmp(pins[i].data, synthetic_png, sizeof synthetic_png));
    }
    assert(!read_one(p, name, 65536, &b));
    assert(pe_hd_pack_close(p) == PE_HD_PACK_BUSY);
    for (size_t i=0; i<PE_HD_PACK_PINS; ++i) {
        SDL_Thread *t=SDL_CreateThread(release_worker,"pack-check-release",&pins[i]);
        assert(t); SDL_WaitThread(t,NULL);
    }
    assert(pe_hd_pack_close(p) == PE_HD_PACK_OK);
    cfg.pinned_bytes = sizeof synthetic_png;
    assert(pe_hd_pack_create(&cfg,&p) == PE_HD_PACK_OK);
    assert(read_one(p,name,65536,&b));
    pe_hd_index_bytes other;
    assert(!read_one(p,name,65536,&other));
    b.release(b.user); assert(read_one(p,name,65536,&b)); b.release(b.user);
    /* Existing pipeline owns index; provider remains alive through final close. */
    pe_hd_pipeline_config pc={{4,1024,65536,65536},{0,1024*1024,0,65536}};
    pc.cache.total_bytes=pe_hd_cache_metadata_bytes()+pc.cache.decode_bytes+65536;
    pe_hd_index_record rec={name,strlen(name)};
    pe_hd_pipeline *pipe=NULL; pe_hd_request req; pe_hd_handle h={0}; PeHdFilename key;
    assert(PeHd_ParseFilename(name,strlen(name),&key));
    assert(pe_hd_pipeline_create(&pc,&rec,1,pipeline_read,p,&pipe)==PE_HD_PIPELINE_READY);
    assert(pe_hd_pipeline_request(pipe,1,&key,&req)==PE_HD_PIPELINE_PENDING);
    pe_hd_pipeline_result r=PE_HD_PIPELINE_PENDING;
    for(unsigned i=0;i<3000;i++) {
        r=pe_hd_pipeline_poll(pipe,req,&h);
        if(r!=PE_HD_PIPELINE_PENDING && r!=PE_HD_PIPELINE_BUSY) break;
        SDL_Delay(1);
    }
    assert(r==PE_HD_PIPELINE_READY && h.width==1 && h.rgba[0]==12);
    assert(pe_hd_pipeline_generation(pipe,2)==PE_HD_PIPELINE_READY);
    assert(pe_hd_pipeline_close(pipe)==PE_HD_PIPELINE_BUSY && h.rgba[0]==12);
    for(unsigned i=0;i<3000;i++) {
        r=pe_hd_pipeline_release(pipe,&h); if(r!=PE_HD_PIPELINE_BUSY) break; SDL_Delay(1);
    }
    assert(r==PE_HD_PIPELINE_READY);
    assert(pe_hd_pipeline_close(pipe)==PE_HD_PIPELINE_CLOSED);
    assert(pe_hd_pack_close(p)==PE_HD_PACK_OK);
    remove_fixture(root,name); remove_fixture(root,"empty.png"); assert(SDL_RemovePath(root));
    puts("PASS pack: path rejection, limits, 32 pins, worker release, BUSY close, real pipeline PNG/cancel/lease/close");
    return 0;
}


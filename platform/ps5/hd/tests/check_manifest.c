#include "pe_hd_manifest.h"
#include <assert.h>
#include <string.h>
#define NAME "texpage-P4-0000000000000001-0000000000000002-4x4-0-0-4x4-P0-15.png"
#define HEADER "PE-HD-MANIFEST\t1\n"
static unsigned reads, releases;
static void release(void *p) { (void)p; ++releases; }
static bool read_bytes(void *u,const char *p,size_t limit,pe_hd_index_bytes *o) {
    static const unsigned char b=42;
    (void)u; assert(!strcmp(p,"a/" NAME)); assert(limit); ++reads;
    *o=(pe_hd_index_bytes){&b,1,1,release,NULL}; return true;
}
static pe_hd_manifest_config config(void) {
    pe_hd_manifest_config c={4096,PE_HD_MANIFEST_MAX_LINE,4096,{16,4096,65536,64}};
    return c;
}
static void bad(const void *s,size_t n,pe_hd_manifest_config c,pe_hd_index_result expected) {
    pe_hd_index *out=(pe_hd_index *)(void *)&reads;
    assert(pe_hd_manifest_create(&c,s,n,read_bytes,NULL,&out)==expected);
    assert(out==(pe_hd_index *)(void *)&reads); assert(!reads);
}
int main(void) {
    const char valid[]=HEADER "file\ta/" NAME "\n";
    pe_hd_manifest_config c=config(); pe_hd_index *idx=NULL;
    PeHdFilename key; pe_hd_source source;
    for(size_t n=0;n<sizeof(valid)-1;++n) {
        /* Header alone is a complete, valid empty index. */
        if(n==strlen(HEADER)) continue;
        bad(valid,n,c,PE_HD_INDEX_INVALID);
    }
    bad(HEADER "file\t../" NAME "\n",strlen(HEADER "file\t../" NAME "\n"),c,PE_HD_INDEX_INVALID);
    bad(HEADER "file\t/" NAME "\n",strlen(HEADER "file\t/" NAME "\n"),c,PE_HD_INDEX_INVALID);
    bad(HEADER "file\ta/" NAME "\t7\n",strlen(HEADER "file\ta/" NAME "\t7\n"),c,PE_HD_INDEX_INVALID);
    bad(HEADER "file\ta/texpage-P4-0000000000000001-0000000000000002-65536x4-0-0-4x4-P0-15.png\n",
        strlen(HEADER "file\ta/texpage-P4-0000000000000001-0000000000000002-65536x4-0-0-4x4-P0-15.png\n"),c,PE_HD_INDEX_INVALID);
    const char duplicate[]=HEADER "file\ta/" NAME "\nfile\tb/texpage-P4-0000000000000001-0000000000000002-04x4-0-0-4x4-P0-15.png\n";
    bad(duplicate,sizeof(duplicate)-1,c,PE_HD_INDEX_DUPLICATE);
    char nul[sizeof(valid)]; memcpy(nul,valid,sizeof(valid)); nul[25]=0;
    bad(nul,sizeof(nul)-1,c,PE_HD_INDEX_INVALID);
    c.max_bytes=sizeof(valid)-2; bad(valid,sizeof(valid)-1,c,PE_HD_INDEX_LIMIT); c=config();
    c.max_line=16; bad(valid,sizeof(valid)-1,c,PE_HD_INDEX_LIMIT); c=config();
    c.scratch_bytes=1; bad(valid,sizeof(valid)-1,c,PE_HD_INDEX_LIMIT); c=config();
    c.index.path_bytes=2; bad(valid,sizeof(valid)-1,c,PE_HD_INDEX_LIMIT); c=config();
    c.index.max_items=1; bad(duplicate,sizeof(duplicate)-1,c,PE_HD_INDEX_LIMIT); c=config();
    c.index.storage_bytes=1; bad(valid,sizeof(valid)-1,c,PE_HD_INDEX_LIMIT); c=config();
    assert(pe_hd_manifest_create(&c,HEADER,strlen(HEADER),read_bytes,NULL,&idx)==PE_HD_INDEX_OK);
    assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_OK);
    char copy[sizeof(valid)]; memcpy(copy,valid,sizeof(valid));
    assert(pe_hd_manifest_create(&c,copy,sizeof(copy)-1,read_bytes,NULL,&idx)==PE_HD_INDEX_OK);
    memset(copy,0,sizeof(copy)); /* Index must own its path/key copy. */
    assert(!reads); assert(PeHd_ParseFilename(NAME,strlen(NAME),&key));
    assert(pe_hd_index_pin(idx,&key,&source)==PE_HD_INDEX_OK);
    assert(reads==1 && source.length==1);
    assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_BUSY);
    source.done(source.user); assert(releases==1);
    assert(pe_hd_index_destroy(idx)==PE_HD_INDEX_OK);
    return 0;
}




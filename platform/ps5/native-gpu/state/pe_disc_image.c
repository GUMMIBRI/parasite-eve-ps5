/* Native BIN/ISO checkpoint hashing; CHD remains a desktop-only reader. */
#include "pe1/cdrom.h"
#include <stdio.h>
#include <stdint.h>
int pe_disc_read_image(PeDisc *disc,uint64_t offset,void *data,size_t size){
    if(!disc || !disc->stream || disc->chd || (!data && size) || offset>disc->image_size || size>disc->image_size-offset || offset>INT64_MAX)return 0;
#ifdef _WIN32
    _lock_file(disc->stream);
    int ok=_fseeki64(disc->stream,(int64_t)offset,SEEK_SET)==0 && fread(data,1,size,disc->stream)==size;
    _unlock_file(disc->stream);
#else
    flockfile(disc->stream);
    int ok=fseeko(disc->stream,(off_t)offset,SEEK_SET)==0 && fread(data,1,size,disc->stream)==size;
    funlockfile(disc->stream);
#endif
    return ok;
}
FILE *Disc_Utf8File(const char *path,const char *mode){return path && mode ? fopen(path,mode):NULL;}

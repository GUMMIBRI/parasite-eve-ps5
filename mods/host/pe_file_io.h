#ifndef PE_FILE_IO_H
#define PE_FILE_IO_H
/* Private platform boundary for checked, staged profile writes. */
#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
static unsigned long pe_profile_process(void) { return GetCurrentProcessId(); }
static int pe_profile_target_ok(const char *p) {
    DWORD a = GetFileAttributesA(p);
    return a == INVALID_FILE_ATTRIBUTES || !(a & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY));
}
static int pe_profile_flush(FILE *f) { return !ferror(f) && !fflush(f) && !_commit(_fileno(f)); }
static int pe_file_publish_new(const char *a,const char *b){return MoveFileExA(a,b,MOVEFILE_WRITE_THROUGH)!=0;}
static int pe_profile_replace(const char *a, const char *b) {
    return MoveFileExA(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
#else
#include <unistd.h>
#include <sys/stat.h>
static unsigned long pe_profile_process(void) { return (unsigned long)getpid(); }
static int pe_profile_target_ok(const char *p) { struct stat s; return stat(p, &s) || S_ISREG(s.st_mode); }
static int pe_profile_flush(FILE *f) { return !ferror(f) && !fflush(f) && !fsync(fileno(f)); }
static int pe_profile_replace(const char *a, const char *b) { return !rename(a, b); }
#ifdef PE_PS5
/* USB filesystems need not support hard links. Reserve a new destination without
 * replacing anything; readers reject incomplete files by size/checksum. */
#include <fcntl.h>
static int pe_file_publish_new(const char *a,const char *b){
    FILE *src=fopen(a,"rb");if(!src)return 0;
    int fd=open(b,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0){fclose(src);return 0;}
    FILE *dst=fdopen(fd,"wb");if(!dst){close(fd);unlink(b);fclose(src);return 0;}
    unsigned char buf[16384];size_t n;int ok=1;
    while((n=fread(buf,1,sizeof buf,src))!=0)if(fwrite(buf,1,n,dst)!=n){ok=0;break;}
    if(ferror(src))ok=0; if(fclose(src))ok=0;
    if(!pe_profile_flush(dst))ok=0;
    if(fclose(dst))ok=0;
    if(!ok){unlink(b);return 0;}
    unlink(a);return 1;
}
#else
static int pe_file_publish_new(const char*a,const char*b){if(link(a,b))return 0;unlink(a);return 1;}
#endif
#endif
#endif

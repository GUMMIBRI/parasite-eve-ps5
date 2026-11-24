#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>
#include <assert.h>
#include <string.h>
#ifdef _WIN32
#define fsync _commit
#undef _WIN32
#endif
#define PE_PS5 1
#include "pe_file_io.h"
int main(int argc,char **argv){
 assert(argc==2);char a[1024],b[1024];snprintf(a,sizeof a,"%s/publish-src.tmp",argv[1]);snprintf(b,sizeof b,"%s/publish-dst.tmp",argv[1]);
 FILE *f=fopen(a,"wb");assert(f);assert(fwrite("checkpoint",1,10,f)==10);assert(fclose(f)==0);
 assert(pe_file_publish_new(a,b));assert(!fopen(a,"rb"));
 f=fopen(b,"rb");char text[11]={0};assert(fread(text,1,10,f)==10);fclose(f);assert(!strcmp(text,"checkpoint"));
 f=fopen(a,"wb");assert(f);fputs("replacement",f);fclose(f);assert(!pe_file_publish_new(a,b));
 f=fopen(b,"rb");assert(fread(text,1,10,f)==10);fclose(f);assert(!strcmp(text,"checkpoint"));
 assert(!pe_file_publish_new("this-input-does-not-exist",b));unlink(a);unlink(b);
 puts("PASS PS5 new-file publication and refusal to overwrite existing checkpoint");return 0;
}

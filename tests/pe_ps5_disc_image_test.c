#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pe1/cdrom.h"
int main(void){
    FILE *file=tmpfile();assert(file);assert(fwrite("0123456789",1,10,file)==10);fflush(file);
    PeDisc disc={0};disc.stream=file;disc.image_size=10;char out[5]={0};
    assert(pe_disc_read_image(&disc,3,out,4));assert(memcmp(out,"3456",4)==0);
    assert(pe_disc_read_image(&disc,0,out,2));assert(memcmp(out,"01",2)==0);
    assert(!pe_disc_read_image(&disc,9,out,2));assert(!pe_disc_read_image(&disc,UINT64_MAX,out,1));
    assert(!pe_disc_read_image(&disc,0,NULL,1));assert(!pe_disc_read_image(NULL,0,out,1));
    disc.chd=(struct PeChd *)1;assert(!pe_disc_read_image(&disc,0,out,1));disc.chd=NULL;
    disc.image_size=20;assert(!pe_disc_read_image(&disc,12,out,1));fclose(file);
    puts("PASS disc image offset reads, bounds, short-file, null and unsupported CHD refusal");return 0;
}

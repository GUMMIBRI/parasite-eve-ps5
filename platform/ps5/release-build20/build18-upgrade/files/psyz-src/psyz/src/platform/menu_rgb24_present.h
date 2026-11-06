/* SPDX-License-Identifier: MIT. Source-owned title companion and bounded raw art. */
#ifndef PE_MENU_RGB24_PRESENT_H
#define PE_MENU_RGB24_PRESENT_H
static PsyzRgb24MenuProducer title_menu_producer;
static void *title_menu_userdata;
static uint64_t title_menu_output_identity;
static bool title_menu_failed,title_menu_output_hd;
enum { TITLE_LABEL_COUNT=5, TITLE_LABEL_HEADER_BYTES=32, TITLE_LABEL_CACHE_LIMIT=768*1024 };
typedef struct {unsigned char *pixels;unsigned bytes;unsigned char attempted;} TitleLabelImage;
static TitleLabelImage title_label_images[TITLE_LABEL_COUNT];
static unsigned title_label_bytes;
static char title_label_root[4096],title_label_filename[4096];
static bool title_label_root_rejected;
static uint64_t title_label_generation=1;
void Psyz_VideoSetRgb24MenuProducer(PsyzRgb24MenuProducer producer,void *userdata){
    if(title_menu_producer==producer&&title_menu_userdata==userdata)return;
    title_menu_producer=producer;title_menu_userdata=userdata;title_menu_output_identity=0;title_menu_failed=false;
}
static void TitleMenuLabelsReset(void){
    for(unsigned i=0;i<TITLE_LABEL_COUNT;i++){
        SDL_free(title_label_images[i].pixels);title_label_images[i]=(TitleLabelImage){0};
    }
    title_label_bytes=0;title_label_root[0]=0;title_label_root_rejected=false;
    if(!++title_label_generation)++title_label_generation;
    title_menu_output_identity=0;
}
static void TitleMenuRetire(void){
    Psyz_VideoSetRgb24MenuProducer(NULL,NULL);TitleMenuLabelsReset();
}
static bool TitleLabelRootValid(const char *root){
    size_t n=0;while(n<sizeof(title_label_root)-32&&root[n])n++;
    if(n==sizeof(title_label_root)-32||(*root&&*root!='/'))return false;
    for(size_t i=0;i<n;i++){
        unsigned char ch=(unsigned char)root[i];
        if(ch<32||ch==127||ch=='\\'||ch==':')return false;
        if(ch=='/'&&root[i+1]=='.'&&(root[i+2]==0||root[i+2]=='/'||
           (root[i+2]=='.'&&(root[i+3]==0||root[i+3]=='/'))))return false;
    }
    return true;
}
static uint32_t TitleLabelU32(const unsigned char *p){
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static bool TitleLabelRead(SDL_IOStream *io,unsigned char *out,unsigned bytes){
    unsigned at=0;
    while(at<bytes){size_t n=SDL_ReadIO(io,out+at,bytes-at);if(!n||n>bytes-at)return false;at+=(unsigned)n;}
    return true;
}
static unsigned char *TitleLabelLoad(const char *filename,unsigned label,unsigned width,unsigned height,unsigned expected){
    SDL_IOStream *io=SDL_IOFromFile(filename,"rb");if(!io)return NULL;
    unsigned char header[TITLE_LABEL_HEADER_BYTES],tail,*pixels=NULL;
    const Sint64 exact=(Sint64)TITLE_LABEL_HEADER_BYTES+expected;
    /* All allocation sizes come from the source-bound label, NEVER the file.
     * Fixed file length/header must agree before one raw payload is allocated. */
    bool valid=SDL_GetIOSize(io)==exact&&TitleLabelRead(io,header,sizeof header)&&
        !memcmp(header,"PETLRGBA",8)&&TitleLabelU32(header+8)==1&&
        TitleLabelU32(header+12)==TITLE_LABEL_HEADER_BYTES&&TitleLabelU32(header+16)==width&&
        TitleLabelU32(header+20)==height&&TitleLabelU32(header+24)==expected&&TitleLabelU32(header+28)==label;
    if(valid){
        pixels=SDL_malloc(expected);
        valid=pixels&&TitleLabelRead(io,pixels,expected)&&SDL_ReadIO(io,&tail,1)==0&&
            SDL_GetIOStatus(io)==SDL_IO_STATUS_EOF&&SDL_GetIOSize(io)==exact;
    }
    if(!SDL_CloseIO(io))valid=false;
    if(!valid){SDL_free(pixels);pixels=NULL;}
    return pixels;
}
const unsigned char *Psyz_VideoTitleLabelRGBA(unsigned label,int width,int height,
    int *pitch,uint64_t *generation){
    static const char *names[]={"press-start.pelabel","new-game.pelabel","continue.pelabel","tutorial.pelabel","ex-game.pelabel"};
    if(pitch)*pitch=0;if(generation)*generation=title_label_generation;
    if(label>=TITLE_LABEL_COUNT||!pitch||!generation||width!=(label==0?576:336)||height!=96)return NULL;
    const char *root=SDL_getenv("PE_HD_TITLE_LABELS");if(!root)root="";
    if(!TitleLabelRootValid(root)){
        if(!title_label_root_rejected){TitleMenuLabelsReset();title_label_root_rejected=true;}
        *generation=title_label_generation;return NULL;
    }
    if(title_label_root_rejected||strcmp(root,title_label_root)){
        TitleMenuLabelsReset();SDL_strlcpy(title_label_root,root,sizeof title_label_root);
    }
    *generation=title_label_generation;if(!*root)return NULL;
    TitleLabelImage *image=&title_label_images[label];
    if(!image->attempted){
        image->attempted=1;
        unsigned expected=(unsigned)width*(unsigned)height*4u;
        if(title_label_bytes>TITLE_LABEL_CACHE_LIMIT||expected>TITLE_LABEL_CACHE_LIMIT-title_label_bytes)return NULL;
        int n=SDL_snprintf(title_label_filename,sizeof title_label_filename,"%s/%s",root,names[label]);
        if(n<=0||(size_t)n>=sizeof title_label_filename)return NULL;
        image->pixels=TitleLabelLoad(title_label_filename,label,(unsigned)width,(unsigned)height,expected);
        if(image->pixels){image->bytes=expected;title_label_bytes+=expected;}
    }
    if(!image->pixels)return NULL;
    *pitch=width*4;return image->pixels;
}
static uint64_t TitleMenuProbe(const u8 *rgb){
    if(!title_menu_producer||title_menu_failed||UseSoftwareBackend()||!device||gpu_failed||display_size.x!=320||display_size.y!=240)return 0;
    return title_menu_producer(title_menu_userdata,rgb,320,240,display_area.y,NULL,0,4,0,0);
}
#endif

/* SPDX-License-Identifier: MIT. Exact source-owned RGB24 title presentation. */
#ifndef PE_TITLE_COMPANION_H
#define PE_TITLE_COMPANION_H
#ifdef PE_PS5_GPU_NATIVE
#include <psyz/video.h>
#include "title_companion_raster.h"
void MenuCard_BlendRotatedImage(MemcardImageNode *,u8 *,s32,s32);
typedef struct {
    int valid,page_y,style,count;
    unsigned char *owner;
    PeTitleCompanionNode nodes[8];
} PeTitleCompanionFrame;
static PeTitleCompanionFrame title_companion_frames[2];
static unsigned char title_companion_proof[320*240*3];
static uint64_t TitleCompanionHash(uint64_t h,const void *data,size_t bytes){
    const unsigned char *s=data;while(bytes--)h=(h^*s++)*UINT64_C(1099511628211);return h;
}
static int TitleCompanionOriginal(MemcardImageNode *node){
    if(!node||TitleType_NodeSlot(node)<0||node->kind<1||node->kind>7||!title_owner.base||title_owner.base!=g_StrFileDirBuffer)return 0;
    int offset=g_MemcardImageOffsets[node->kind];
    if(offset<0)return 0;
    size_t at=0x4264u+(unsigned)offset;
    if(at>title_owner.bytes||20u>title_owner.bytes-at||
       node->image!=(MemcardImage *)(title_owner.base+at)||
       node->width<=0||node->width>320||node->height<=0||node->height>240||
       node->image->width*2!=node->width*3||node->image->height!=node->height)return 0;
    size_t bytes=(size_t)node->width*node->height*3u;
    return bytes<=title_owner.bytes-at-20u;
}
static const unsigned char *TitleCompanionBackground(void){
    if(!title_owner.base||title_owner.base!=g_StrFileDirBuffer)return NULL;
    int offset=g_MemcardImageOffsets[0];if(offset<0)return NULL;
    size_t at=0x4264u+(unsigned)offset;
    if(at>title_owner.bytes||20u+320u*204u*3u>title_owner.bytes-at)return NULL;
    MemcardImage *image=(MemcardImage *)(title_owner.base+at);
    return image->width==480&&image->height==204?(unsigned char *)image+20:NULL;
}
static uint64_t TitleCompanionProduce(void *opaque,const unsigned char *rgb,
    int width,int height,int page_y,unsigned char *rgba,int pitch,int scale,int margin,int preserve_background){
    (void)opaque;
    if(!rgb||width!=320||height!=240||scale!=4||margin<0||margin>256)return 0;
    PeTitleCompanionFrame *frame=NULL;
    for(unsigned i=0;i<2;i++)if(title_companion_frames[i].valid&&title_companion_frames[i].page_y==page_y){
        if(frame)return 0; /* ambiguous physical page never chooses a snapshot */
        frame=&title_companion_frames[i];
    }
    if(!frame||frame->owner!=title_owner.base||frame->style!=PeFont_GetStyle(1))return 0;
    const unsigned char *background=TitleCompanionBackground();if(!background)return 0;
    memset(title_companion_proof,0,sizeof title_companion_proof);
    memcpy(title_companion_proof+20*320*3,background,320*204*3);
    uint64_t identity=UINT64_C(14695981039346656037);
    for(int i=0;i<frame->count;i++){
        PeTitleCompanionNode *node=&frame->nodes[i];
        node->replacement=NULL;node->replacement_pitch=0;
        if(!PeTitleDrawNode(title_companion_proof,320*3,3,320,240,node,1,0))return 0;
    }
    /* Exact whole native image is the frame witness. Dimensions alone never
     * classify a movie, title, stale page or another RGB24 screen as this UI. */
    if(memcmp(title_companion_proof,rgb,sizeof title_companion_proof))return 0;
    for(int i=0;i<frame->count;i++){
        PeTitleCompanionNode *node=&frame->nodes[i];
        /* Low-resolution quantization can hide an intensity/progress change
         * that still matters at4x. Cache the actual source descriptors too. */
        const int values[]={node->x,node->y,node->width,node->height,node->intensity,node->progress,node->rotated,node->label};
        identity=TitleCompanionHash(identity,values,sizeof values);
        identity=TitleCompanionHash(identity,node->rgb,(size_t)node->width*node->height*3);
        if(node->label>=0&&frame->style==PE_FONT_STYLE_PLEX){
            uint64_t asset_generation=0;
            node->replacement=Psyz_VideoTitleLabelRGBA((unsigned)node->label,node->width*4,96,
                                                       &node->replacement_pitch,&asset_generation);
            identity=TitleCompanionHash(identity,&asset_generation,sizeof asset_generation);
            unsigned replacement_available=node->replacement!=NULL;
            identity=TitleCompanionHash(identity,&replacement_available,sizeof replacement_available);
        }
    }
    identity=TitleCompanionHash(identity,rgb,sizeof title_companion_proof);
    identity=TitleCompanionHash(identity,&frame->style,sizeof frame->style);
    if(!identity)identity=1;
    if(!rgba)return identity;
    if(pitch<(320*4+margin*2)*4||pitch>8192*4)return 0;
    if(!preserve_background){
        for(int y=0;y<240*4;y++){
            unsigned char *out=rgba+y*pitch;
            memset(out,0,(320*4+margin*2)*4);
            for(int x=0;x<320*4+margin*2;x++)out[x*4+3]=255;
            if(y>=20*4&&y<(20+204)*4)for(int x=0;x<320*4;x++){
                const unsigned char *s=background+((y/4-20)*320+x/4)*3;
                unsigned char *d=out+(x+margin)*4;d[0]=s[0];d[1]=s[1];d[2]=s[2];
            }
        }
    }
    for(int i=0;i<frame->count;i++)
        if(!PeTitleDrawNode(rgba,pitch,4,320,240,&frame->nodes[i],4,margin))return 0;
    return identity;
}
void MenuCard_BindTitlePresentation(void){
    if(!title_owner.base||title_owner.base!=g_StrFileDirBuffer)return;
    int slot=g_MemcardCurrentScreen==g_MemcardScreenBuffers[0]?0:
             g_MemcardCurrentScreen==g_MemcardScreenBuffers[1]?1:-1;
    if(slot<0)return;
    PeTitleCompanionFrame *frame=&title_companion_frames[slot];frame->valid=0;frame->count=0;
    frame->style=PeFont_GetStyle(1);
    if(frame->style!=PE_FONT_STYLE_PLEX&&frame->style!=PE_FONT_STYLE_ATKINSON)return;
    frame->owner=title_owner.base;frame->page_y=g_MemcardCurrentScreen->disp.disp.y;
    int high_labels=0,visited=0;
    for(MemcardImageNode *node=g_MemcardActiveNodeHead;node;node=node->next){
        if(visited++>=8||!TitleCompanionOriginal(node))return;
        if(!node->update)continue;
        if(node->update!=MenuCard_BlendImage&&node->update!=MenuCard_BlendRotatedImage)return;
        MemcardImage *image=TitleType_Image(node);
        PeTitleCompanionNode *out=&frame->nodes[frame->count++];
        *out=(PeTitleCompanionNode){.x=node->x,.y=node->y,.width=node->width,.height=node->height,
            .intensity=node->intensity,.progress=node->targetY,.rotated=node->update==MenuCard_BlendRotatedImage,
            .label=-1,.rgb=(const unsigned char *)image+20};
        for(unsigned i=0;i<5;i++)if(node->kind==labels[i].kind){
            if(image==node->image||title_label_hi_style[i]!=frame->style)return;
            out->label=(int)i;out->coverage=title_label_hi[i];high_labels++;
        }
    }
    if(!high_labels)return;
    frame->valid=1;Psyz_VideoSetRgb24MenuProducer(TitleCompanionProduce,NULL);
}
static void TitleCompanionRetire(void){
    memset(title_companion_frames,0,sizeof title_companion_frames);
    Psyz_VideoSetRgb24MenuProducer(NULL,NULL);
}
#else
void MenuCard_BindTitlePresentation(void){}
static void TitleCompanionRetire(void){}
#endif
#endif

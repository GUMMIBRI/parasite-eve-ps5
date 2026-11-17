/* SPDX-License-Identifier: MIT. Source-owned title max-blend companion.
 * Integer source/destination mapping follows the native title consumers. */
#ifndef PE_TITLE_COMPANION_RASTER_H
#define PE_TITLE_COMPANION_RASTER_H
typedef struct {
    int x,y,width,height,intensity,progress,rotated,label;
    const unsigned char *rgb,*coverage,*replacement;
    int replacement_pitch;
} PeTitleCompanionNode;
static int PeTitlePixel(PeTitleCompanionNode *node,int sx,int sy,int *dx,int *dy,int *turn){
    *turn=0;
    if(!node->rotated){*dx=node->x+sx;*dy=node->y+sy;return 1;}
    int p=node->progress,columns=p+24-sy;
    if(columns>84)columns=84;
    if(sx<columns){*dx=node->x+sx;*dy=node->y+(p<60?60-p:0)+sy;return 1;}
    /* The two native pieces may cover a source pixel twice. The second piece
     * is iterated separately below, retaining its original max-blend order. */
    return 0;
}
static void PeTitleBlend(unsigned char *out,int pitch,int channels,int width,int height,
                         const PeTitleCompanionNode *node,int sx,int sy,int dx,int dy,
                         int scale,int turn){
    if(dx<0||dy<0||dx>=width||dy>=height)return;
    const unsigned char *source=node->rgb+(sy*node->width+sx)*3;
    for(int y=0;y<scale;y++)for(int x=0;x<scale;x++){
        int ux=turn?scale-1-y:x,uy=turn?scale-1-x:y;
        unsigned char *d=out+(dy*scale+y)*pitch+(dx*scale+x)*channels;
        for(int c=0;c<3;c++){
            int value=source[c];
            if(scale==4&&node->replacement){
                const unsigned char *p=node->replacement+(sy*4+uy)*node->replacement_pitch+(sx*4+ux)*4;
                value=((unsigned)p[c]*p[3]+127)/255;
            }else if(scale==4&&node->coverage)value=node->coverage[(sy*4+uy)*(144*4)+sx*4+ux];
            value=value*node->intensity>>8;
            if(value>d[c])d[c]=(unsigned char)value;
        }
    }
}
static int PeTitleDrawNode(unsigned char *out,int pitch,int channels,int width,int height,
                           PeTitleCompanionNode *node,int scale,int margin){
    if(!out||!node->rgb||node->width<=0||node->width>320||node->height<=0||node->height>240||
       node->intensity<0||node->intensity>256||(scale!=1&&scale!=4))return 0;
    if(node->rotated&&(node->width!=84||node->height!=24||node->progress<0||node->progress>84))return 0;
    int maxw=node->rotated?84:node->width;
    int maxh=node->rotated?(node->progress<60?84-node->progress:24):node->height;
    if(node->x<0||node->y<0||node->x+maxw>width||node->y+maxh>height)return 0;
    out+=margin*channels;
    for(int sy=0;sy<node->height;sy++)for(int sx=0;sx<node->width;sx++){
        int dx,dy,turn;if(PeTitlePixel(node,sx,sy,&dx,&dy,&turn))
            PeTitleBlend(out,pitch,channels,width,height,node,sx,sy,dx,dy,scale,turn);
    }
    if(node->rotated)for(int row=0;row<84-node->progress;row++){
        int count=84-row-node->progress;if(count>24)count=24;
        for(int column=0;column<count;column++)
            PeTitleBlend(out,pitch,channels,width,height,node,83-row,23-column,
                         node->x+node->progress+column,node->y+row,scale,1);
    }
    return 1;
}
#endif

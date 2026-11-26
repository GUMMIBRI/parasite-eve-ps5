#include <assert.h>
#include <math.h>
#include <stdio.h>
#define PE_PS5_GPU_NATIVE 1
enum{OVERLAY_HEADER_H=128,OVERLAY_ROW_H=46,OVERLAY_ACTION_GAP=14,OVERLAY_FOOTER_H=94,OVERLAY_ACTION_ROWS=4,OVERLAY_REFERENCE_H=820,OVERLAY_PANEL_W=840};
typedef struct{int x,y,w,h;float scale;}OverlayLayout;
static int category;static int Host_SettingsVisibleRowCount(int page){int counts[]={12,11,7+category*3,13,11};return counts[page];}
static int OverlayDesignHeight(int rowCount) {
#ifdef PE_PS5_GPU_NATIVE
    rowCount=9+OVERLAY_ACTION_ROWS; /* Fixed outer frame for every tab/category. */
#endif
    return OVERLAY_HEADER_H + rowCount * OVERLAY_ROW_H + OVERLAY_ACTION_GAP +
           OVERLAY_FOOTER_H;
}

static void OverlayFitLayout(int designW, int designH, int windowW, int windowH,
                             OverlayLayout *layout) {
    float scale = (float)windowH / (float)OVERLAY_REFERENCE_H;
    float widthLimit = (float)windowW * 0.94f / (float)designW;
    float heightLimit = (float)windowH * 0.94f / (float)designH;

    if (scale > widthLimit) scale = widthLimit;
    if (scale > heightLimit) scale = heightLimit;
    if (scale > 2.5f) scale = 2.5f;
#ifndef PE_PS5_GPU_NATIVE
    if (scale < 0.5f) scale = 0.5f;
#endif
    if(scale>=0.125f)scale = floorf(scale * 8.0f) / 8.0f; /* no repaint on every resize pixel */
    layout->scale = scale;
    layout->w = (int)ceilf((float)designW * scale);
    layout->h = (int)ceilf((float)designH * scale);
    layout->x = (windowW - layout->w) / 2;
    layout->y = (windowH - layout->h) / 2;
}

static void OverlayComputeLayout(int page, int windowW, int windowH,
                                 OverlayLayout *layout) {
    OverlayFitLayout(OVERLAY_PANEL_W, OverlayDesignHeight(Host_SettingsVisibleRowCount(page)),
                     windowW, windowH, layout);
}

int main(void){int sizes[][2]={{3840,2160},{1920,1080},{1280,720},{640,480},{320,240}};for(int i=0;i<5;i++){OverlayLayout first;OverlayComputeLayout(0,sizes[i][0],sizes[i][1],&first);assert(first.x>=0&&first.y>=0&&first.w<=sizes[i][0]&&first.h<=sizes[i][1]);for(category=0;category<3;category++)for(int page=0;page<5;page++){OverlayLayout next;OverlayComputeLayout(page,sizes[i][0],sizes[i][1],&next);assert(next.x==first.x&&next.y==first.y&&next.w==first.w&&next.h==first.h&&next.scale==first.scale);assert(OverlayDesignHeight(Host_SettingsVisibleRowCount(page))==834);}}puts("actual geometry:all tabs/categories fixed frame; five viewport bounds PASS");}

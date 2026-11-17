#ifndef MENU_MEMCARD_SCREEN_H
#define MENU_MEMCARD_SCREEN_H

#include "common.h"
#include "pe1/psyq_gpu.h"
#include "pe1/render_packets.h"

/* One of the two full-screen buffers the memory card screens flip between. */
typedef struct MemcardScreenBuffer {
    DRAWENV draw;
    DISPENV disp;
    RECT overlay;
    RECT dirty;
    RenderNativeTag ordering[0x1000];
    _Alignas(16) u8 packets[0x4000];
    u8 overlayImage[0x14000];
    _Alignas(16) u8 presentImage[320 * 204 * 2 + 8];
} MemcardScreenBuffer;
PE1_STATIC_ASSERT(PE1_OFFSETOF(MemcardScreenBuffer, ordering) >= sizeof(DRAWENV) + sizeof(DISPENV) + 2*sizeof(RECT),
                  memcard_screen_ordering_offset);
PE1_STATIC_ASSERT(sizeof(((MemcardScreenBuffer *)0)->ordering) == 0x1000*sizeof(RenderNativeTag),
                  memcard_screen_ordering_capacity);

typedef struct MemcardSprite {
    RenderNativeTag tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} MemcardSprite;
PE1_STATIC_ASSERT(sizeof(MemcardSprite) == sizeof(RenderNativeTag) + 16,
                  memcard_sprite_native_size);

typedef struct MemcardDrawTPage {
    RenderNativeTag tag;
    uintptr_t code;
} MemcardDrawTPage;
PE1_STATIC_ASSERT(sizeof(MemcardDrawTPage) == 3*sizeof(uintptr_t),
                  memcard_page_native_size);

/* A TIM image block: byte size, VRAM rectangle, pixels. */
typedef struct MemcardTimBlock {
    u32 size;
    RECT rect;
    u32 data[1];
} MemcardTimBlock;

typedef struct MemcardTim {
    u32 id;
    u32 flags;
    MemcardTimBlock clut;
} MemcardTim;

extern MemcardScreenBuffer *g_MemcardScreenBuffers[2];
extern MemcardScreenBuffer *g_MemcardCurrentScreen;
extern s32 g_MemcardScreenPage;

/* Publish the actual overlay for every page, including interactive redraws.
 * These are RGB24 pixel coordinates, before packing x/w into VRAM words. */
#ifdef PE_PS5_GPU_NATIVE
void Psyz_VideoBindRgb24BackdropOverlay(int x, int y, int width, int height);
void MenuCard_BindTitlePresentation(void);
#endif
static inline void MenuCard_BindHdBackgroundOverlay(void) {
#ifdef PE_PS5_GPU_NATIVE
    MenuCard_BindTitlePresentation();
    const RECT *area = &g_MemcardCurrentScreen->overlay;
    Psyz_VideoBindRgb24BackdropOverlay(area->x, area->y, area->w, area->h);
#endif
}


void SetDispMask(s32 mask);
void ResetGraph(s32 mode);
void MenuCard_ShowLogoFade(void);
void MenuCard_FlipScreenPages(void);

#endif

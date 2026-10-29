/* Image presenter, with the game's frame stubbed: where a box goes, which pixels strips paste, and no
 * paste on a 24-bit display. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "pe_mod_host.h"
#include "pe_present.h"
#include "pe1/display_env.h"
#include "pe1/draw_buffers.h"

DISPENV g_RenderDispEnvArray[2];
DRAWENV g_RenderDrawEnvArray[2];
int g_ActiveDrawSlot;
static PeHookFn s_frame_drawn;
static RECT s_rect[64];
static int s_loads, s_covered;

int pe_hook_add(int id, PeHookFn fn, void *u, int p, const char *m) {
    (void)u; (void)p; (void)m; assert(id == PE_HOOK_FRAME_DRAWN); s_frame_drawn = fn; return 0;
}
int LoadImage(RECT *r, void *pixels) { (void)pixels; if (s_loads < 64) s_rect[s_loads] = *r; ++s_loads; s_covered += r->w * r->h; return 0; }
Uint64 SDL_GetPerformanceCounter(void) { static Uint64 t; return t += 100; }
Uint64 SDL_GetPerformanceFrequency(void) { return 1000000; }

int main(void) {
    static uint8_t rgba[8 * 4 * 4];
    PeImage img = {8, 4, rgba};
    int i;
    for (i = 0; i < 8 * 4; ++i) { rgba[4 * i] = 255; rgba[4 * i + 3] = (i % 8 < 3 || i % 8 == 6) ? 255 : 0; }  /* 2 runs a row */
    g_RenderDrawEnvArray[0].clip = (RECT){0, 0, 320, 224};
    g_RenderDrawEnvArray[1].clip = (RECT){0, 224, 320, 224};
    pe_present_install();
    assert(s_frame_drawn);

    s_frame_drawn(NULL, NULL);                          /* nothing shown: nothing pasted */
    assert(s_loads == 0);

    assert(pe_present_show(0, 100, 50, &img, PE_PRESENT_BOX, 0x102030) == 0);
    g_ActiveDrawSlot = 0;                               /* the frame just drawn is buffer 1 */
    s_frame_drawn(NULL, NULL);
    assert(s_loads == 1 && s_rect[0].x == 100 && s_rect[0].y == 224 + 50 && s_rect[0].w == 8 && s_rect[0].h == 4);
    g_ActiveDrawSlot = 1; s_loads = 0;
    s_frame_drawn(NULL, NULL);
    assert(s_loads == 1 && s_rect[0].y == 50);

    assert(pe_present_show(0, 100, 50, &img, PE_PRESENT_STRIPS, 0) == 0);
    s_loads = s_covered = 0;
    s_frame_drawn(NULL, NULL);
    assert(s_loads == 8 && s_covered == 4 * 4);          /* 2 strips a row, 4 covered pixels a row */
    assert(s_rect[0].x == 100 && s_rect[0].w == 3 && s_rect[1].x == 106 && s_rect[1].w == 1 && s_rect[0].h == 1);

    g_RenderDispEnvArray[1].isrgb24 = 1; s_loads = 0;   /* a movie's 24-bit display: no paste */
    s_frame_drawn(NULL, NULL);
    assert(s_loads == 0);
    g_RenderDispEnvArray[1].isrgb24 = 0;

    assert(pe_present_show(0, 316, 50, &img, PE_PRESENT_BOX, 0) == 0);   /* off the frame: skipped */
    s_loads = 0;
    s_frame_drawn(NULL, NULL);
    assert(s_loads == 0);

    pe_present_hide(0); s_loads = 0;
    s_frame_drawn(NULL, NULL);
    assert(s_loads == 0);
    assert(pe_present_show(PE_PRESENT_SLOTS, 0, 0, &img, PE_PRESENT_BOX, 0) != 0);
    puts("Presenter checks passed: box placement in the shown buffer, strips, 24-bit displays, clipping, hide.");
    return 0;
}

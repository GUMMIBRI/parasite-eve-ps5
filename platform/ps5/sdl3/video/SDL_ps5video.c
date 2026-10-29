/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* Altered for SDL3 from public ps5-payload-dev/SDL ee4c47dc0d617b3bc8f35108f9956baf228a1322. Software-only fixed1080p; not console-tested. */
#include "SDL_internal.h"
#include "SDL_properties_c.h"
#ifdef SDL_VIDEO_DRIVER_PS5
#include "video/SDL_sysvideo.h"
#include "SDL_ps5native.h"
#include "SDL_ps5tilemap.h"
#include <sys/mman.h>

#define PS5_SURFACE_PROPERTY "SDL.internal.ps5.framebuffer"
#define PS5_WIDTH 1920
#define PS5_HEIGHT 1080

struct SDL_VideoData {
    int handle;
    struct kevent *queue;
    bool flip_event, allocated;
    intptr_t physical;
    void *mapped;
    size_t bytes;
    PS5_VideoBuf buffers[2];
    SDL_Surface *canvas;
    PS5_Tilemap *tilemap;
    SDL_Window *window;
    Uint32 frame;
};

static void PS5_VideoQuit(SDL_VideoDevice *device)
{
    SDL_VideoData *data = device->internal;
    if (data->flip_event) {
        sceVideoOutDeleteFlipEvent(data->queue, data->handle);
        data->flip_event = false;
    }
    if (data->handle >= 0) {
        sceVideoOutClose(data->handle);
        data->handle = -1;
    }
    if (data->queue) {
        sceKernelDeleteEqueue(data->queue);
        data->queue = NULL;
    }
    if (data->mapped) {
        munmap(data->mapped, data->bytes);
        data->mapped = NULL;
    }
    if (data->allocated) {
        sceKernelReleaseDirectMemory(data->physical, data->bytes);
        data->allocated = false;
    }
    SDL_DestroySurface(data->canvas);
    data->canvas = NULL;
    PS5_Tilemap_Destroy(data->tilemap);
    data->tilemap = NULL;
}

static bool PS5_InitError(SDL_VideoDevice *device, const char *operation, int result)
{
    PS5_VideoQuit(device);
    return SDL_SetError("%s failed: 0x%x", operation, (unsigned)result);
}

static bool PS5_VideoInit(SDL_VideoDevice *device)
{
    SDL_VideoData *data = device->internal;
    PS5_VideoAttr attributes = {0};
    SDL_DisplayMode mode;
    int result;
    data->handle = sceVideoOutOpen(0xff, 0, 0, NULL);
    if (data->handle < 0) return PS5_InitError(device, "sceVideoOutOpen", data->handle);
    data->bytes = 0x4000000;
    result = sceKernelAllocateMainDirectMemory(data->bytes, 0x20000, 3, &data->physical);
    if (result) return PS5_InitError(device, "sceKernelAllocateMainDirectMemory", result);
    data->allocated = true;
    result = sceKernelMapDirectMemory(&data->mapped, data->bytes, 0x33, 0, data->physical, 0x20000);
    if (result) return PS5_InitError(device, "sceKernelMapDirectMemory", result);
    SDL_memset(data->mapped, 0, data->bytes);
    data->buffers[0].data = data->mapped;
    data->buffers[1].data = (Uint8 *)data->mapped + data->bytes / 2;
    if (PS5_Tilemap_BufferSize(PS5_WIDTH, PS5_HEIGHT) > data->bytes / 2)
        return PS5_InitError(device, "scanout buffer size", -1);
    result = sceKernelCreateEqueue(&data->queue, "pe-sdl3-flip");
    if (result) return PS5_InitError(device, "sceKernelCreateEqueue", result);
    result = sceVideoOutAddFlipEvent(data->queue, data->handle, NULL);
    if (result) return PS5_InitError(device, "sceVideoOutAddFlipEvent", result);
    data->flip_event = true;
    result = sceVideoOutSetFlipRate(data->handle, 0);
    if (result) return PS5_InitError(device, "sceVideoOutSetFlipRate", result);
    sceVideoOutSetBufferAttribute2(&attributes, UINT64_C(0x8000000022000000), 0,
                                  PS5_WIDTH, PS5_HEIGHT, 0, 0, 0);
    result = sceVideoOutRegisterBuffers2(data->handle, 0, 0, data->buffers, 2, &attributes, 0, NULL);
    if (result) return PS5_InitError(device, "sceVideoOutRegisterBuffers2", result);
    data->canvas = SDL_CreateSurface(PS5_WIDTH, PS5_HEIGHT, SDL_PIXELFORMAT_ABGR8888);
    data->tilemap = PS5_Tilemap_Create(PS5_WIDTH, PS5_HEIGHT);
    if (!data->canvas || !data->tilemap) return PS5_InitError(device, "software scanout allocation", -1);
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ABGR8888;
    mode.w = PS5_WIDTH;
    mode.h = PS5_HEIGHT;
    mode.refresh_rate = 60.0f;
    if (!SDL_AddBasicVideoDisplay(&mode)) return PS5_InitError(device, "SDL_AddBasicVideoDisplay", -1);
    sceSystemServiceHideSplashScreen();
    return true;
}

static bool PS5_CreateWindow(SDL_VideoDevice *device, SDL_Window *window, SDL_PropertiesID properties)
{
    (void)properties;
    if (device->internal->window) return SDL_SetError("PS5 supports one application window");
    if (window->flags & (SDL_WINDOW_OPENGL | SDL_WINDOW_VULKAN | SDL_WINDOW_METAL))
        return SDL_SetError("PS5 backend currently supports software presentation only");
    device->internal->window = window;
    return true;
}

static bool PS5_CreateFramebuffer(SDL_VideoDevice *device, SDL_Window *window,
                                 SDL_PixelFormat *format, void **pixels, int *pitch)
{
    SDL_Surface *surface;
    int width, height;
    (void)device;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height)) return false;
    if (width <= 0 || height <= 0 || width > PS5_WIDTH || height > PS5_HEIGHT)
        return SDL_SetError("PS5 software framebuffer must fit 1920x1080");
    surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ABGR8888);
    if (!surface) return false;
    if (!SDL_SetSurfaceProperty(SDL_GetWindowProperties(window), PS5_SURFACE_PROPERTY, surface)) {
        /* SDL_SetSurfaceProperty takes cleanup ownership even on failure. */
        return false;
    }
    *format = SDL_PIXELFORMAT_ABGR8888;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return true;
}

static bool PS5_UpdateFramebuffer(SDL_VideoDevice *device, SDL_Window *window,
                                 const SDL_Rect *rectangles, int count)
{
    SDL_VideoData *data = device->internal;
    SDL_Surface *surface = SDL_GetPointerProperty(SDL_GetWindowProperties(window), PS5_SURFACE_PROPERTY, NULL);
    struct kevent event;
    SDL_Rect destination;
    int returned, result;
    unsigned index = data->frame & 1;
    (void)rectangles;
    (void)count;
    if (!surface) return SDL_SetError("PS5 window framebuffer unavailable");
    /* Preserve the window aspect ratio; never stretch a 4:3 game to 16:9. */
    destination.w = PS5_WIDTH;
    destination.h = surface->h * PS5_WIDTH / surface->w;
    if (destination.h > PS5_HEIGHT) {
        destination.h = PS5_HEIGHT;
        destination.w = surface->w * PS5_HEIGHT / surface->h;
    }
    destination.x = (PS5_WIDTH - destination.w) / 2;
    destination.y = (PS5_HEIGHT - destination.h) / 2;
    if (!SDL_FillSurfaceRect(data->canvas, NULL, UINT32_C(0xff000000))) return false;
    /* Full damage ensures the two alternating scanout buffers stay consistent. */
    if (!SDL_BlitSurfaceScaled(surface, NULL, data->canvas, &destination, SDL_SCALEMODE_NEAREST)) return false;
    PS5_Tilemap_Blit(data->tilemap, data->canvas->pixels, data->canvas->pitch / 4,
                    data->buffers[index].data, (int)index, NULL, 0, 0, 0);
    result = sceVideoOutSubmitFlip(data->handle, (int)index, 1, data->frame);
    if (result) return SDL_SetError("sceVideoOutSubmitFlip failed: 0x%x", (unsigned)result);
    result = sceKernelWaitEqueue(data->queue, &event, 1, &returned, NULL);
    if (result) return SDL_SetError("sceKernelWaitEqueue failed: 0x%x", (unsigned)result);
    ++data->frame;
    return true;
}

static void PS5_DestroyFramebuffer(SDL_VideoDevice *device, SDL_Window *window)
{
    (void)device;
    SDL_ClearProperty(SDL_GetWindowProperties(window), PS5_SURFACE_PROPERTY);
}

static void PS5_DestroyWindow(SDL_VideoDevice *device, SDL_Window *window)
{
    if (device->internal->window == window) device->internal->window = NULL;
}

static void PS5_DeleteDevice(SDL_VideoDevice *device)
{
    PS5_VideoQuit(device);
    SDL_free(device->internal);
    SDL_free(device);
}

/* This fixed-display backend has no native window event queue. SDL's common
 * event maintenance polls the joystick driver separately after this callback. */
static void PS5_PumpEvents(SDL_VideoDevice *device)
{
    (void)device;
}

static SDL_VideoDevice *PS5_CreateDevice(void)
{
    SDL_VideoDevice *device = SDL_calloc(1, sizeof(*device));
    if (!device) { SDL_OutOfMemory(); return NULL; }
    device->internal = SDL_calloc(1, sizeof(*device->internal));
    if (!device->internal) { SDL_free(device); SDL_OutOfMemory(); return NULL; }
    device->internal->handle = -1;
    device->VideoInit = PS5_VideoInit;
    device->VideoQuit = PS5_VideoQuit;
    device->PumpEvents = PS5_PumpEvents;
    device->CreateSDLWindow = PS5_CreateWindow;
    device->DestroyWindow = PS5_DestroyWindow;
    device->CreateWindowFramebuffer = PS5_CreateFramebuffer;
    device->UpdateWindowFramebuffer = PS5_UpdateFramebuffer;
    device->DestroyWindowFramebuffer = PS5_DestroyFramebuffer;
    device->free = PS5_DeleteDevice;
    return device;
}

VideoBootStrap PS5_bootstrap = { "ps5", "Public PS5 software video", PS5_CreateDevice, NULL, false };
#endif


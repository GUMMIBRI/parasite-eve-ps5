/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

/* Altered source: SDL3 private-driver adaptation of ps5-payload-dev/SDL
 * src/audio/ps5/SDL_ps5audio.{c,h} at
 * ee4c47dc0d617b3bc8f35108f9956baf228a1322.
 * Targets SDL3 fa2c02bb6e21974a89ea9824bc53c9932abe5f9c.
 * No proprietary Sony headers; console output remains owner-validated.
 */

#include "SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_PS5

#include "SDL_ps5audio.h"

static bool PS5AUDIO_OpenDevice(SDL_AudioDevice *device)
{
    const SDL_AudioFormat *formats;
    SDL_AudioFormat format;
    unsigned int native_format;

    if (device->recording) {
        return SDL_SetError("PS5 audio recording is unavailable");
    }
    device->hidden = SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return false;
    }
    device->hidden->handle = -1;

    formats = SDL_ClosestAudioFormats(device->spec.format);
    while ((format = *formats++) != SDL_AUDIO_UNKNOWN) {
        if (format == SDL_AUDIO_S16LE || format == SDL_AUDIO_F32LE) {
            break;
        }
    }
    if (format == SDL_AUDIO_UNKNOWN) {
        return SDL_SetError("PS5 audio has no supported device format");
    }

    device->spec.format = format;
    device->spec.freq = 48000;
    device->spec.channels = device->spec.channels == 1 ? 1 : 2;

    /* Public SDL2 backend uses grains 256, 512, ... 2048 frames. */
    device->sample_frames = SDL_clamp(device->sample_frames, 256, 2048);
    device->sample_frames = (device->sample_frames / 256) * 256;
    SDL_UpdatedAudioDeviceFormat(device);

    if (format == SDL_AUDIO_S16LE) {
        native_format = device->spec.channels == 1 ? PS5_AUDIO_OUT_FORMAT_S16_MONO : PS5_AUDIO_OUT_FORMAT_S16_STEREO;
    } else {
        native_format = device->spec.channels == 1 ? PS5_AUDIO_OUT_FORMAT_FLOAT_MONO : PS5_AUDIO_OUT_FORMAT_FLOAT_STEREO;
    }

    const int handle = sceAudioOutOpen(PS5_AUDIO_SYSTEM_USER, PS5_AUDIO_OUT_PORT_TYPE_MAIN,
                                      0, device->sample_frames, device->spec.freq, native_format);
    /* Preserve the public SDL2 backend's handle < 1 failure check. */
    if (handle < 1) {
        return SDL_SetError("sceAudioOutOpen failed: 0x%08x", (unsigned int)handle);
    }
    device->hidden->handle = handle;
    device->hidden->mixbuf = SDL_aligned_alloc(64, device->buffer_size);
    if (!device->hidden->mixbuf) {
        return false; /* SDL3 calls CloseDevice for partial initialization. */
    }
    SDL_memset(device->hidden->mixbuf, device->silence_value, device->buffer_size);
    return true;
}

static Uint8 *PS5AUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    *buffer_size = device->buffer_size;
    return device->hidden->mixbuf;
}

static bool PS5AUDIO_WaitDevice(SDL_AudioDevice *device)
{
    /* SDL3 calls this on the same playback thread, after releasing the physical
     * device mutex. CloseDevice cannot free the buffer until this thread joins.
     * No other thread reads or writes pending, so it needs no second lock.
     */
    if (!device->hidden->pending) {
        return true;
    }
    device->hidden->pending = false;
    if (SDL_GetAtomicInt(&device->shutdown)) {
        return true; /* Do not submit a new grain after close was requested. */
    }
    const int result = sceAudioOutOutput(device->hidden->handle, device->hidden->mixbuf);
    if (result < 0) {
        return SDL_SetError("sceAudioOutOutput failed: 0x%08x", (unsigned int)result);
    }
    return true;
}

static bool PS5AUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    if (buffer != device->hidden->mixbuf || buflen != device->buffer_size) {
        return SDL_SetError("PS5 audio received an incomplete playback grain");
    }
    if (device->hidden->pending) {
        return SDL_SetError("PS5 audio playback grain has not been consumed");
    }
    /* PlayDevice runs under SDL3's physical-device mutex: validate and publish
     * only. The playback thread owns mixbuf until WaitDevice returns; it cannot
     * start filling the next grain while native output still reads this one.
     */
    device->hidden->pending = true;
    return true;
}

static void PS5AUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (device->hidden->handle >= 1) {
            sceAudioOutClose(device->hidden->handle);
            device->hidden->handle = -1;
        }
        SDL_aligned_free(device->hidden->mixbuf);
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

static bool PS5AUDIO_Init(SDL_AudioDriverImpl *impl)
{
    static bool initialized;
    if (!initialized) {
        const int result = sceAudioOutInit();
        if (result != 0) {
            return SDL_SetError("sceAudioOutInit failed: 0x%08x", (unsigned int)result);
        }
        initialized = true;
    }

    impl->OpenDevice = PS5AUDIO_OpenDevice;
    impl->CloseDevice = PS5AUDIO_CloseDevice;
    impl->GetDeviceBuf = PS5AUDIO_GetDeviceBuf;
    impl->WaitDevice = PS5AUDIO_WaitDevice;
    impl->PlayDevice = PS5AUDIO_PlayDevice;
    impl->OnlyHasDefaultPlaybackDevice = true;
    return true;
}

AudioBootStrap PS5AUDIO_bootstrap = {
    "ps5", "PS5 public homebrew audio driver", PS5AUDIO_Init, false, false
};

#endif /* SDL_AUDIO_DRIVER_PS5 */


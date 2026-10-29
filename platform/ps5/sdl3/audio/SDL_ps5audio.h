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

#ifndef SDL_ps5audio_h_
#define SDL_ps5audio_h_

#include "SDL_sysaudio.h"

struct SDL_PrivateAudioData
{
    int handle;
    Uint8 *mixbuf;
};

#define PS5_AUDIO_OUT_PORT_TYPE_MAIN 0
#define PS5_AUDIO_OUT_FORMAT_S16_MONO 0
#define PS5_AUDIO_OUT_FORMAT_S16_STEREO 1
#define PS5_AUDIO_OUT_FORMAT_FLOAT_MONO 3
#define PS5_AUDIO_OUT_FORMAT_FLOAT_STEREO 4
#define PS5_AUDIO_SYSTEM_USER 0xff

/* Declarations retained from the public SDL2 PS5 backend, not Sony SDK headers. */
int sceAudioOutInit(void);
int sceAudioOutOpen(int user_id, int type, int index,
                    unsigned int frames, unsigned int frequency, unsigned int format);
int sceAudioOutOutput(int handle, const void *buffer);
int sceAudioOutClose(int handle);

#endif


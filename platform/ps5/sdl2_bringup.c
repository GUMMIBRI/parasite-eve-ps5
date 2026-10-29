/* Standalone public-SDL2 bring-up, not a Parasite Eve game executable.
 * Uses the zlib-licensed ps5-payload-dev/SDL public API, not Sony SDK headers.
 * No console execution has been performed by the authoring agent.
 * Console behavior of the pinned public SDL2 PS5 backend is unverified;
 * see work/ps5-coordination/PUBLIC-PLATFORM-PATH.md before deploying this test.
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <string.h>

typedef struct ToneState {
    unsigned phase;
    unsigned frames_left;
} ToneState;

/* Optional short, low-volume square-wave signal; default output is silence. */
static void SDLCALL audio_callback(void *userdata, Uint8 *bytes, int length) {
    ToneState *tone = (ToneState *)userdata;
    Sint16 *samples = (Sint16 *)bytes;
    int frames = length / (int)(2 * sizeof(*samples));
    SDL_memset(bytes, 0, (size_t)length);
    for (int i = 0; i < frames && tone->frames_left; ++i) {
        Sint16 sample = tone->phase < 55 ? 512 : -512;
        samples[2 * i] = sample;
        samples[2 * i + 1] = sample;
        tone->phase = (tone->phase + 1) % 109;
        --tone->frames_left;
    }
}

static SDL_GameController *open_controller(int index) {
    if (!SDL_IsGameController(index)) return NULL;
    SDL_GameController *pad = SDL_GameControllerOpen(index);
    if (!pad) {
        SDL_Log("controller-open failed: %s", SDL_GetError());
        return NULL;
    }
    SDL_Log("controller-open: %s id=%d", SDL_GameControllerName(pad),
            (int)SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)));
    return pad;
}

int main(int argc, char **argv) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_GameController *pad = NULL;
    SDL_AudioDeviceID audio = 0;
    SDL_AudioSpec wanted, obtained;
    ToneState tone = {0, 0};
    int result = 1, running = 1, saw_controller = 0, controller_ready = 0;
    Uint8 shade = 32;

    if (argc == 2 && !strcmp(argv[1], "--tone")) tone.frames_left = 48000;
    else if (argc != 1) {
        fprintf(stderr, "usage: %s [--tone]\n", argv[0]);
        return 1;
    }
    SDL_SetMainReady();
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    SDL_Log("PE SDL2 diagnostic bring-up; NOT THE GAME; untested on console");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        SDL_Log("video/timer initialization failed: %s", SDL_GetError());
        goto cleanup;
    }
    SDL_Log("video-driver=%s", SDL_GetCurrentVideoDriver());
    window = SDL_CreateWindow("PE PS5 platform bring-up", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN);
    if (!window) { SDL_Log("window failed: %s", SDL_GetError()); goto cleanup; }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { SDL_Log("software renderer failed: %s", SDL_GetError()); goto cleanup; }

    controller_ready = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    if (!controller_ready) SDL_Log("controller initialization failed: %s", SDL_GetError());
    else {
        for (int i = 0; i < SDL_NumJoysticks() && !pad; ++i) pad = open_controller(i);
        saw_controller = pad != NULL;
        if (!pad) SDL_Log("controller pending: connect a controller; OPTIONS exits");
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) SDL_Log("audio initialization failed: %s", SDL_GetError());
    else {
        SDL_zero(wanted);
        wanted.freq = 48000;
        wanted.format = AUDIO_S16SYS;
        wanted.channels = 2;
        wanted.samples = 512;
        wanted.callback = audio_callback;
        wanted.userdata = &tone;
        /* No format changes: the callback above has an exact S16 stereo contract. */
        audio = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
        if (!audio) SDL_Log("audio-open failed: %s", SDL_GetError());
        else {
            SDL_Log("audio-open driver=%s rate=%d channels=%u; %s",
                    SDL_GetCurrentAudioDriver(), obtained.freq, obtained.channels,
                    tone.frames_left ? "one-second low-volume signal" : "silence");
            SDL_PauseAudioDevice(audio, 0);
        }
    }

    SDL_Log("clear loop: CROSS changes color, OPTIONS or Escape exits; no auto-close");
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) running = 0;
            else if (event.type == SDL_CONTROLLERDEVICEADDED && !pad) {
                pad = open_controller(event.cdevice.which);
                if (pad) saw_controller = 1;
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && pad &&
                       event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
                SDL_GameControllerClose(pad);
                pad = NULL;
                SDL_Log("controller removed");
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN && pad &&
                       event.cbutton.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
                SDL_Log("controller button=%u", event.cbutton.button);
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) running = 0;
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) shade = (Uint8)(shade + 48);
            }
        }
        if (!running) break;
        if (SDL_SetRenderDrawColor(renderer, shade, 32, 80, 255) < 0 || SDL_RenderClear(renderer) < 0) {
            SDL_Log("clear failed: %s", SDL_GetError());
            goto cleanup;
        }
        /* SDL2 RenderPresent returns void; visible output requires owner validation. */
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    result = audio && saw_controller ? 0 : 2;
    SDL_Log("diagnostic exit=%d; game integration and console validation still required", result);

cleanup:
    /* Closing the audio device synchronizes with its callback before stack teardown. */
    if (audio) SDL_CloseAudioDevice(audio);
    if (pad) SDL_GameControllerClose(pad);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}

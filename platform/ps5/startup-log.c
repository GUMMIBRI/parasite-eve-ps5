/* Opt-in game diagnostics. No signal handler: the last flushed stage is evidence. */
#include <SDL3/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

static int log_fd = -1;
static void stage(const char *fmt, ...) {
    if (log_fd < 0) return;
    int saved_errno = errno;
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t len = (size_t)n < sizeof(line) ? (size_t)n : sizeof(line)-1;
        size_t done = 0;
        while (done < len) {
            ssize_t w = write(log_fd, line + done, len - done);
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) break;
            done += (size_t)w;
        }
        fsync(log_fd);
    }
    errno = saved_errno;
}
static void SDLCALL sdl_log(void *userdata, int category, SDL_LogPriority priority, const char *message) {
    (void)userdata;
    stage("SDL category=%d priority=%d: %s\n", category, (int)priority, message);
}
int __real_main(int, char **);
int __wrap_main(int argc, char **argv) {
    const char *path = "/data/parasite-eve/startup.log";
    log_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    int primary_error = log_fd < 0 ? errno : 0;
    if (log_fd < 0) {
        path = "/data/parasite-eve-startup.log";
        log_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    }
    stage("\n=== PE startup debug: entered main argc=%d path=%s primary_open_errno=%d ===\n", argc, path, primary_error);
    if (log_fd >= 0) {
        int out = dup2(log_fd, STDOUT_FILENO);
        int err = dup2(log_fd, STDERR_FILENO);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        stage("stdout redirect=%d stderr redirect=%d\n", out, err);
    }
    stage("before SDL_SetLogOutputFunction\n");
    SDL_SetLogOutputFunction(sdl_log, NULL);
    stage("before game main\n");
    int result = __real_main(argc, argv);
    stage("game main returned=%d\n", result);
    return result;
}
#define INIT_WRAP(name) \
    bool __real_##name(SDL_InitFlags); \
    bool __wrap_##name(SDL_InitFlags flags) { \
        stage("before " #name " flags=0x%x\n", (unsigned)flags); \
        bool ok = __real_##name(flags); \
        stage("after " #name " result=%d error=%s\n", ok, ok ? "" : SDL_GetError()); \
        return ok; \
    }
INIT_WRAP(SDL_Init)
INIT_WRAP(SDL_InitSubSystem)
SDL_Window *__real_SDL_CreateWindow(const char *, int, int, SDL_WindowFlags);
SDL_Window *__wrap_SDL_CreateWindow(const char *title, int w, int h, SDL_WindowFlags flags) {
    stage("before SDL_CreateWindow %dx%d\n", w, h);
    SDL_Window *p = __real_SDL_CreateWindow(title, w, h, flags);
    stage("after SDL_CreateWindow result=%p error=%s\n", (void *)p, p ? "" : SDL_GetError());
    return p;
}
SDL_Surface *__real_SDL_GetWindowSurface(SDL_Window *);
SDL_Surface *__wrap_SDL_GetWindowSurface(SDL_Window *w) {
    static int first = 1;
    int trace = first; first = 0;
    if (trace) stage("before first SDL_GetWindowSurface\n");
    SDL_Surface *s = __real_SDL_GetWindowSurface(w);
    if (trace || !s) stage("after SDL_GetWindowSurface result=%p error=%s\n", (void *)s, s ? "" : SDL_GetError());
    return s;
}
bool __real_SDL_UpdateWindowSurface(SDL_Window *);
bool __wrap_SDL_UpdateWindowSurface(SDL_Window *w) {
    static int first = 1;
    int trace = first; first = 0;
    if (trace) stage("before first SDL_UpdateWindowSurface\n");
    bool ok = __real_SDL_UpdateWindowSurface(w);
    if (trace || !ok) stage("after SDL_UpdateWindowSurface result=%d error=%s\n", ok, ok ? "" : SDL_GetError());
    return ok;
}
SDL_AudioDeviceID __real_SDL_OpenAudioDevice(SDL_AudioDeviceID, const SDL_AudioSpec *);
SDL_AudioDeviceID __wrap_SDL_OpenAudioDevice(SDL_AudioDeviceID id, const SDL_AudioSpec *spec) {
    stage("before SDL_OpenAudioDevice id=%u\n", (unsigned)id);
    SDL_AudioDeviceID p = __real_SDL_OpenAudioDevice(id, spec);
    stage("after SDL_OpenAudioDevice result=%u error=%s\n", (unsigned)p, p ? "" : SDL_GetError());
    return p;
}
SDL_Gamepad *__real_SDL_OpenGamepad(SDL_JoystickID);
SDL_AudioStream *__real_SDL_OpenAudioDeviceStream(SDL_AudioDeviceID, const SDL_AudioSpec *, SDL_AudioStreamCallback, void *);
SDL_AudioStream *__wrap_SDL_OpenAudioDeviceStream(SDL_AudioDeviceID id, const SDL_AudioSpec *spec, SDL_AudioStreamCallback cb, void *userdata) {
    stage("before SDL_OpenAudioDeviceStream id=%u\n", (unsigned)id);
    SDL_AudioStream *p = __real_SDL_OpenAudioDeviceStream(id, spec, cb, userdata);
    stage("after SDL_OpenAudioDeviceStream result=%p error=%s\n", (void *)p, p ? "" : SDL_GetError());
    return p;
}
bool __real_SDL_ResumeAudioStreamDevice(SDL_AudioStream *);
bool __wrap_SDL_ResumeAudioStreamDevice(SDL_AudioStream *stream) {
    stage("before SDL_ResumeAudioStreamDevice\n");
    bool ok = __real_SDL_ResumeAudioStreamDevice(stream);
    stage("after SDL_ResumeAudioStreamDevice result=%d error=%s\n", ok, ok ? "" : SDL_GetError());
    return ok;
}
SDL_Gamepad *__wrap_SDL_OpenGamepad(SDL_JoystickID id) {
    stage("before SDL_OpenGamepad id=%u\n", (unsigned)id);
    SDL_Gamepad *p = __real_SDL_OpenGamepad(id);
    stage("after SDL_OpenGamepad result=%p error=%s\n", (void *)p, p ? "" : SDL_GetError());
    return p;
}
#define VOID_WRAP(name) \
    void __real_##name(void); \
    void __wrap_##name(void) { stage("before " #name "\n"); __real_##name(); stage("after " #name "\n"); }
#define INT_WRAP(name) \
    int __real_##name(void); \
    int __wrap_##name(void) { stage("before " #name "\n"); int r=__real_##name(); stage("after " #name " result=%d\n",r); return r; }
VOID_WRAP(Boot_InitSystem)
VOID_WRAP(Boot_InitGameState)
VOID_WRAP(Boot_InitSubsystems)
INT_WRAP(Asset_OpenPeImage)
INT_WRAP(Scene_LoadSceneData)

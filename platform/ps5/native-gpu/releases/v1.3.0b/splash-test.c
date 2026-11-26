#include <assert.h>
#include <stdio.h>
typedef unsigned long long Uint64;
static Uint64 ticks=100;static int delays,hides;
static Uint64 SDL_GetTicks(void){return ticks;}
static void SDL_Delay(unsigned ms){assert(ms==2000);ticks+=ms;delays++;}
static int __wrap_sceSystemServiceHideSplashScreen(void){hides++;return 0;}
#define stage(...) ((void)0)
static void pe_dismiss_launch_splash(void) {
    static int attempted;
    if(attempted)return;
    attempted=1;
    const Uint64 started=SDL_GetTicks();
    SDL_Delay(2000);
    const int result=__wrap_sceSystemServiceHideSplashScreen();
    stage("launcher splash dismiss after=%llu ms result=0x%08x once=1\n",
          (unsigned long long)(SDL_GetTicks()-started),(unsigned)result);
}
typedef struct {int dummy;} SDL_Window;
typedef unsigned long long SDL_WindowFlags;
static int window_ok;static SDL_Window window;
static const char*SDL_GetError(void){return "mock error";}
static SDL_Window*__real_SDL_CreateWindow(const char*t,int w,int h,SDL_WindowFlags f){(void)t;(void)w;(void)h;(void)f;return window_ok?&window:0;}
SDL_Window *__wrap_SDL_CreateWindow(const char *title, int w, int h, SDL_WindowFlags flags) {
    stage("before SDL_CreateWindow %dx%d\n", w, h);
    SDL_Window *p = __real_SDL_CreateWindow(title, w, h, flags);
    stage("after SDL_CreateWindow result=%p error=%s\n", (void *)p, p ? "" : SDL_GetError());
    if(p)pe_dismiss_launch_splash();
    return p;
}
int main(void){assert(!__wrap_SDL_CreateWindow("test",1,1,0));assert(!delays&&!hides);window_ok=1;assert(__wrap_SDL_CreateWindow("test",1,1,0)==&window);assert(ticks==2100&&delays==1&&hides==1);assert(__wrap_SDL_CreateWindow("test",1,1,0)==&window);assert(ticks==2100&&delays==1&&hides==1);puts("actual wrapper:failed window leaves splash; first success delays2000ms then hides once; later windows untouched PASS");}

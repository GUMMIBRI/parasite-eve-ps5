# Apply only to exported PsyZ sources; preserve the pinned checkout and notices.
param([Parameter(Mandatory=$true)][string]$Stage, [string]$OriginalSource)
$ErrorActionPreference='Stop'
$p=Join-Path $Stage 'psyz/src/platform/sdl3_gpu.c'
$original=if($OriginalSource){$OriginalSource}else{Join-Path $PSScriptRoot '../../third_party/psyz/psyz/src/platform/sdl3_gpu.c'}
$s=[IO.File]::ReadAllText($original).Replace("`r`n","`n")
$state='static bool hidden_gpu_renderer;'
if(!$s.Contains($state)){throw 'Missing renderer state anchor'}
$s=$s.Replace($state,$state+"`n#ifdef PE_PS5`nstatic bool ps5_interactive_software;`n#endif")
$old='    headless_renderer = headless && strcmp(headless, "1") == 0;'
$new=@'
    headless_renderer = headless && strcmp(headless, "1") == 0;
#ifdef PE_PS5
    if (headless && strcmp(headless, "gpu") == 0) {
        ERRORF("PS5 GPU headless mode is unavailable in the software build");
        return false;
    }
    ps5_interactive_software = !headless_renderer;
    if (ps5_interactive_software) {
    // PE_PS5 software presentation uses the existing CPU PS1 rasterizer.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        ERRORF("SDL_Init: %s", SDL_GetError());
        return false;
    }
    sdl3_window = SDL_CreateWindow(window_title, 640, 480, 0);
    if (!sdl3_window || !SDL_GetWindowSurface(sdl3_window)) {
        ERRORF("PS5 software window: %s", SDL_GetError());
        return false;
    }
    headless_renderer = true; // routes primitives to the software rasterizer
    hidden_gpu_renderer = false;
    SoftwareRasterizer_Reset();
    vsync_mode = PSYZ_VSYNC_OFF; // common timing code paces the CPU renderer
    Sdl3Common_TimingInit();
    is_window_visible = true;
    is_platform_init_successful = true;
    return true;
    }
#endif
'@
if(!$s.Contains($old)){throw 'Missing renderer init anchor'}
$s=$s.Replace($old,$new)
$old="static void PlatformBackend_Present(void) {`n    if (UseSoftwareBackend()) {"
$new=@'
static void PlatformBackend_Present(void) {
    if (UseSoftwareBackend()) {
#ifdef PE_PS5
        // Reuse existing 15-bit VRAM and RGB24 conversion for display.
        if (ps5_interactive_software && sdl3_window && !present_skip) {
            SDL_Surface *dst = SDL_GetWindowSurface(sdl3_window);
            if (dst) {
                SDL_FillSurfaceRect(dst, NULL, 0);
                if (disp_on) {
                    int w, h;
                    unsigned char *pixels = Psyz_VideoAllocCapturedFrame(&w, &h);
                    if (pixels && w > 0 && h > 0) {
                        SDL_Surface *src = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGB24, pixels, w * 3);
                        if (src) {
                            SDL_BlitSurfaceScaled(src, NULL, dst, NULL, SDL_SCALEMODE_NEAREST);
                            SDL_DestroySurface(src);
                        }
                    }
                    free(pixels);
                }
                if (!SDL_UpdateWindowSurface(sdl3_window))
                    ERRORF("PS5 present: %s", SDL_GetError());
            }
        }
#endif
'@
if(!$s.Contains($old)){throw 'Missing renderer present anchor'}
$s=$s.Replace($old,$new)
# Reject GPU-only options before they can request textures on a NULL device.
$guards=@{
 'int Psyz_VideoSetInternalResolution(unsigned multiplier) {'='if (multiplier != 1) return -1;'
 'int Psyz_VideoSetGeometryCorrection(int enabled) {'='if (enabled) return -1;'
 'int Psyz_VideoSetAntialiasing(int enabled) {'='if (enabled) return -1;'
 'int Psyz_VideoSetModelFiltering(int enabled) {'='if (enabled) return -1;'
 'int Psyz_VideoSetWidescreen(int enabled) {'='if (enabled) return -1;'
}
foreach($anchor in $guards.Keys){
 if(!$s.Contains($anchor)){throw "Missing software feature guard anchor: $anchor"}
 $s=$s.Replace($anchor,$anchor+"`n#ifdef PE_PS5`n    "+$guards[$anchor]+"`n#endif")
}
[IO.File]::WriteAllText($p,$s)

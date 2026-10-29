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
        SDL_strlcpy(ps5_platform_init_error,
                    "PS5 GPU headless mode is unavailable in the software build",
                    sizeof(ps5_platform_init_error));
        ERRORF("%s", ps5_platform_init_error);
        return false;
    }
    ps5_interactive_software = !headless_renderer;
    if (ps5_interactive_software) {
    // PE_PS5 software presentation uses the existing CPU PS1 rasterizer.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        SDL_strlcpy(ps5_platform_init_error, SDL_GetError(),
                    sizeof(ps5_platform_init_error));
        ERRORF("SDL_Init: %s", ps5_platform_init_error);
        return false;
    }
    sdl3_window = SDL_CreateWindow(window_title, 640, 480, 0);
    if (!sdl3_window || !SDL_GetWindowSurface(sdl3_window)) {
        SDL_strlcpy(ps5_platform_init_error, SDL_GetError(),
                    sizeof(ps5_platform_init_error));
        ERRORF("PS5 software window: %s", ps5_platform_init_error);
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
# These checked transformations are regenerated from the selected original on
# every invocation, including Resume; no private candidate input is required.
function ReplaceUnique([string]$Text,[string]$Old,[string]$New) {
 if(([regex]::Matches($Text,[regex]::Escape($Old))).Count -ne 1){throw "Expected unique platform anchor: $Old"}
 return $Text.Replace($Old,$New)
}
$status=@'
#ifdef PE_PS5
/* Snapshot before later boot/audio calls can replace SDL's thread error. */
static char ps5_platform_init_error[1024];
int Psyz_PS5PlatformReady(void) {
    return is_platform_init_successful &&
           (!ps5_interactive_software || sdl3_window != NULL);
}
const char *Psyz_PS5PlatformInitError(void) {
    return ps5_platform_init_error[0] ? ps5_platform_init_error :
           "platform did not complete initialization";
}
#endif

'@
$s=ReplaceUnique $s 'bool InitPlatform() {' ($status+"`n"+'bool InitPlatform() {')
$s=ReplaceUnique $s "        if (!SDL_Init(SDL_INIT_EVENTS))`n            return false;" @'
        if (!SDL_Init(SDL_INIT_EVENTS)) {
#ifdef PE_PS5
            SDL_strlcpy(ps5_platform_init_error, SDL_GetError(),
                        sizeof(ps5_platform_init_error));
#endif
            return false;
        }
'@
$s=ReplaceUnique $s "    return headless_renderer;`n}" @'
#ifdef PE_PS5
    /* CPU mode remains CPU even when initialization failed. Readiness is
     * reported separately; never route a failed CPU boot to GPU. */
    return ps5_interactive_software || headless_renderer;
#else
    return headless_renderer;
#endif
}
'@
# The headless CPU backend never needed a display mask; the PS5 presenter does.
$s=ReplaceUnique $s "void Draw_DisplayEnable(unsigned int on) {`n    if (UseSoftwareBackend())`n        return;" @'
void Draw_DisplayEnable(unsigned int on) {
    if (UseSoftwareBackend()) {
#ifdef PE_PS5
        /* Interactive software presentation still obeys the PS1 display mask. */
        if (ps5_interactive_software)
            disp_on = on;
#endif
        return;
    }
'@
[IO.File]::WriteAllText($p,$s)
# Card initialization must use the same absolute mapping as all card I/O.
# Patch only the exported revision; never mutate the pinned PsyZ checkout.
$card=Join-Path $Stage 'psyz/src/psyz/libcard.c'
$cardText=[IO.File]::ReadAllText($card).Replace("`r`n","`n")
$cardAnchor="void _bu_init(void) {`n#ifdef _MSC_VER"
$cardReplacement=@'
void _bu_init(void) {
#ifdef PE_PS5
    char path[4096];
    Psyz_AdjustPath(path, "bu00:*", sizeof(path));
    Psyz_AdjustPath(path, "bu10:*", sizeof(path));
#elif defined(_MSC_VER)
'@
# Validate the entire selected original initializer, or exactly this recipe's
# result. A Resume stage retains libcard.c, unlike the regenerated GPU source.
$cardOriginal=@'
void _bu_init(void) {
#ifdef _MSC_VER
    NOT_IMPLEMENTED;
#elif __MINGW32__
    mkdir("bu00");
    mkdir("bu10");
#else
    mkdir("bu00", 0755);
    mkdir("bu10", 0755);
#endif
}
'@
$cardPatched=$cardOriginal.Replace($cardAnchor,$cardReplacement)
$originalCount=([regex]::Matches($cardText,[regex]::Escape($cardOriginal))).Count
$patchedCount=([regex]::Matches($cardText,[regex]::Escape($cardPatched))).Count
$initCount=([regex]::Matches($cardText,'\bvoid\s+_bu_init\s*\(')).Count
if($initCount -ne 1 -or ($originalCount+$patchedCount) -ne 1){throw 'Unknown or non-unique card initialization form'}
if($originalCount -eq 1){[IO.File]::WriteAllText($card,$cardText.Replace($cardOriginal,$cardPatched))}


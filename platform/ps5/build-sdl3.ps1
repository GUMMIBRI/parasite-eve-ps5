# Build a staged, public-only SDL3 PS5 static library. No pinned submodule edits.
[CmdletBinding()]
param(
    [string]$Sdk = "$PSScriptRoot/tools/v0.45/ps5-payload-sdk",
    [string]$LlvmBin = "$PSScriptRoot/tools/llvm-18.1.8/bin",
    [string]$CMake = 'cmake',
    [string]$Ninja = 'ninja',
    [string]$BuildRoot = 'C:/.pe/tools/pe-ps5-sdl3',
    [switch]$WithoutJoystick
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$sdl = (Resolve-Path "$root/third_party/psyz/external/SDL").Path
$Sdk = (Resolve-Path $Sdk).Path
$LlvmBin = (Resolve-Path $LlvmBin).Path
$CMake = (Get-Command $CMake -ErrorAction Stop).Source
$Ninja = (Get-Command $Ninja -ErrorAction Stop).Source
$pin = 'fa2c02bb6e21974a89ea9824bc53c9932abe5f9c'
$actual = & git -C $sdl rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $pin) { throw 'Unexpected pinned SDL3 revision' }
$dirty = & git -C $sdl status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Pinned SDL3 tracked sources must be clean' }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
if ($BuildRoot -match '\s' -or $Sdk -match '\s') { throw 'Public linker requires paths without whitespace' }
New-Item -ItemType Directory -Force $BuildRoot | Out-Null
$out = Join-Path $BuildRoot ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 6))
New-Item -ItemType Directory $out | Out-Null
$source = Join-Path $out 'src'
$build = Join-Path $out 'b'
$install = Join-Path $out 'install'
New-Item -ItemType Directory $source | Out-Null
$oldPath = $env:PATH
$oldSdk = $env:PS5_PAYLOAD_SDK
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments 2>&1 | Tee-Object -FilePath "$out/native.log" -Append
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE); see $out/native.log" }
}
function Replace-Once([string]$Path, [string]$Old, [string]$New) {
    $text = [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
    if ($text.IndexOf($Old) -lt 0 -or $text.IndexOf($Old) -ne $text.LastIndexOf($Old)) { throw "Non-unique source patch anchor in $Path" }
    [IO.File]::WriteAllText($Path, $text.Replace($Old, $New))
}
try {
    $env:PATH = "$LlvmBin;$oldPath"
    $env:PS5_PAYLOAD_SDK = $Sdk
    Run 'git' @('-C', $sdl, 'archive', '--format=zip', "--output=$out/source.zip", $pin)
    Expand-Archive -LiteralPath "$out/source.zip" -DestinationPath $source
    $backends = @('audio', 'video')
    if (!$WithoutJoystick) { $backends += 'joystick' }
    foreach ($backend in $backends) {
        $overlay = Join-Path "$PSScriptRoot/sdl3" $backend
        if (!(Test-Path -LiteralPath $overlay)) { throw "Backend source not delivered yet: $overlay" }
        $destination = Join-Path $source "src/$backend/ps5"
        New-Item -ItemType Directory -Force $destination | Out-Null
        Get-ChildItem -LiteralPath $overlay -File | Where-Object Extension -in '.c','.h' | Copy-Item -Destination $destination
    }
    Copy-Item -LiteralPath "$PSScriptRoot/sdl3/platform.cmake" -Destination "$source/pe-public-ps5.cmake"
    Replace-Once "$source/cmake/sdlplatform.cmake" '  if(WIN32)' "  if(PS5)`n    set(sdl_cmake_platform PS5)`n  elseif(WIN32)"
    Replace-Once "$source/CMakeLists.txt" ("`n" + 'elseif(UNIX AND NOT APPLE AND NOT RISCOS AND NOT HAIKU)') ("`n" + 'elseif(PS5)' + "`n" + '  include("${SDL3_SOURCE_DIR}/pe-public-ps5.cmake")' + "`n`n" + 'elseif(UNIX AND NOT APPLE AND NOT RISCOS AND NOT HAIKU)')
    Replace-Once "$source/src/audio/SDL_audio.c" '#ifdef SDL_AUDIO_DRIVER_PS2' "#ifdef SDL_AUDIO_DRIVER_PS5`n    &PS5AUDIO_bootstrap,`n#endif`n#ifdef SDL_AUDIO_DRIVER_PS2"
    Replace-Once "$source/src/audio/SDL_sysaudio.h" 'extern AudioBootStrap PS2AUDIO_bootstrap;' "extern AudioBootStrap PS5AUDIO_bootstrap;`nextern AudioBootStrap PS2AUDIO_bootstrap;"
    Replace-Once "$source/src/video/SDL_video.c" '#ifdef SDL_VIDEO_DRIVER_PSP' "#ifdef SDL_VIDEO_DRIVER_PS5`n    &PS5_bootstrap,`n#endif`n#ifdef SDL_VIDEO_DRIVER_PSP"
    Replace-Once "$source/src/video/SDL_sysvideo.h" 'extern VideoBootStrap PSP_bootstrap;' "extern VideoBootStrap PS5_bootstrap;`nextern VideoBootStrap PSP_bootstrap;"
    $cfg = "$source/include/build_config/SDL_build_config.h.cmake"
    Replace-Once $cfg '#cmakedefine SDL_AUDIO_DRIVER_PS2 1' "#cmakedefine SDL_AUDIO_DRIVER_PS5 1`n#cmakedefine SDL_AUDIO_DRIVER_PS2 1"
    Replace-Once $cfg '#cmakedefine SDL_VIDEO_DRIVER_PSP 1' "#cmakedefine SDL_VIDEO_DRIVER_PS5 1`n#cmakedefine SDL_VIDEO_DRIVER_PSP 1"
    if (!$WithoutJoystick) {
        Replace-Once "$source/src/joystick/SDL_joystick.c" '#ifdef SDL_JOYSTICK_PSP' "#ifdef SDL_JOYSTICK_PS5`n    &SDL_PS5_JoystickDriver,`n#endif`n#ifdef SDL_JOYSTICK_PSP"
        Replace-Once "$source/src/joystick/SDL_sysjoystick.h" 'extern SDL_JoystickDriver SDL_PSP_JoystickDriver;' "extern SDL_JoystickDriver SDL_PS5_JoystickDriver;`nextern SDL_JoystickDriver SDL_PSP_JoystickDriver;"
        Replace-Once $cfg '#cmakedefine SDL_JOYSTICK_PSP 1' "#cmakedefine SDL_JOYSTICK_PS5 1`n#cmakedefine SDL_JOYSTICK_PSP 1"
    }
    $overlayHashes = foreach ($backend in $backends) {
        foreach ($file in (Get-ChildItem -LiteralPath "$source/src/$backend/ps5" -File)) {
            @{ File="$backend/$($file.Name)"; SHA256=(Get-FileHash -LiteralPath $file.FullName).Hash }
        }
    }
    @{ SDL3Revision=$pin; OverlayFiles=@($overlayHashes); PlatformFragmentSHA256=(Get-FileHash "$source/pe-public-ps5.cmake").Hash } | ConvertTo-Json -Depth 5 | Set-Content "$out/SOURCES.json"
    $joystick = if ($WithoutJoystick) { 'OFF' } else { 'ON' }
    Run $CMake @('-S', $source, '-B', $build, '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_TOOLCHAIN_FILE=$Sdk/toolchain/prospero.cmake", "-DPS5_PAYLOAD_SDK=$Sdk", "-DCMAKE_AR=$LlvmBin/llvm-ar.exe", "-DCMAKE_RANLIB=$LlvmBin/llvm-ranlib.exe", "-DCMAKE_INSTALL_PREFIX=$install", '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_FLAGS=-std=gnu11', '-DSDL_SHARED=OFF', '-DSDL_STATIC=ON', '-DSDL_TESTS=OFF', '-DSDL_TEST_LIBRARY=OFF', '-DSDL_EXAMPLES=OFF', '-DSDL_AUDIO=ON', '-DSDL_VIDEO=ON', "-DSDL_JOYSTICK=$joystick", '-DSDL_HIDAPI=OFF', '-DSDL_HAPTIC=OFF', '-DSDL_SENSOR=OFF', '-DSDL_CAMERA=OFF', '-DSDL_GPU=OFF', '-DSDL_OPENGL=OFF', '-DSDL_OPENGLES=OFF', '-DSDL_VULKAN=OFF', '-DSDL_POWER=OFF', '-DSDL_DIALOG=OFF', '-DSDL_TRAY=OFF', '-DSDL_DUMMYAUDIO=OFF', '-DSDL_DUMMYVIDEO=OFF', '-DSDL_UNIX_CONSOLE_BUILD=ON', '-DSDL_OFFSCREEN=OFF', '-DSDL_DISKAUDIO=OFF', '-DSDL_PTHREADS=ON', '-DSDL_PTHREADS_PRIVATE=ON', '-DSDL_ASSEMBLY=OFF', '-DSDL_ALSA=OFF', '-DSDL_OSS=OFF', '-DSDL_PIPEWIRE=OFF', '-DSDL_PULSEAUDIO=OFF', '-DSDL_JACK=OFF', '-DSDL_X11=OFF', '-DSDL_WAYLAND=OFF', '-DSDL_KMSDRM=OFF')
    Run $CMake @('--build', $build, '--parallel', '4')
    Run $CMake @('--install', $build)
    $header = [IO.File]::ReadAllText("$build/include-config-release/build_config/SDL_build_config.h")
    foreach ($macro in @('SDL_AUDIO_DRIVER_PS5', 'SDL_VIDEO_DRIVER_PS5', 'SDL_THREAD_PTHREAD', 'SDL_TIMER_UNIX')) {
        if ($header -notmatch "(?m)^#define $macro 1") { throw "Required PS5 backend missing: $macro" }
    }
    if (!$WithoutJoystick -and $header -notmatch '(?m)^#define SDL_JOYSTICK_PS5 1') { throw 'PS5 joystick registration missing' }
    $library = Get-ChildItem -LiteralPath $build -Filter libSDL3.a -Recurse | Select-Object -First 1
    if (!$library) { throw 'No SDL3 static library produced' }
    $symbols = & "$LlvmBin/llvm-nm.exe" --defined-only $library.FullName
    if ($LASTEXITCODE -ne 0) { throw 'SDL3 symbol inspection failed' }
    $symbols | Set-Content "$out/DEFINED-SYMBOLS.txt"
    foreach ($symbol in @('PS5AUDIO_bootstrap', 'PS5_bootstrap')) { if (($symbols -join "`n") -notmatch "\b$symbol\b") { throw "Missing real backend symbol: $symbol" } }
    @{ SDL3Revision=$pin; Output=$out; StaticLibrary=$library.FullName; SHA256=(Get-FileHash $library.FullName).Hash; JoystickEnabled=(!$WithoutJoystick); Configuration='Public PS5 software VideoOut + AudioOut; no GL/GPU; untested on console' } | ConvertTo-Json | Set-Content "$out/RESULT.json"
    Write-Output "SDL3 PS5 static library built: $out"
} finally {
    $env:PATH = $oldPath
    $env:PS5_PAYLOAD_SDK = $oldSdk
}

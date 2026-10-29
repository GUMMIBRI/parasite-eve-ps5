# Public source-only dependencies; no deployment. Existing SDK/source trees untouched.
[CmdletBinding()]
param(
    [string]$Sdk = "$PSScriptRoot/tools/v0.45/ps5-payload-sdk",
    [string]$LlvmBin = "$PSScriptRoot/tools/llvm-18.1.8/bin",
    [string]$SDLSource = "$PSScriptRoot/../../../ps5-coordination/public-reference/SDL-ps5",
    [string]$CMake = 'cmake',
    [string]$Ninja = 'ninja'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script:SdlNativeLog = $null
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    if ($script:SdlNativeLog) {
        & $Program @Arguments 2>&1 | Tee-Object -FilePath $script:SdlNativeLog -Append
    } else {
        & $Program @Arguments
    }
    if ($LASTEXITCODE -ne 0) { throw "$Program failed: $LASTEXITCODE" }
}
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$Sdk = (Resolve-Path $Sdk).Path
$LlvmBin = (Resolve-Path $LlvmBin).Path
$SDLSource = (Resolve-Path $SDLSource).Path
$CMake = (Get-Command $CMake -ErrorAction Stop).Source
$Ninja = (Get-Command $Ninja -ErrorAction Stop).Source
$pin = & git -c "safe.directory=$($SDLSource.Replace('\','/'))" -C $SDLSource rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $pin -ne 'ee4c47dc0d617b3bc8f35108f9956baf228a1322') { throw 'Unexpected SDL2 source revision' }
$dirty = & git -c "safe.directory=$($SDLSource.Replace('\','/'))" -C $SDLSource status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'SDL2 tracked source must be clean' }
$version = & "$LlvmBin/clang.exe" --version
if ($LASTEXITCODE -ne 0 -or $version[0] -notmatch 'clang version 18\.1\.8') { throw 'LLVM18.1.8 required' }
$cache = "$root/outputs/ps5/sdl2-deps"
New-Item -ItemType Directory -Force $cache | Out-Null
'*' | Set-Content "$cache/.gitignore"
$archive = "$cache/libsamplerate-0.2.2.tar.xz"
if (!(Test-Path -LiteralPath $archive)) {
    Invoke-Checked 'curl.exe' @('-L','--fail','--output',$archive,'https://github.com/libsndfile/libsamplerate/releases/download/0.2.2/libsamplerate-0.2.2.tar.xz')
}
if ((Get-FileHash $archive).Hash -ne '3258da280511d24b49d6b08615bbe824d0cacc9842b0e4caf11c52cf2b043893') { throw 'PacBrew libsamplerate archive hash mismatch' }
$out = "$root/outputs/ps5/sdl2-" + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
New-Item -ItemType Directory $out | Out-Null
'*' | Set-Content "$out/.gitignore"
if ($out -match '\s' -or $Sdk -match '\s') { throw 'SDK linker requires paths without whitespace' }
$oldPath = $env:PATH
$oldSdk = $env:PS5_PAYLOAD_SDK
Start-Transcript "$out/build.log" | Out-Null
$script:SdlNativeLog = "$out/native.log"
try {
    $env:PATH = "$LlvmBin;$oldPath"
    $env:PS5_PAYLOAD_SDK = $Sdk
    Invoke-Checked 'tar.exe' @('-xf',$archive,'-C',$out)
    Copy-Item "$SDLSource/LICENSE.txt" "$out/SDL2-LICENSE.txt"
    Copy-Item "$out/libsamplerate-0.2.2/COPYING" "$out/libsamplerate-COPYING.txt"
    @{
        SDL2Revision = $pin
        PacBrewRecipeRevision = '722187f0ee246c0d7d57401c8a97de1c7f4e2ff0'
        LibsamplerateVersion = '0.2.2'
        LibsamplerateSHA256 = (Get-FileHash $archive).Hash
        SDK = $Sdk
        Clang = $version[0]
        BringupSHA256 = (Get-FileHash "$root/platform/ps5/sdl2_bringup.c").Hash
        Configuration = 'Static SDL2; software renderer; OpenGL/OpenGLES/Vulkan disabled; upstream sources unchanged'
    } | ConvertTo-Json | Set-Content "$out/SOURCES.json"
    $cmakeInclude = "$root/cmake/ps5/sdl2-build.cmake".Replace('\','/')
    "cmake_minimum_required(VERSION 3.16)`nproject(pe_ps5_sdl2 C)`ninclude(`"$cmakeInclude`")" | Set-Content "$out/CMakeLists.txt"
    Invoke-Checked $CMake @('-S',$out,'-B',"$out/build",'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_TOOLCHAIN_FILE=$Sdk/toolchain/prospero.cmake", "-DPS5_PAYLOAD_SDK=$Sdk", "-DCMAKE_AR=$LlvmBin/llvm-ar.exe", "-DCMAKE_RANLIB=$LlvmBin/llvm-ranlib.exe",'-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DCMAKE_BUILD_TYPE=Release',"-DSDL_SOURCE=$SDLSource", "-DSAMPLERATE_SOURCE=$out/libsamplerate-0.2.2", "-DBRINGUP_SOURCE=$root/platform/ps5/sdl2_bringup.c")
    Invoke-Checked $CMake @('--build',"$out/build",'--parallel','4')
    $config = Get-Content "$out/build/sdl2/include-config-release/SDL2/SDL_config.h" -Raw
    foreach ($macro in @('HAVE_LIBSAMPLERATE_H','SDL_AUDIO_DRIVER_PS5','SDL_JOYSTICK_PS5','SDL_THREAD_PTHREAD','SDL_VIDEO_DRIVER_PS5')) {
        if ($config -notmatch "(?m)^#define $macro 1") { throw "Missing target feature: $macro" }
    }
    $headers = & "$LlvmBin/llvm-readelf.exe" --file-header --program-headers "$out/build/sdl2_bringup.elf"
    if ($LASTEXITCODE -ne 0) { throw 'ELF inspection failed' }
    $headers | Set-Content "$out/ELF-HEADERS.txt"
    $joined = $headers -join "`n"
    foreach ($pattern in @('Class:\s+ELF64','Type:\s+DYN','Machine:\s+Advanced Micro Devices X86-64','Entry point address:\s+0x[0-9a-fA-F]*[1-9a-fA-F][0-9a-fA-F]*','\bLOAD\b')) {
        if ($joined -notmatch $pattern) { throw "ELF validation failed: $pattern" }
    }
    Get-FileHash "$out/build/sdl2_bringup.elf", "$out/build/sdl2/libSDL2.a", "$out/build/samplerate/src/libsamplerate.a" | Select-Object Path,Hash | ConvertTo-Json | Set-Content "$out/SHA256.json"
    'UNTESTED ON CONSOLE. Software rendering; no GL/Vulkan or game assets.' | Set-Content "$out/CONSOLE-UNTESTED.txt"
    "Build passed: $out"
} finally {
    $env:PATH = $oldPath
    $env:PS5_PAYLOAD_SDK = $oldSdk
    Stop-Transcript | Out-Null
}

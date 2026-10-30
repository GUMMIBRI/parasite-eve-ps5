# Build the real game with an installed public SDL3 package, preserving submodules.
[CmdletBinding()]
param(
 [string]$Sdk='',
 [string]$LlvmBin='',
 [string]$SDL3Prefix='C:/.pe/tools/pe-ps5-sdl3/20261029T010754Z-68fad8/install-reviewfix',
 [string]$SDL3Source,
 [string]$SDL3Build,
 [string]$CMake='C:/.pe/tools/cmake/cmake-4.4.4-windows-x86_64/bin/cmake.exe',
 [string]$Ninja='C:/.pe/tools/ninja/ninja.exe',
 [string]$Resume,
 [string]$SourceRoot,
 [string]$OutputRoot,
 [string]$PsyZRevision,
 [switch]$StartupLog,
 [switch]$AllocationLog,
 [switch]$DebugSymbols,
 [string]$ApplicationTooling
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$buildConfiguration=if($DebugSymbols){'RelWithDebInfo'}else{'Release'}
# Never relabel an existing build or reuse objects with a different debug mode.
if($Resume){
 $resumeInputs=Join-Path $Resume 'INPUTS.json'
 if(!(Test-Path -LiteralPath $resumeInputs)){throw 'Resume requires INPUTS.json to verify build configuration'}
 $resumeProvenance=Get-Content -LiteralPath $resumeInputs -Raw | ConvertFrom-Json
 $previousConfiguration=if($resumeProvenance.PSObject.Properties.Name -contains 'BuildConfiguration'){$resumeProvenance.BuildConfiguration}else{'Release'}
 if($previousConfiguration -ne $buildConfiguration){throw 'Resume build configuration changed; use a fresh output or retain DebugSymbols'}
}
if($AllocationLog -and (!$ApplicationTooling -or !$StartupLog)){throw 'AllocationLog requires ApplicationTooling and StartupLog'}
if($ApplicationTooling -and $StartupLog -and $Resume){throw 'Application diagnostics require a fresh output to preserve CRT instrumentation provenance'}
if($ApplicationTooling){$ApplicationTooling=(Resolve-Path -LiteralPath $ApplicationTooling).Path}
$gameSource=if($SourceRoot){(Resolve-Path $SourceRoot).Path}else{$root}
$candidate=$null
if($SourceRoot){
 $candidateFile=Join-Path (Split-Path $gameSource) 'CANDIDATE.json'
 if(!(Test-Path $candidateFile)){throw 'Isolated SourceRoot requires CANDIDATE.json from prepare-upstream-source.ps1'}
 $candidate=Get-Content $candidateFile -Raw | ConvertFrom-Json
 if(!$candidate.Ready -or $candidate.Conflicts.Count){throw 'Candidate still has unresolved conflicts'}
 if($candidate.SourceRoot -ne $gameSource){throw 'Candidate source path does not match its manifest'}
 foreach($file in @('CMakeLists.txt','src/main/boot/main.c')){
  if([IO.File]::ReadAllText("$gameSource/$file") -match '(?m)^(<<<<<<< |=======\s*$|>>>>>>> )'){throw "Unresolved candidate conflict marker: $file"}
 }
}
if(!$Sdk){$Sdk="$PSScriptRoot/tools/v0.45/ps5-payload-sdk"}
if(!$LlvmBin){$LlvmBin="$PSScriptRoot/tools/llvm-18.1.8/bin"}
$Sdk=(Resolve-Path $Sdk).Path
$LlvmBin=(Resolve-Path $LlvmBin).Path
if($ApplicationTooling){
 $applicationInputs=& "$PSScriptRoot/validate-application-inputs.ps1" -NativeSource $ApplicationTooling -Sdk $Sdk
 # Application generation changes CRT/backend inputs; never reuse another ABI tree.
 if($Resume){throw 'Application builds require a fresh output; resume could mix CRT, SDK or SDL inputs'}
}
$SDL3Prefix=(Resolve-Path $SDL3Prefix).Path
if(!$SDL3Source){$SDL3Source=Join-Path (Split-Path $SDL3Prefix) 'src'}
if(!$SDL3Build){$SDL3Build=Join-Path (Split-Path $SDL3Prefix) 'b'}
$SDL3Source=(Resolve-Path $SDL3Source).Path
$SDL3Build=(Resolve-Path $SDL3Build).Path
$CMake=(Get-Command $CMake -ErrorAction Stop).Source
$Ninja=(Get-Command $Ninja -ErrorAction Stop).Source
$pin=& git -C "$root/third_party/psyz" rev-parse HEAD
if($LASTEXITCODE -ne 0){throw 'Cannot identify PsyZ revision'}
if($PsyZRevision){
 $pin=& git -C "$root/third_party/psyz" rev-parse "$PsyZRevision^{commit}"
 if($LASTEXITCODE -ne 0){throw 'Selected PsyZ revision is not available locally'}
}
$dirty=& git -C "$root/third_party/psyz" status --porcelain --untracked-files=no
if($LASTEXITCODE -ne 0 -or $dirty){throw 'PsyZ tracked sources must be clean for generated patches'}
# Resume identifies an existing tree; output selection must never redirect it.
if($Resume -and $PSBoundParameters.ContainsKey('OutputRoot')){throw 'OutputRoot cannot be combined with Resume'}
if(!$Resume){
 if(!$OutputRoot){$OutputRoot=$env:PE_PS5_OUTPUT_ROOT}
 if(!$OutputRoot){$OutputRoot=Join-Path $root 'outputs/ps5'}
 $OutputRoot=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
 if(Test-Path -LiteralPath $OutputRoot -PathType Leaf){throw 'OutputRoot must be a directory, not a file'}
 New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
 $OutputRoot=(Resolve-Path -LiteralPath $OutputRoot).Path
}
$out=if($Resume){(Resolve-Path -LiteralPath $Resume).Path}else{Join-Path $OutputRoot ('game-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N'))}
if($Resume -and (Test-Path "$out/application-project.cmake") -and !$ApplicationTooling){throw 'Application build resume requires ApplicationTooling; use a fresh output for payload ABI'}
if($Resume -and !$StartupLog -and ((Test-Path "$out/startup-project.cmake") -or (Test-Path "$out/STARTUP-LOG.json"))){
 throw 'Cannot disable startup diagnostics when resuming an instrumented build; retain StartupLog or use a fresh output'
}
if($ApplicationTooling -and $Resume -and (Test-Path "$out/startup-project.cmake")){throw 'Cannot reuse payload diagnostics build for application ABI'}
if($Resume){
 if(!(Test-Path -LiteralPath $out -PathType Container)){throw 'Resume must be a directory'}
}else{New-Item -ItemType Directory -Path $out | Out-Null}
if($Resume -and (Test-Path "$out/INPUTS.json")){
 $previous=Get-Content "$out/INPUTS.json" -Raw | ConvertFrom-Json
 if($previous.PsyZRevision -ne $pin){throw 'Resume PsyZ revision changed; use a new isolated build'}
 if($previous.PSObject.Properties.Name -contains 'SourceRoot'){
  if($previous.SourceRoot -ne $gameSource){throw 'Resume SourceRoot changed; use a new isolated build'}
 }elseif($SourceRoot){throw 'Cannot resume a baseline build with isolated sources'}
}
if($candidate -and $candidate.PsyZRevision -ne $pin){throw 'Candidate PsyZ revision differs from selected build revision'}
# Invalidate success before stage/INPUTS mutation; retain prior provenance.
$attempt=[Guid]::NewGuid().ToString('N')
foreach($name in @('RESULT.json','INPUTS.json')){
 if(Test-Path -LiteralPath "$out/$name"){
  Copy-Item -LiteralPath "$out/$name" -Destination "$out/$name.before-$attempt"
 }
}
@{Schema=2; State='building'; Attempt=$attempt; StartedUtc=[DateTime]::UtcNow.ToString('o')} |
 ConvertTo-Json | Set-Content "$out/RESULT.json"
function Run([string]$Program,[string[]]$Arguments){
 & $Program @Arguments 2>&1 | Tee-Object "$out/build.log" -Append
 if($LASTEXITCODE -ne 0){throw "Build failed ($LASTEXITCODE): $out/build.log"}
}
function Patch([string]$File,[string]$Old,[string]$New){
 $s=[IO.File]::ReadAllText($File).Replace("`r`n","`n")
 if(!$s.Contains($Old)){throw "Patch anchor absent: $File"}
 [IO.File]::WriteAllText($File,$s.Replace($Old,$New))
}
$oldPath=$env:PATH
$oldSdk=$env:PS5_PAYLOAD_SDK
try{
 $env:PATH="$LlvmBin;$oldPath"
 $env:PS5_PAYLOAD_SDK=$Sdk
 $stage="$out/psyz-src"
 if(!$Resume){
  New-Item -ItemType Directory $stage | Out-Null
  Run git @('-C',"$root/third_party/psyz",'archive','--format=zip',"--output=$out/psyz.zip",$pin)
  Expand-Archive "$out/psyz.zip" $stage
  Patch "$stage/psyz/CMakeLists.txt" 'if(NOT PSP)' 'if(NOT PSP AND NOT TARGET SDL3::SDL3)'
  Patch "$stage/psyz/CMakeLists.txt" '    list(APPEND PSYZ_SOURCES src/dbgserver/dbgserver.c)' '# Console build: debug HTTP server excluded.'
  # Headers referenced by GPU translation unit; rendering is forced to software below.
  New-Item -ItemType Directory -Force "$stage/external/SDL/src/video" | Out-Null
  Copy-Item -Recurse "$root/third_party/psyz/external/SDL/src/video/khronos" "$stage/external/SDL/src/video/"
 }
 # Always regenerate from the selected archive, including on resume.
 $gpuOriginal="$out/sdl3_gpu.original.c"
 $gpuText=& git -C "$root/third_party/psyz" show "${pin}:psyz/src/platform/sdl3_gpu.c"
 if($LASTEXITCODE -ne 0){throw 'Cannot read selected PsyZ GPU source'}
 [IO.File]::WriteAllText($gpuOriginal,($gpuText -join "`n")+"`n")
 & "$PSScriptRoot/patch-game.ps1" -Stage $stage -OriginalSource $gpuOriginal
 $inputFiles=@('CMakeLists.txt','cmake/ps5/game.cmake','platform/ps5/build-game.ps1','platform/ps5/patch-game.ps1','src/main/boot/main.c')
 if($StartupLog){$inputFiles+='platform/ps5/startup-log.c'}
 if($ApplicationTooling -and $StartupLog){$inputFiles+='platform/ps5/application-startup-log.c'}
 if($AllocationLog){$inputFiles+='platform/ps5/application-allocation-log.c'}
 $inputs=$inputFiles | ForEach-Object {
  $inputRoot=if($_ -like 'platform/*'){$root}else{$gameSource}
  @{File=$_; SHA256=(Get-FileHash "$inputRoot/$_").Hash}
 }
 if($candidate){
  Get-ChildItem -LiteralPath $gameSource -File -Recurse | Sort-Object FullName | ForEach-Object {
   @{File=$_.FullName.Substring($gameSource.Length+1); SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
  } | ConvertTo-Json -Depth 3 | Set-Content "$out/CANDIDATE-SOURCES.json"
  Copy-Item -LiteralPath (Join-Path (Split-Path $gameSource) 'CANDIDATE.json') -Destination "$out/CANDIDATE.json"
 }
 @{BuildConfiguration=$buildConfiguration; DebugSymbols=[bool]$DebugSymbols; PsyZRevision=$pin; StartupLog=[bool]$StartupLog; AllocationLog=[bool]$AllocationLog; ApplicationABI=[bool]$ApplicationTooling; GameRevision=(& git -C $root rev-parse HEAD); SourceRoot=$gameSource; UpstreamCandidate=$candidate; SDL3Prefix=$SDL3Prefix; SDL3Source=$SDL3Source; SDL3Build=$SDL3Build; SDL3CoreSHA256=(Get-FileHash "$SDL3Source/src/SDL.c").Hash; SDL3ConfigSHA256=(Get-FileHash "$SDL3Build/include-config-release/build_config/SDL_build_config.h").Hash; SDL3LibrarySHA256=(Get-FileHash "$SDL3Prefix/lib/libSDL3.a").Hash; Files=@($inputs)} | ConvertTo-Json -Depth 8 | Set-Content "$out/INPUTS.json"
 Run $CMake @('-S',$gameSource,'-B',"$out/b",'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_TOOLCHAIN_FILE=$Sdk/toolchain/prospero.cmake", "-DPS5_PAYLOAD_SDK=$Sdk", "-DCMAKE_AR=$LlvmBin/llvm-ar.exe", "-DCMAKE_RANLIB=$LlvmBin/llvm-ranlib.exe", "-DSDL3_DIR=$SDL3Prefix/lib/cmake/SDL3", "-DPE_PS5_SDL3_SOURCE=$SDL3Source", "-DPE_PS5_SDL3_BUILD=$SDL3Build", "-DPSYZ_DIR=$stage", "-DPE_PS5_LLVM_BIN=$LlvmBin", '-DCMAKE_VERBOSE_MAKEFILE=OFF', '-DPE_PS5_GAME=ON','-DBUILD_TESTING=OFF',"-DCMAKE_BUILD_TYPE=$buildConfiguration",'-DCMAKE_C_FLAGS=-std=gnu11')
 if($ApplicationTooling){
  $native=(Resolve-Path -LiteralPath $ApplicationTooling).Path.Replace('\','/')
  foreach($file in @('app_crt.cpp','ps5-pie.ld','app-symbols.map')){
   if(!(Test-Path "$native/$file")){throw "Missing public application input: $file"}
  }
  if($StartupLog){
   # Instrument a private copy; retain upstream licensing and original source.
   $instrumented="$out/native-debug"
   New-Item -ItemType Directory $instrumented | Out-Null
   foreach($file in @('app_crt.cpp','ps5-pie.ld','app-symbols.map')){Copy-Item "$native/$file" $instrumented}
   $native=$instrumented.Replace('\','/')
   Patch "$native/app_crt.cpp" '    void _init_env(void *process_parameters);' '    void pe_application_stage(const char *message);
    void _init_env(void *process_parameters);'
   Patch "$native/app_crt.cpp" '    const int argc = *static_cast<const int *>(process_parameters);' '    pe_application_stage("\n=== PE application: entered _start ===\n");
    const int argc = *static_cast<const int *>(process_parameters);'
   Patch "$native/app_crt.cpp" '    _init_env(process_parameters);' '    pe_application_stage("before _init_env\n");
    _init_env(process_parameters);
    pe_application_stage("after _init_env\n");'
   Patch "$native/app_crt.cpp" '    _init();' '    pe_application_stage("before constructors\n");
    _init();
    pe_application_stage("after constructors; before main\n");'
   Patch "$native/app_crt.cpp" '    catchReturnFromMain(status);' '    pe_application_stage("main returned\n");
    catchReturnFromMain(status);
    pe_application_stage("before exit\n");'
  }
  # Compile the real public application entry point; never include payload crt1.o.
  # Application mount layout supplies the executable location. Do not probe
  # /proc through the SDK's raw inline-syscall readlink adapter. Preserve the
  # public SDL source and license in a private generated copy.
  $appFilesystem="$out/SDL-application-filesystem.c"
  Copy-Item "$SDL3Source/src/filesystem/unix/SDL_sysfilesystem.c" $appFilesystem
  Patch $appFilesystem 'static char *GetExePath(void)
{' '/* Altered for PS5 application mount layout; payload builds use upstream. */
static char *GetExePath(void)
{
    return SDL_strdup("/app0/eboot.bin");'
  $crtDebugFlags=@(if($DebugSymbols){'-g'})
  Run "$LlvmBin/clang++.exe" ($crtDebugFlags + @('-target','x86_64-sie-ps5','-isysroot',$Sdk,'-isystem',"$Sdk/target/include/c++/v1",'-isystem',"$Sdk/target/include",'-fno-stack-protector','-fno-plt','-femulated-tls','-std=c++20','-O2','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections','-c',"$native/app_crt.cpp",'-o',"$out/app_crt.o"))
  $app=@"
target_sources(parasite-eve-pc PRIVATE "$($out.Replace('\','/'))/app_crt.o")
target_sources(parasite-eve-pc PRIVATE "$($PSScriptRoot.Replace('\','/'))/application-static-dlfcn.c")
# This static software-rendered application has no plugin loader. Use SDL's
# explicit unsupported backend rather than payload CRT dlsym callbacks.
target_sources(pe-ps5-sdl-core PRIVATE "$($SDL3Source.Replace('\','/'))/src/loadso/dummy/SDL_sysloadso.c")
target_compile_definitions(pe-ps5-sdl-core PRIVATE SDL_LOADSO_DUMMY=1)
# Process creation is unsupported for this application. The real SDL dummy
# backend avoids pulling raw inline vfork/setsid syscalls from SDK libc.a.
target_sources(pe-ps5-sdl-core PRIVATE "$($SDL3Source.Replace('\','/'))/src/process/dummy/SDL_dummyprocess.c" "$($appFilesystem.Replace('\','/'))")
target_compile_definitions(pe-ps5-sdl-core PRIVATE SDL_PROCESS_DUMMY=1)
# Generated filesystem source retains upstream relative include paths.
target_include_directories(pe-ps5-sdl-core PRIVATE "$($SDL3Source.Replace('\','/'))/src/filesystem/unix")
set(CMAKE_C_LINK_EXECUTABLE "${LlvmBin}/ld.lld.exe -m elf_x86_64 -pie --eh-frame-hdr -z max-page-size=0x4000 -T $native/ps5-pie.ld --version-script=$native/app-symbols.map -e _start --gc-sections -L$($Sdk.Replace('\','/'))/target/lib <OBJECTS> -o <TARGET> <LINK_LIBRARIES> -lc_stub_weak -lkernel -lSceLibcInternal -lSceNet")
"@
  # Public SDK libc supplies real POSIX adapters absent from the import stubs.
  # Audit extracted members/imports before converting or delivering this ELF.
  if($StartupLog){$app+="`ntarget_sources(parasite-eve-pc PRIVATE `"$($PSScriptRoot.Replace('\','/'))/application-startup-log.c`")"}
  $app=$app.Replace('-lc_stub_weak -lkernel','-lc -lc_stub_weak -lkernel')
  [IO.File]::WriteAllText("$out/application.cmake",$app.Replace('\','/'))
  [IO.File]::WriteAllText("$out/application-project.cmake", "cmake_language(DEFER CALL include `"$($out.Replace('\','/'))/application.cmake`")")
  Run $CMake @('-S',$gameSource,'-B',"$out/b", "-DCMAKE_PROJECT_parasite-eve-pc_INCLUDE=$out/application-project.cmake")
  @{ABI='native-application'; PublicTooling=$native; EntryPoint='_start(process_parameters, loader_teardown)'; PayloadCRT=$false; Kernel='libkernel'; DynamicPlugins='unsupported with explicit ENOSYS'; Inputs=@(@('app_crt.cpp','ps5-pie.ld','app-symbols.map') | ForEach-Object {@{File=$_; SHA256=(Get-FileHash "$native/$_").Hash}}); ConsoleTested=$false} | ConvertTo-Json -Depth 5 | Set-Content "$out/APPLICATION-ABI.json"
 }
 if($StartupLog){
  # Separate opt-in instrumentation; do not mutate game sources or baseline SDK.
  $diagSource=(Join-Path $PSScriptRoot 'startup-log.c').Replace('\','/')
  $diag=@"
add_library(pe-startup-log OBJECT "$diagSource")
target_link_libraries(pe-startup-log PRIVATE SDL3::SDL3)
target_sources(parasite-eve-pc PRIVATE `$<TARGET_OBJECTS:pe-startup-log>)
"@
  $wraps=@('main','chdir','dup2','fopen','stat','mkdir','opendir','readdir','SDL_CreateDirectory','SDL_GetPathInfo','SDL_EnumerateDirectory','SDL_SaveFile','SDL_Init','SDL_InitSubSystem','SDL_CreateWindow','SDL_GetWindowSurface','SDL_UpdateWindowSurface','SDL_OpenAudioDevice','SDL_OpenAudioDeviceStream','SDL_ResumeAudioStreamDevice','SDL_OpenGamepad','Boot_InitSystem','Boot_InitGameState','Boot_InitSubsystems','Asset_OpenPeImage','Scene_LoadSceneData')
  if($ApplicationTooling){
   $diag+="`ntarget_compile_definitions(pe-startup-log PRIVATE PE_APPLICATION_SERVICE_DIAGNOSTICS=1)"
   $wraps+=@('sceUserServiceInitialize','scePadInit','sceSystemServiceHideSplashScreen','sceVideoOutOpen')
  }
  if($AllocationLog){
   $allocationSource=(Join-Path $PSScriptRoot 'application-allocation-log.c').Replace('\','/')
   $diag+="`ntarget_sources(pe-startup-log PRIVATE `"$allocationSource`")"
   $wraps+=@('malloc','calloc','realloc')
  }
  foreach($symbol in $wraps){$diag+="`ntarget_link_libraries(parasite-eve-pc PRIVATE -Wl,--wrap=$symbol)"}
  # Windows direct ld invocation does not accept the compiler-driver -Wl prefix.
  $diag=$diag.Replace('-Wl,--wrap=','--wrap=')
  if($ApplicationTooling){$diag="include(`"$($out.Replace('\','/'))/application.cmake`")`n"+$diag}
  [IO.File]::WriteAllText("$out/startup-log.cmake",$diag)
  # Deferred include runs after the game target has been declared.
  [IO.File]::WriteAllText("$out/startup-project.cmake", "cmake_language(DEFER CALL include `"$($out.Replace('\','/'))/startup-log.cmake`")")
  Run $CMake @('-S',$gameSource,'-B',"$out/b", "-DCMAKE_PROJECT_parasite-eve-pc_INCLUDE=$out/startup-project.cmake")
 }
 Run $CMake @('--build',"$out/b",'--target','parasite-eve-pc','--parallel','4')
 $elf="$out/b/parasite-eve-ps5.elf"
 if(!(Test-Path $elf)){throw 'Game target did not produce its ELF'}
 $symbolElf=$null
 if($DebugSymbols){
  # Retain the linked ELF before any external conversion/packaging. Fail closed
  # if a toolchain/linker update silently removes the requested debug info.
  $sections=& "$LlvmBin/llvm-readelf.exe" --sections --wide $elf
  if($LASTEXITCODE -ne 0){throw 'Cannot inspect debug sections'}
  $sections | Set-Content "$out/DWARF-SECTIONS.txt"
  foreach($section in @('.debug_info','.debug_abbrev','.debug_line')){
   if(($sections -join "`n") -notmatch ([regex]::Escape($section)+'\s')){throw "Missing DWARF section: $section"}
  }
  $symbolElf="$out/parasite-eve-ps5-unstripped.elf"
  Copy-Item -LiteralPath $elf -Destination $symbolElf
  @{BuildConfiguration=$buildConfiguration; ELF=$symbolElf; SHA256=(Get-FileHash $symbolElf).Hash; SourceRoot=$gameSource; PsyZSource=$stage; ApplicationSource=$(if($ApplicationTooling){"$native/app_crt.cpp"}else{$null}); Sections="$out/DWARF-SECTIONS.txt"; ConsoleTested=$false} | ConvertTo-Json | Set-Content "$out/DEBUG-SYMBOLS.json"
 }
 if($StartupLog){
  Copy-Item -LiteralPath $elf -Destination "$out/b/parasite-eve-ps5-startup-debug.elf"
  Copy-Item -LiteralPath "$PSScriptRoot/startup-log.c" -Destination "$out/startup-log.c"
  if($ApplicationTooling){Copy-Item "$PSScriptRoot/application-startup-log.c" "$out/application-startup-log.c"}
  @{Enabled=$true; AllocationLog=[bool]$AllocationLog; ApplicationABI=[bool]$ApplicationTooling; SourceSHA256=(Get-FileHash "$out/startup-log.c").Hash; EarlyApplicationSourceSHA256=$(if($ApplicationTooling){(Get-FileHash "$out/application-startup-log.c").Hash}else{$null}); DebugELF="$out/b/parasite-eve-ps5-startup-debug.elf"; SHA256=(Get-FileHash $elf).Hash; PrimaryLog='/data/parasite-eve/startup.log'; FallbackLog='/data/parasite-eve-startup.log'; ConsoleTested=$false} | ConvertTo-Json | Set-Content "$out/STARTUP-LOG.json"
 }
 Run "$LlvmBin/llvm-readelf.exe" @('-h','-d',$elf)
 Get-FileHash $elf | Format-List | Out-File "$out/ELF-SHA256.txt"
 @{Schema=2; State='succeeded'; BuildConfiguration=$buildConfiguration; DebugSymbols=[bool]$DebugSymbols; UnstrippedELF=$symbolElf; Attempt=$attempt; InputsSHA256=(Get-FileHash "$out/INPUTS.json").Hash; ELF=$elf; SHA256=(Get-FileHash $elf).Hash; Bytes=(Get-Item $elf).Length; ConsoleTested=$false} | ConvertTo-Json | Set-Content "$out/RESULT.json"
 Write-Output "Linked native game ELF (UNTESTED ON CONSOLE): $elf"
}finally{$env:PATH=$oldPath; $env:PS5_PAYLOAD_SDK=$oldSdk}


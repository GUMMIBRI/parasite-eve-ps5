# Build the real game with an installed public SDL3 package, preserving submodules.
[CmdletBinding()]
param(
 [string]$Sdk='',
 [string]$LlvmBin='',
 [string]$SDL3Prefix='C:/.pe/tools/pe-ps5-sdl3/20261029T010754Z-68fad8/install',
 [string]$SDL3Source,
 [string]$SDL3Build,
 [string]$CMake='C:/.pe/tools/cmake/cmake-4.4.4-windows-x86_64/bin/cmake.exe',
 [string]$Ninja='C:/.pe/tools/ninja/ninja.exe',
 [string]$Resume,
 [string]$SourceRoot
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path "$PSScriptRoot/../..").Path
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
$SDL3Prefix=(Resolve-Path $SDL3Prefix).Path
if(!$SDL3Source){$SDL3Source=Join-Path (Split-Path $SDL3Prefix) 'src'}
if(!$SDL3Build){$SDL3Build=Join-Path (Split-Path $SDL3Prefix) 'b'}
$SDL3Source=(Resolve-Path $SDL3Source).Path
$SDL3Build=(Resolve-Path $SDL3Build).Path
$CMake=(Get-Command $CMake -ErrorAction Stop).Source
$Ninja=(Get-Command $Ninja -ErrorAction Stop).Source
$pin=& git -C "$root/third_party/psyz" rev-parse HEAD
if($LASTEXITCODE -ne 0){throw 'Cannot identify PsyZ revision'}
$dirty=& git -C "$root/third_party/psyz" status --porcelain --untracked-files=no
if($LASTEXITCODE -ne 0 -or $dirty){throw 'PsyZ tracked sources must be clean for generated patches'}
$out=if($Resume){(Resolve-Path $Resume).Path}else{Join-Path $root ('outputs/ps5/game-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))}
New-Item -ItemType Directory -Force $out | Out-Null
if($Resume -and (Test-Path "$out/INPUTS.json")){
 $previous=Get-Content "$out/INPUTS.json" -Raw | ConvertFrom-Json
 if($previous.PsyZRevision -ne $pin){throw 'Resume PsyZ revision changed; use a new isolated build'}
 if($previous.PSObject.Properties.Name -contains 'SourceRoot'){
  if($previous.SourceRoot -ne $gameSource){throw 'Resume SourceRoot changed; use a new isolated build'}
 }elseif($SourceRoot){throw 'Cannot resume a baseline build with isolated sources'}
}
if($candidate -and $candidate.PsyZRevision -ne $pin){throw 'Candidate PsyZ revision differs from preserved checkout'}
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
  Run git @('-C',"$root/third_party/psyz",'archive','--format=zip',"--output=$out/psyz.zip",'HEAD')
  Expand-Archive "$out/psyz.zip" $stage
  Patch "$stage/psyz/CMakeLists.txt" 'if(NOT PSP)' 'if(NOT PSP AND NOT TARGET SDL3::SDL3)'
  Patch "$stage/psyz/CMakeLists.txt" '    list(APPEND PSYZ_SOURCES src/dbgserver/dbgserver.c)' '# Console build: debug HTTP server excluded.'
  # Headers referenced by GPU translation unit; rendering is forced to software below.
  New-Item -ItemType Directory -Force "$stage/external/SDL/src/video" | Out-Null
  Copy-Item -Recurse "$root/third_party/psyz/external/SDL/src/video/khronos" "$stage/external/SDL/src/video/"
 }
 & "$PSScriptRoot/patch-game.ps1" -Stage $stage
 $inputs=@('CMakeLists.txt','cmake/ps5/game.cmake','platform/ps5/build-game.ps1','platform/ps5/patch-game.ps1','src/main/boot/main.c') | ForEach-Object {
  $inputRoot=if($_ -like 'platform/*'){$root}else{$gameSource}
  @{File=$_; SHA256=(Get-FileHash "$inputRoot/$_").Hash}
 }
 if($candidate){
  Get-ChildItem -LiteralPath $gameSource -File -Recurse | Sort-Object FullName | ForEach-Object {
   @{File=$_.FullName.Substring($gameSource.Length+1); SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
  } | ConvertTo-Json -Depth 3 | Set-Content "$out/CANDIDATE-SOURCES.json"
  Copy-Item -LiteralPath (Join-Path (Split-Path $gameSource) 'CANDIDATE.json') -Destination "$out/CANDIDATE.json"
 }
 @{PsyZRevision=$pin; GameRevision=(& git -C $root rev-parse HEAD); SourceRoot=$gameSource; UpstreamCandidate=$candidate; SDL3Prefix=$SDL3Prefix; SDL3Source=$SDL3Source; SDL3Build=$SDL3Build; SDL3CoreSHA256=(Get-FileHash "$SDL3Source/src/SDL.c").Hash; SDL3ConfigSHA256=(Get-FileHash "$SDL3Build/include-config-release/build_config/SDL_build_config.h").Hash; SDL3LibrarySHA256=(Get-FileHash "$SDL3Prefix/lib/libSDL3.a").Hash; Files=@($inputs)} | ConvertTo-Json -Depth 8 | Set-Content "$out/INPUTS.json"
 Run $CMake @('-S',$gameSource,'-B',"$out/b",'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_TOOLCHAIN_FILE=$Sdk/toolchain/prospero.cmake", "-DPS5_PAYLOAD_SDK=$Sdk", "-DCMAKE_AR=$LlvmBin/llvm-ar.exe", "-DCMAKE_RANLIB=$LlvmBin/llvm-ranlib.exe", "-DSDL3_DIR=$SDL3Prefix/lib/cmake/SDL3", "-DPE_PS5_SDL3_SOURCE=$SDL3Source", "-DPE_PS5_SDL3_BUILD=$SDL3Build", "-DPSYZ_DIR=$stage", "-DPE_PS5_LLVM_BIN=$LlvmBin", '-DCMAKE_VERBOSE_MAKEFILE=OFF', '-DPE_PS5_GAME=ON','-DBUILD_TESTING=OFF','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_FLAGS=-std=gnu11')
 Run $CMake @('--build',"$out/b",'--target','parasite-eve-pc','--parallel','4')
 $elf="$out/b/parasite-eve-ps5.elf"
 if(!(Test-Path $elf)){throw 'Game target did not produce its ELF'}
 Run "$LlvmBin/llvm-readelf.exe" @('-h','-d',$elf)
 Get-FileHash $elf | Format-List | Out-File "$out/ELF-SHA256.txt"
 @{Schema=2; State='succeeded'; Attempt=$attempt; InputsSHA256=(Get-FileHash "$out/INPUTS.json").Hash; ELF=$elf; SHA256=(Get-FileHash $elf).Hash; Bytes=(Get-Item $elf).Length; ConsoleTested=$false} | ConvertTo-Json | Set-Content "$out/RESULT.json"
 Write-Output "Linked native game ELF (UNTESTED ON CONSOLE): $elf"
}finally{$env:PATH=$oldPath; $env:PS5_PAYLOAD_SDK=$oldSdk}

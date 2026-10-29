[CmdletBinding()]
param(
 [Parameter(Mandatory)][ValidateSet('Host','SDK','Off')][string]$Mode,
 [Parameter(Mandatory)][string]$CMake,
 [Parameter(Mandatory)][string]$Ninja,
 [string]$CTest,
 [string]$SDL3Prefix,
 [string]$Sdk,
 [string]$LlvmBin,
 [string]$HostCompiler,
 [string]$HostCompilerArg1,
 [string[]]$ExtraCMakeArgs=@(),
 [string]$BuildDirectory
)
$ErrorActionPreference='Stop'
function Existing([string]$p,[string]$description) {
 if(!$p -or !(Test-Path -LiteralPath $p)){throw "Missing $description : $p"}
 return (Resolve-Path -LiteralPath $p).Path.Replace('\','/')
}
$CMake=Existing $CMake 'CMake executable'
$Ninja=Existing $Ninja 'Ninja executable'
$source=Existing "$PSScriptRoot/tests" 'synthetic checks source'
$root=(Resolve-Path "$PSScriptRoot/../../..").Path
if(!$BuildDirectory){$BuildDirectory=Join-Path $root ('outputs/ps5/hd-staging-'+$Mode.ToLower()+'-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))}
# Resolve prerequisites before creating/configuring a build tree. Never download.
$argsList=@('-S',$source,'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$Ninja",'-DCMAKE_BUILD_TYPE=Release')
if($Mode -eq 'Off'){
 $argsList+=@('-DPE_PS5_HD_STAGING=OFF','-DPE_HD_CHECKS=OFF')
 if($HostCompiler){$HostCompiler=Existing $HostCompiler 'host compiler'; $argsList+="-DCMAKE_C_COMPILER=$HostCompiler"}
}else{
 $SDL3Prefix=Existing $SDL3Prefix 'SDL3 installation prefix'
 $null=Existing "$SDL3Prefix/lib/cmake/SDL3/SDL3Config.cmake" 'SDL3 package config'
 $argsList+=@('-DPE_PS5_HD_STAGING=ON','-DPE_HD_CHECKS=ON',"-DSDL3_DIR=$SDL3Prefix/lib/cmake/SDL3")
 if($Mode -eq 'SDK'){
  $Sdk=Existing $Sdk 'public SDK root'
  $null=Existing "$Sdk/toolchain/prospero.cmake" 'public SDK toolchain'
  $LlvmBin=Existing $LlvmBin 'LLVM tools directory'
  foreach($tool in @('clang.exe','llvm-ar.exe','llvm-ranlib.exe')){$null=Existing "$LlvmBin/$tool" "LLVM $tool"}
  $argsList+=@("-DCMAKE_TOOLCHAIN_FILE=$Sdk/toolchain/prospero.cmake","-DCMAKE_AR=$LlvmBin/llvm-ar.exe","-DCMAKE_RANLIB=$LlvmBin/llvm-ranlib.exe")
 }else{
  $HostCompiler=Existing $HostCompiler 'host compiler'
  $CTest=Existing $CTest 'CTest executable'
  $argsList+="-DCMAKE_C_COMPILER=$HostCompiler"
 }
}
if($HostCompilerArg1){$argsList+="-DCMAKE_C_COMPILER_ARG1=$HostCompilerArg1"}
New-Item -ItemType Directory -Force $BuildDirectory | Out-Null
$out=(Resolve-Path -LiteralPath $BuildDirectory).Path.Replace('\','/')
$argsList+=@('-B',$out)
$argsList+=$ExtraCMakeArgs
function Run([string]$exe,[string[]]$arguments){
 & $exe @arguments 2>&1 | Tee-Object "$out/validation.log" -Append
 if($LASTEXITCODE -ne 0){throw "Command failed ($LASTEXITCODE): $exe; see $out/validation.log"}
}
$oldPath=$env:PATH; $oldGlobal=$env:ZIG_GLOBAL_CACHE_DIR; $oldLocal=$env:ZIG_LOCAL_CACHE_DIR
try {
 if($LlvmBin){$env:PATH="$LlvmBin;$oldPath"}
 $env:ZIG_GLOBAL_CACHE_DIR="$out/zig-global"; $env:ZIG_LOCAL_CACHE_DIR="$out/zig-local"
 Run $CMake $argsList
 Run $CMake @('--build',$out,'--parallel','4')
 if($Mode -eq 'Host'){Run $CTest @('--test-dir',$out,'--output-on-failure','--no-tests=error')}
 $hashes=Get-ChildItem $PSScriptRoot -File -Recurse | Where-Object {$_.Extension -in '.c','.h','.cmake','.ps1','.txt'} | ForEach-Object {
  @{Path=$_.FullName.Substring($root.Length+1).Replace('\','/'); SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
 }
 @{UTC=[DateTime]::UtcNow.ToString('o');Mode=$Mode;ConsoleTested=$false;HostChecksPassed=($Mode -eq 'Host');ConfigureArguments=$argsList;Inputs=$hashes} | ConvertTo-Json -Depth 5 | Set-Content "$out/RESULT.json"
 Write-Output "HD staging evidence: $out"
} finally {$env:PATH=$oldPath; $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal; $env:ZIG_LOCAL_CACHE_DIR=$oldLocal}


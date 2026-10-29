# Build the public application converter with an existing host zlib DLL.
# Does not package, deploy, download, or modify the reference/tool caches.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$Reference,
 [Parameter(Mandatory)][string]$ZlibHeaders,
 [Parameter(Mandatory)][string]$ZlibDll,
 [string]$Zig='C:/.pe/tools/zig/zig-x86_64-windows-0.15.2/zig.exe'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$ref=(Resolve-Path -LiteralPath $Reference).Path
$validated=& "$PSScriptRoot/validate-application-inputs.ps1" -NativeSource "$ref/tooling/native" -FullTooling
$headers=(Resolve-Path -LiteralPath $ZlibHeaders).Path
$dll=(Resolve-Path -LiteralPath $ZlibDll).Path
foreach($file in @('zlib.h','zconf.h')){
 if(!(Test-Path "$headers/$file")){throw "Missing host zlib header: $file"}
}
$out=Join-Path $root ('outputs/ps5/native-tool-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))
$null=New-Item -ItemType Directory "$out/source","$out/host","$out/zlib-headers"
Copy-Item "$ref/tooling/native/*" "$out/source" -Recurse
Copy-Item "$ref/LICENSE" "$out/source/LICENSE"
Copy-Item "$headers/zlib.h","$headers/zconf.h" "$out/zlib-headers"
Copy-Item -LiteralPath $dll -Destination "$out/host"
$names=@('native_app_builder','self_container','elf_object','sce_module_writer')
$sources=@($names | ForEach-Object {"$out/source/$_.cpp"})
$oldGlobal=$env:ZIG_GLOBAL_CACHE_DIR
$oldLocal=$env:ZIG_LOCAL_CACHE_DIR
try {
 $env:ZIG_GLOBAL_CACHE_DIR="$out/zig-global"
 $env:ZIG_LOCAL_CACHE_DIR="$out/zig-local"
 & $Zig c++ -std=c++20 -O2 -I "$out/zlib-headers" @sources $dll -o "$out/host/ps5-native-tool.exe" *> "$out/build.log"
 if($LASTEXITCODE -ne 0){throw "Host converter build failed: $out/build.log"}
 # Usage exits 2; this also checks the DLL can actually be loaded.
 & "$out/host/ps5-native-tool.exe" *> "$out/usage.log"
 if($LASTEXITCODE -ne 2){throw 'Host converter failed its usage check'}
 [ordered]@{
  Tool="$out/host/ps5-native-tool.exe"
  ToolSHA256=(Get-FileHash "$out/host/ps5-native-tool.exe").Hash
  Reference=$ref
  PublicSource='https://github.com/SynoPiia/PSXS5/tree/5c17ea31aec5791c12451e915386fa98d670adb4/tooling/native'
  ToolLicense='GPL-3.0-or-later; notices and LICENSE retained'
  ZlibDLL=$dll
  ZlibSHA256=(Get-FileHash $dll).Hash
  ZlibHeaderSHA256=(Get-FileHash "$out/zlib-headers/zlib.h").Hash
  HostDependencyOnly=$true
  ConsoleTested=$false
 } | ConvertTo-Json | Set-Content "$out/RESULT.json"
 Write-Output $out
} finally {
 $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal
 $env:ZIG_LOCAL_CACHE_DIR=$oldLocal
}

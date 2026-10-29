# Reproduce only the public raw runtime; never edits the reference or packages a title.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$Reference,
 [string]$Zig='C:/.pe/tools/zig/zig-x86_64-windows-0.15.2/zig.exe'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$ref=(Resolve-Path -LiteralPath $Reference).Path
$validated=& "$PSScriptRoot/validate-application-inputs.ps1" -NativeSource "$ref/tooling/native" -FullTooling
$expected='8ee6e124993e1af26420cb455890fd002f5d6c7e78883c860ce45734e7d002bb'
$out=Join-Path $root ('outputs/ps5/runtime-reproduction-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))
$null=New-Item -ItemType Directory -Path "$out/source/runtime","$out/zig-global","$out/zig-local"
Copy-Item -LiteralPath "$ref/tooling/native/libc_builder.cpp","$ref/tooling/native/hash.hpp","$ref/LICENSE","$ref/runtime/README.md","$ref/tools/rebuild-libc.sh" -Destination "$out/source"
Copy-Item -LiteralPath "$ref/tooling/native/runtime/api-surface.txt","$ref/tooling/native/runtime/imports.txt" -Destination "$out/source/runtime"
$oldGlobal=$env:ZIG_GLOBAL_CACHE_DIR
$oldLocal=$env:ZIG_LOCAL_CACHE_DIR
try {
 $env:ZIG_GLOBAL_CACHE_DIR="$out/zig-global"
 $env:ZIG_LOCAL_CACHE_DIR="$out/zig-local"
 & $Zig c++ -std=c++20 -O2 "$out/source/libc_builder.cpp" -o "$out/libc-builder.exe" *> "$out/build-runtime.log"
 if($LASTEXITCODE -ne 0){throw "Host runtime emitter build failed; see $out/build-runtime.log"}
 foreach($copy in @('a','b')){
  & "$out/libc-builder.exe" "$out/source/runtime/api-surface.txt" "$out/source/runtime/imports.txt" "$out/libc-$copy.raw.elf" *> "$out/emit-$copy.log"
  if($LASTEXITCODE -ne 0){throw "Runtime generation failed: $copy"}
  if((Get-FileHash "$out/libc-$copy.raw.elf").Hash.ToLowerInvariant() -ne $expected){throw 'Raw runtime differs from recorded public source digest'}
 }
 [ordered]@{
  Reference=$ref
  Source='https://github.com/SynoPiia/PSXS5/tree/5c17ea31aec5791c12451e915386fa98d670adb4/tooling/native'
  License='GPL-3.0-or-later; copied source notices and LICENSE retained'
  RawSHA256=$expected
  RawBytes=(Get-Item "$out/libc-a.raw.elf").Length
  Stage='raw-runtime-only'
  ConsoleValidated=$false
  Remaining='Build the unmodified public ps5-native-tool with host zlib; normalize and FSELF-wrap both raw modules; verify signed digest e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036. Raw runtime is not a deployable libc.prx.'
 } | ConvertTo-Json | Set-Content "$out/RESULT.json"
 Write-Output $out
} finally {
 $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal
 $env:ZIG_LOCAL_CACHE_DIR=$oldLocal
}

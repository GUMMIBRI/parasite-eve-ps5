# Read-only local asset check. Compile the unchanged loader portion of its game
# translation unit; no unrelated game callbacks or replacement implementations.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$Disc1,
 [Parameter(Mandatory=$true)][string]$Disc2,
 [Parameter(Mandatory=$true)][string]$Archive1,
 [Parameter(Mandatory=$true)][string]$Archive2,
 [string]$Zig='C:/.pe/tools/zig/zig-x86_64-windows-0.15.2/zig.exe'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$out=Join-Path $root ('outputs/ps5/assets-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'))
New-Item -ItemType Directory $out | Out-Null
Set-Content "$out/.gitignore" '*'
$source=[IO.File]::ReadAllText("$root/src/main/cdrom/scene_assets.c")
$start=$source.IndexOf('/* Image access belongs to the original archive loader, shared by native callers. */')
$end=$source.IndexOf('PeDisc *g_CdImage;', $start)
if($start -lt 0 -or $end -le $start){throw 'Original disc loader boundaries changed'}
[IO.File]::WriteAllText("$out/disc-loader.c", '#include "pe1/cdrom.h"'+"`n"+$source.Substring($start,$end-$start))
$oldGlobal=$env:ZIG_GLOBAL_CACHE_DIR
$oldLocal=$env:ZIG_LOCAL_CACHE_DIR
try{
 $env:ZIG_GLOBAL_CACHE_DIR="$root/outputs/zig-asset-cache"
 $env:ZIG_LOCAL_CACHE_DIR="$root/outputs/zig-asset-local"
 & $Zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror -I "$root/include" "$PSScriptRoot/check-disc-assets.c" "$out/disc-loader.c" -o "$out/check-disc-assets.exe" 2>&1 | Tee-Object "$out/build.log"
 if($LASTEXITCODE -ne 0){throw 'Native disc-check compilation failed'}
 $paths=@(@($Disc1,$Archive1),@($Disc2,$Archive2))
 $matches=@()
 for($i=0;$i -lt 2;$i++){
  & "$out/check-disc-assets.exe" $paths[$i][0] ($i+1) $paths[$i][1] 2>&1 | Tee-Object "$out/disc-$($i+1).log"
  if($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 3){throw "Disc $($i+1) failed read-only asset validation"}
  $matches+=($LASTEXITCODE -eq 0)
 }
 $files=@($Disc1,$Disc2,$Archive1,$Archive2,"$root/src/main/cdrom/scene_assets.c","$PSScriptRoot/check-disc-assets.c") | ForEach-Object {Get-FileHash -LiteralPath $_}
 @{Files=@($files | Select-Object Path,Hash); DiscArchivesReadable=$true; ExactArchiveMatches=$matches; ConsoleTested=$false} | ConvertTo-Json -Depth 5 | Set-Content "$out/RESULT.json"
 Write-Output "Read-only disc scan complete; inspect separate reference-match results: $out"
}finally{
 $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal
 $env:ZIG_LOCAL_CACHE_DIR=$oldLocal
}

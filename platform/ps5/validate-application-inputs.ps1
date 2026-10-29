# Validate recorded public inputs before generating or compiling application code.
[CmdletBinding()]
param(
 [string]$NativeSource,
 [string]$Sdk,
 [switch]$FullTooling
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if(!$NativeSource -and !$Sdk){throw 'Supply NativeSource and/or Sdk'}
if($FullTooling -and !$NativeSource){throw 'FullTooling requires NativeSource'}
$manifest=Get-Content -LiteralPath "$PSScriptRoot/application-inputs.json" -Raw | ConvertFrom-Json
$checked=0
foreach($group in @('Native','Sdk')){
 $base=if($group -eq 'Native'){$NativeSource}else{$Sdk}
 if(!$base){continue}
 $base=(Resolve-Path -LiteralPath $base).Path
 foreach($entry in $manifest.$group){
  if($group -eq 'Native' -and !$FullTooling -and !$entry.Application){continue}
  $path=Join-Path $base $entry.Path
  # Upstream keeps LICENSE at repository root; isolated copies retain it locally.
  if($group -eq 'Native' -and $entry.Path -eq 'LICENSE' -and !(Test-Path -LiteralPath $path)){
   $path=Join-Path $base '../../LICENSE'
  }
  if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing pinned $group input: $($entry.Path)"}
  if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.SHA256){
   throw "Pinned $group input differs: $($entry.Path). Review the public source/version; do not silently regenerate the manifest."
  }
  $checked++
 }
}
[pscustomobject]@{Validated=$true; CheckedFiles=$checked; SourcePin=$manifest.SourcePin; ConsoleTested=$false}

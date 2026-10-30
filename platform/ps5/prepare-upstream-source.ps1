# Export and three-way reconcile a candidate without changing the checkout/index.
[CmdletBinding()]
param([string]$Revision='da1f775e686ebe47fa5848c723f7efba73d59211', [switch]$AcceptVersionResolution, [string]$PsyZRevision, [string]$OutputRoot, [switch]$IncludePS5Plugins)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path "$PSScriptRoot/../..").Path
function GitText([string[]]$Arguments) {
 $result=& git -C $root @Arguments
 if($LASTEXITCODE -ne 0){throw "git failed: $Arguments"}
 return ($result -join "`n")+"`n"
}
$theirs=(GitText @('rev-parse',"$Revision^{commit}")).Trim()
$original08=$theirs -eq '8c52cdde5d60ee52f00913517ffb593e9521bf2d'
$ours=(GitText @('rev-parse','HEAD')).Trim()
$base=(GitText @('merge-base',$ours,$theirs)).Trim()
if(!$OutputRoot){$OutputRoot=$env:PE_PS5_OUTPUT_ROOT}
if(!$OutputRoot){$OutputRoot=Join-Path $root 'outputs/ps5'}
$OutputRoot=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
if(Test-Path -LiteralPath $OutputRoot -PathType Leaf){throw 'OutputRoot must be a directory, not a file'}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot=(Resolve-Path -LiteralPath $OutputRoot).Path
$out=Join-Path $OutputRoot ('upstream-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N'))
$source=Join-Path $out 'src'
New-Item -ItemType Directory $source | Out-Null
[IO.File]::WriteAllText("$out/.gitignore","*`n")
& git -C $root archive --format=zip "--output=$out/upstream.zip" $theirs
if($LASTEXITCODE -ne 0){throw 'Upstream archive failed'}
Expand-Archive "$out/upstream.zip" $source
& git -C $root diff --binary "--output=$out/LOCAL-TRACKED.patch" $base --
if($LASTEXITCODE -ne 0){throw 'Cannot save tracked local patch'}
& git -C $root diff --binary "--output=$out/UPSTREAM.patch" $base $theirs --
if($LASTEXITCODE -ne 0){throw 'Cannot save upstream patch'}
$changes=@(); $conflicts=@()
$paths=@(& git -C $root diff --name-only $base --)
if($LASTEXITCODE -ne 0){throw 'Cannot enumerate local tracked differences'}
foreach($path in $paths) {
 $dest=Join-Path $source $path
 $local=Join-Path $root $path
 $entry=& git -C $root ls-tree $base -- $path
 if($entry -match '^160000'){throw "Submodule changes require deliberate validation: $path"}
 if(!(Test-Path -LiteralPath $local -PathType Leaf)){throw "Local deletion needs explicit resolution: $path"}
 $safe=$path.Replace('/','_')
 $mergeDir="$out/merge/$safe"
 New-Item -ItemType Directory -Force $mergeDir | Out-Null
 $localHash=(Get-FileHash -LiteralPath $local).Hash
 $localText=[IO.File]::ReadAllText($local).Replace("`r`n","`n")
 if((Get-FileHash -LiteralPath $local).Hash -ne $localHash){throw "Local input changed while reading: $path"}
 [IO.File]::WriteAllText("$mergeDir/ours",$localText)
 if($entry) {
  [IO.File]::WriteAllText("$mergeDir/base",(GitText @('show',"${base}:$path")))
  if(!(Test-Path -LiteralPath $dest)){throw "Upstream deletion needs explicit resolution: $path"}
  [IO.File]::WriteAllText("$mergeDir/theirs",[IO.File]::ReadAllText($dest).Replace("`r`n","`n"))
  & git merge-file -L local-ps5 -L common-base -L upstream "$mergeDir/ours" "$mergeDir/base" "$mergeDir/theirs"
  $mergeCode=$LASTEXITCODE
  if($mergeCode -lt 0 -or $mergeCode -gt 127){throw "Merge failed: $path ($mergeCode)"}
  $resolution=if($mergeCode){'unresolved'}else{'three-way clean'}
  if($mergeCode -and $AcceptVersionResolution -and $path -eq 'CMakeLists.txt'){
   $merged=[IO.File]::ReadAllText("$mergeDir/ours")
   $pattern='<<<<<<< local-ps5\noption\(PE_PS5_GAME "Build the native game with the public PS5 SDK" OFF\)\nproject\(parasite-eve-pc VERSION [0-9.]+ LANGUAGES C\)\nif\(PE_PS5_GAME\)\n    include\(cmake/ps5/game.cmake\)\nendif\(\)\n=======\n(project\(parasite-eve-pc VERSION [0-9.]+ LANGUAGES C\))\n>>>>>>> upstream'
   $match=[regex]::Match($merged,$pattern)
   if($match.Success){
    $accepted="option(PE_PS5_GAME `"Build the native game with the public PS5 SDK`" OFF)`n"+$match.Groups[1].Value+"`nif(PE_PS5_GAME)`n    include(cmake/ps5/game.cmake)`nendif()"
    $merged=$merged.Replace($match.Value,$accepted)
    [IO.File]::WriteAllText("$mergeDir/ours",$merged)
    if($merged -notmatch '(?m)^<<<<<<< '){$mergeCode=0; $resolution='Accepted upstream project version plus local PS5 opt-in/include (-AcceptVersionResolution)'}
   }
  }
  if($mergeCode -and $original08 -and $AcceptVersionResolution -and $path -eq 'src/main/boot/main.c'){
   & "$PSScriptRoot/integrate-upstream08.ps1" -Root "$mergeDir/ours" -Revision $theirs -Mode MainConflict
   $mergeCode=0; $resolution='Checked original 0.8 main conflict recipe: preserve PS5 guards and upstream Unicode/HD dispatch'
  }
  if($mergeCode -and ($IncludePS5Plugins -or ($original08 -and $AcceptVersionResolution)) -and $path -eq 'mods/CMakeLists.txt'){
   # Preserve the selected upstream host/list. The checked plugin recipe below
   # adds the CPU adapter and the opt-in CMake forces only pe_assists.
   Copy-Item -LiteralPath "$mergeDir/theirs" -Destination "$mergeDir/ours"
   $mergeCode=0; $resolution='Selected upstream mods build glue plus checked PS5 plugin integration'
  }
  if($mergeCode -ne 0){$conflicts+=$path}
 } else {
  if(Test-Path -LiteralPath $dest){throw "Add/add needs explicit resolution: $path"}
  $resolution='local addition'
 }
 New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
 Copy-Item -LiteralPath "$mergeDir/ours" -Destination $dest
 $changes+=@{Path=$path; Resolution=$resolution; LocalSHA256=$localHash}
}
# Only source build glue; never copy tools, outputs, assets or the submodules.
foreach($relative in @('cmake/ps5/game.cmake','cmake/ps5/payload.cmake')) {
 $dest=Join-Path $source $relative
 New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
 Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $dest
}
# settings_overlay.h has source-relative shader includes even for software mode.
$shaderRelative='third_party/psyz/external/SDL/src/render/gpu/shaders'
New-Item -ItemType Directory -Force "$source/$shaderRelative" | Out-Null
Copy-Item "$root/$shaderRelative/*.h" "$source/$shaderRelative/"
Copy-Item "$root/third_party/psyz/external/SDL/LICENSE.txt" "$source/third_party/psyz/external/SDL/LICENSE.txt"
$sdlPin=(GitText @('-C',"$root/third_party/psyz/external/SDL",'rev-parse','HEAD')).Trim()
# Native software presentation has no widescreen extended drawing surface.
# Retain upstream desktop logic; normalize only the existing PE_PS5 branches.
$boot="$source/src/main/boot/main.c"
$bootText=[IO.File]::ReadAllText($boot)
if($bootText.Contains('    int widescreen;')){
 $bootText=$bootText.Replace("    host_options.model_filtering = 0;`n    host_options.fullscreen = 0;", "    host_options.model_filtering = 0;`n    host_options.widescreen = 0;`n    host_options.fullscreen = 0;")
 $bootText=$bootText.Replace("    host_options.geometry_correction = host_options.antialiasing = host_options.model_filtering = 0;`n#endif", "    host_options.geometry_correction = host_options.antialiasing = host_options.model_filtering = 0;`n    host_options.widescreen = 0;`n#endif")
 [IO.File]::WriteAllText($boot,$bootText)
 $changes+=@{Path='src/main/boot/main.c'; Resolution='PE_PS5-only widescreen option normalization; CPU renderer has no extended widescreen surface'}
}
if($IncludePS5Plugins){
 & "$PSScriptRoot/integrate-plugins.ps1" -Root $source -Tree Game -Revision $theirs | Out-Null
 $changes+=@{Path='platform/ps5/plugin-source.json'; Resolution='Checked built-in source integration; PE_PS5_PLUGINS defaults OFF'; SHA256=(Get-FileHash "$PSScriptRoot/plugin-source.json").Hash}
}
if($original08 -and $AcceptVersionResolution -and !$conflicts.Count){
 & "$PSScriptRoot/integrate-upstream08.ps1" -Root $source -Revision $theirs
 $changes+=@{Path='platform/ps5/upstream08-recipe.json'; Resolution='Checked original 0.8 adaptation; desktop downloads default ON, native OFF'; SHA256=(Get-FileHash "$PSScriptRoot/upstream08-recipe.json").Hash; ScriptSHA256=(Get-FileHash "$PSScriptRoot/integrate-upstream08.ps1").Hash}
}
$pin=(GitText @('-C',"$root/third_party/psyz",'rev-parse','HEAD')).Trim()
if($PsyZRevision){$pin=(GitText @('-C',"$root/third_party/psyz",'rev-parse',"$PsyZRevision^{commit}")).Trim()}
$upstreamPin=(GitText @('ls-tree',$theirs,'third_party/psyz')).Trim()
if((GitText @('rev-parse','HEAD')).Trim() -ne $ours){throw 'Checkout HEAD changed during preparation; candidate not marked ready'}
foreach($change in $changes){
 if($change.ContainsKey('LocalSHA256') -and (Get-FileHash -LiteralPath (Join-Path $root $change.Path)).Hash -ne $change.LocalSHA256){
  throw "Local input changed during preparation: $($change.Path); candidate not marked ready"
 }
}
@{Revision=$theirs; Base=$base; Ours=$ours; Theirs=$theirs; SourceRoot=$source; PsyZRevision=$pin; SDLHeaderRevision=$sdlPin; UpstreamPsyZ=$upstreamPin; Changes=$changes; Conflicts=$conflicts; Ready=($conflicts.Count -eq 0)} | ConvertTo-Json -Depth 6 | Set-Content "$out/CANDIDATE.json"
Write-Output "Candidate: $out"
if($conflicts.Count){throw "Resolve recorded conflicts in candidate only: $($conflicts -join ', ')"}

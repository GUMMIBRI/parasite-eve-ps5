# Package a linked game, optionally with explicitly selected local validated BINs.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$BuildOutput,
 [string]$Disc1,
 [string]$Disc2,
 [string]$AssetPreflight
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$BuildOutput=(Resolve-Path $BuildOutput).Path
$result=Get-Content "$BuildOutput/RESULT.json" -Raw | ConvertFrom-Json
$elf=Join-Path $BuildOutput 'b/parasite-eve-ps5.elf'
if(!(Test-Path $elf) -or (Get-FileHash $elf).Hash -ne $result.SHA256){throw 'Missing or changed successfully linked game ELF'}
$root=(Resolve-Path "$PSScriptRoot/../..").Path
$assetManifest=$null
if($Disc1 -or $Disc2 -or $AssetPreflight){
 if(!$Disc1 -or !$Disc2 -or !$AssetPreflight){throw 'Both BIN files and their asset preflight are required together'}
 $assetManifest=Get-Content (Join-Path $AssetPreflight 'RESULT.json') -Raw | ConvertFrom-Json
 if(!$assetManifest.DiscArchivesReadable){throw 'Asset preflight did not validate complete disc archives'}
 $discs=@((Resolve-Path $Disc1).Path,(Resolve-Path $Disc2).Path)
 for($i=0;$i -lt 2;$i++){
  if([IO.Path]::GetExtension($discs[$i]) -ine '.bin'){throw 'Asset copying currently accepts validated raw BIN files only'}
  if((Get-FileHash -LiteralPath $discs[$i]).Hash -ne $assetManifest.Files[$i].Hash){throw 'Disc differs from validated preflight input'}
 }
}
$out=Join-Path $root ('outputs/PS5-native-game-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))
New-Item -ItemType Directory $out | Out-Null
# A local test folder may contain retail images. Exclude the entire folder before
# creating/copying anything inside; no Git add, remote upload or deployment here.
Set-Content "$out/.gitignore" '*'
New-Item -ItemType Directory "$out/data/discs","$out/licenses" | Out-Null
Copy-Item $elf "$out/parasite-eve-ps5.elf"
Copy-Item "$BuildOutput/RESULT.json","$BuildOutput/INPUTS.json" $out
@'
# Example only: copy to /data/parasite-eve/parasite-eve.toml yourself.
# Preserve existing configuration, game images and memory cards.
[graphics]
internal_resolution = 1
fullscreen = false
square_pixels = false
disable_dithering = false
geometry_correction = false
antialiasing = false
model_filtering = false
[audio]
volume = 100
[discs]
disc1 = "discs/disc1.cue"
disc2 = "discs/disc2.cue"
'@ | Set-Content "$out/data/parasite-eve.toml.example"
@'
Native Parasite Eve PS5 ELF — compiled and linked; NOT TESTED ON CONSOLE.
This is the existing game's native modules, not a diagnostic ELF or emulator.

User handles all transfer, loader selection and console testing. This script
does not deploy or connect to a console. Only explicitly selected, preflighted
BIN images may be copied locally; source images and saves are preserved.

Prepare /data/parasite-eve yourself (or set PE_DATA_HOME in the loader).
Copy data/parasite-eve.toml.example there as parasite-eve.toml only if no existing
config is present. Adjust [discs] to your own USA CUE/BIN files. CUE track names
must match the actual BIN filenames; both original USA discs should be configured
before startup. Relative paths resolve under the data root. See the local-copy
note below to determine whether BINs were explicitly included in this folder.
The expected layout for this sample is:
 /data/parasite-eve/parasite-eve.toml
 /data/parasite-eve/discs/disc1.cue (+ its referenced BIN files)
 /data/parasite-eve/discs/disc2.cue (+ its referenced BIN files)
 /data/parasite-eve/memory-cards/bu00/ (created by game as needed)
 /data/parasite-eve/memory-cards/bu10/ (created by game as needed)
PE_CONFIG_HOME overrides configuration and memory-card location. Never replace
existing cards/saves. Existing portable card-import behavior is retained.

The public SDL3 PS5 drivers provide software VideoOut, AudioOut and ScePad calls.
The CPU PS1 rasterizer displays at native internal resolution; GPU/GL enhancements
are unavailable. Touchpad is PS1 Select by default. Press L3+R3 together to open
settings in the same window, using the existing
controller UI; movie-skip confirmation also uses the software surface.
Display, controller, sound, card writes and firmware service permissions are all
untested. Loader/runtime may require services/modules unavailable in its context.

Original textures come from your own disc images through the existing loaders.
External HD texpage-*.png packs with config.yaml are NOT supported by this native
renderer; it has no hash-matching PNG replacement loader. No HD assets are included.
This folder is local only and is not a public release or a console-success claim.
'@ | Set-Content "$out/USER-TEST.txt"
$notices=@('LICENSE.md','THIRD_PARTY_NOTICES.md','third_party/psyz/LICENSE','third_party/psyz/decomp/LICENSE','third_party/psyz/psyz/src/platform/LICENSE','third_party/psyz/psyz/src/psyz/LICENSE','third_party/psyz/external/SDL/LICENSE.txt','src/main/boot/fonts/OFL-IBMPlex.txt')
foreach($notice in $notices){
 if(!(Test-Path "$root/$notice")){throw "Required notice missing: $notice"}
 Copy-Item "$root/$notice" (Join-Path "$out/licenses" ($notice.Replace('/','_')))
}
Copy-Item "$PSScriptRoot/sdl3/audio/README.md" "$out/licenses/PS5-audio-provenance.md"
Copy-Item "$PSScriptRoot/sdl3/video/README.md" "$out/licenses/PS5-video-provenance.md"
Copy-Item "$PSScriptRoot/sdl3/joystick/README.md" "$out/licenses/PS5-joystick-provenance.md"
Copy-Item "$PSScriptRoot/tools/v0.45/ps5-payload-sdk/LICENSE.SDK" "$out/licenses/public-SDK-LICENSE.txt"
Copy-Item "$PSScriptRoot/tools/v0.45/ps5-payload-sdk/README.SOURCE.md" "$out/licenses/public-SDK-SOURCE.md"
Get-FileHash "$out/parasite-eve-ps5.elf" | Format-List | Out-File "$out/ELF-SHA256.txt"
if($assetManifest){
 for($i=0;$i -lt 2;$i++){
  $destination=Join-Path "$out/data/discs" "disc$($i+1).bin"
  Copy-Item -LiteralPath $discs[$i] -Destination $destination
  if((Get-FileHash -LiteralPath $destination).Hash -ne $assetManifest.Files[$i].Hash){throw 'Copied disc hash mismatch'}
 }
 $config=[IO.File]::ReadAllText("$out/data/parasite-eve.toml.example").Replace('disc1.cue','disc1.bin').Replace('disc2.cue','disc2.bin')
 [IO.File]::WriteAllText("$out/data/parasite-eve.toml",$config)
 Copy-Item (Join-Path $AssetPreflight 'RESULT.json') "$out/LOCAL-ASSET-PREFLIGHT.json"
 Add-Content "$out/USER-TEST.txt" "`nTHIS LOCAL COPY includes both validated original BINs and their original textures. Use data/parasite-eve.toml with discs/disc1.bin and discs/disc2.bin. No HD pack or standalone extracted PE.IMG is used. Never upload this folder."
}
Write-Output "Local user-test folder (console untested): $out"

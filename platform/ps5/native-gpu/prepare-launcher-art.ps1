# SPDX-License-Identifier: MIT
# Mechanical owner-art format conversion only; no downloads or console access.
param([Parameter(Mandatory=$true)][string]$Texconv,[Parameter(Mandatory=$true)][string]$BackgroundPng,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$expectedTool='dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06'
if((Get-FileHash -LiteralPath $Texconv -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedTool){throw 'Use the pinned public DirectXTex may2026 texconv.exe; digest mismatch.'}
$source=(Resolve-Path -LiteralPath $BackgroundPng).Path
if((Get-Item -LiteralPath $source).Length -gt 67108864){throw 'Source PNG exceeds64MiB.'}
$png=[IO.File]::ReadAllBytes($source)
if($png.Length -lt 24 -or [Convert]::ToHexString($png[0..7]) -ne '89504E470D0A1A0A'){throw 'A caller-supplied PNG is required.'}
function Read-Big32([byte[]]$Bytes,[int]$Offset){return ([uint32]$Bytes[$Offset]*16777216+[uint32]$Bytes[$Offset+1]*65536+[uint32]$Bytes[$Offset+2]*256+[uint32]$Bytes[$Offset+3])}
$w=Read-Big32 $png 16; $h=Read-Big32 $png 20
if(!$w -or !$h -or $w -gt 8192 -or $h -gt 8192 -or [uint64]$w*9 -ne [uint64]$h*16){throw 'Supply a composition-approved16:9 PNG; this recipe does not crop or invent composition.'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'Output directory must be fresh.'}
New-Item -ItemType Directory -Path $out | Out-Null
$work=Join-Path $out 'conversion-work'; New-Item -ItemType Directory -Path $work | Out-Null
& $Texconv -nologo -y -nogpu -w 3840 -h 2160 -m 1 -f BC7_UNORM -dx10 -o $work $BackgroundPng
if($LASTEXITCODE -ne 0){throw 'DirectXTex conversion failed.'}
$dds=Get-ChildItem -LiteralPath $work -File -Filter '*.dds'
if(@($dds).Count -ne 1){throw 'Expected one DDS result.'}
$bytes=[IO.File]::ReadAllBytes($dds.FullName)
function Read-Little32([int]$Offset){[BitConverter]::ToUInt32($bytes,$Offset)}
if([Text.Encoding]::ASCII.GetString($bytes,0,4) -ne 'DDS ' -or (Read-Little32 4) -ne 124 -or (Read-Little32 12) -ne 2160 -or (Read-Little32 16) -ne 3840 -or [Text.Encoding]::ASCII.GetString($bytes,84,4) -ne 'DX10' -or (Read-Little32 128) -ne 98 -or (Read-Little32 132) -ne 3 -or (Read-Little32 140) -ne 1 -or (Read-Little32 28) -ne 1 -or $bytes.Length -ne 8294548){throw 'Expected3840x2160 BC7_UNORM DX10 single2Dsurface/oneMip DDS.'}
$records=foreach($name in @('pic0.dds','pic1.dds')){$target=Join-Path $out $name;Copy-Item -LiteralPath $dds.FullName -Destination $target;@{file=$name;sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant();bytes=$bytes.Length;width=3840;height=2160;format='BC7_UNORM DX10';mips=1}}
@{files=@($records);sameCallerArtworkForSelectionAndLaunch=$true;gameplayQualified=$false;deployOnly=@('pic0.dds','pic1.dds');scratchNotForDeployment='conversion-work';tool='Microsoft DirectXTex may2026';toolSHA256=$expectedTool} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $out 'ARTIFACTS.json')


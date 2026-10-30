# SPDX-License-Identifier: MIT
# Local public FSELF assembly only. Never installs or validates console operation.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$BuildOutput,
 [Parameter(Mandatory)][string]$NativeTool,
 [Parameter(Mandatory)][string]$StubDirectory,
 [Parameter(Mandatory)][string]$LlvmBin,
 [Parameter(Mandatory)][string]$CompanionRuntime,
 [Parameter(Mandatory)][string]$ParamJson,
 [Parameter(Mandatory)][string]$Icon,
 [Parameter(Mandatory)][string]$OutputParent,
 [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Load([string]$p){Get-Content -LiteralPath $p -Raw | ConvertFrom-Json}
foreach($name in @('BuildOutput','NativeTool','StubDirectory','LlvmBin','CompanionRuntime','ParamJson','Icon','OutputParent')){
 Set-Variable -Name $name -Value (Resolve-Path -LiteralPath (Get-Variable $name -ValueOnly)).Path
}
$result=Load "$BuildOutput/RESULT.json"
if($result.Schema -ne 2 -or $result.State -cne 'succeeded'){throw 'Require successful schema 2 build'}
if((Hash "$BuildOutput/INPUTS.json") -ne $result.InputsSHA256){throw 'Build INPUTS hash mismatch'}
$inputs=Load "$BuildOutput/INPUTS.json"
$abi=Load "$BuildOutput/APPLICATION-ABI.json"
if(($inputs.PSObject.Properties.Name -contains 'ApplicationABI' -and $inputs.ApplicationABI -ne $true) -or $abi.ABI -cne 'native-application' -or $abi.PayloadCRT -ne $false -or $abi.Kernel -cne 'libkernel'){throw 'Require native application ABI without payload CRT'}
$elf=(Resolve-Path -LiteralPath $result.ELF).Path
$buildPrefix=$BuildOutput.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
if(!$elf.StartsWith($buildPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'ELF must belong to selected build output'}
if((Hash $elf) -ne $result.SHA256 -or (Get-Item -LiteralPath $elf).Length -ne $result.Bytes){throw 'Build ELF hash/size mismatch'}
$inputShape=(& "$PSScriptRoot/inspect-executable.ps1" -Path $elf | Out-String) | ConvertFrom-Json
if($inputShape.ElfType -cne '0x0003' -or $inputShape.HasPayloadRuntimeSymbol -ne $false -or $inputShape.EntryPoint -eq '0x0'){throw 'Require application-link PIE with entry and no payload runtime markers'}
foreach($entry in $abi.Inputs){
 if($entry.File -notin @('app_crt.cpp','ps5-pie.ld','app-symbols.map')){throw 'Unexpected ABI input'}
 if((Hash (Join-Path $abi.PublicTooling $entry.File)) -ne $entry.SHA256){throw 'ABI source hash mismatch'}
}
if(@($abi.Inputs).Count -ne 3 -or @($abi.Inputs.File | Select-Object -Unique).Count -ne 3){throw 'Incomplete ABI inputs'}
if((Get-Item -LiteralPath $CompanionRuntime).Length -ne 1284674 -or (Hash $CompanionRuntime) -ne 'e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036'){throw 'Wrong signed public companion runtime'}
$metadata=& "$PSScriptRoot/validate-application-metadata.ps1" -ParamJson $ParamJson
$image=[IO.File]::ReadAllBytes($Icon)
if($image.Length -lt 33 -or [BitConverter]::ToString($image,0,8) -cne '89-50-4E-47-0D-0A-1A-0A' -or [BitConverter]::ToString($image,8,8) -cne '00-00-00-0D-49-48-44-52' -or [BitConverter]::ToString($image,16,8) -cne '00-00-02-00-00-00-02-00'){throw 'Require PNG with 512x512 IHDR'}
foreach($tool in @('llvm-readelf.exe','llvm-nm.exe','llvm-objdump.exe')){
 if(!(Test-Path -LiteralPath (Join-Path $LlvmBin $tool) -PathType Leaf)){throw "Missing LLVM tool: $tool"}
}
$provenance=[ordered]@{BuildResultSHA256=(Hash "$BuildOutput/RESULT.json"); InputsSHA256=$result.InputsSHA256; AbiSHA256=(Hash "$BuildOutput/APPLICATION-ABI.json"); ElfSHA256=$result.SHA256; NativeToolSHA256=(Hash $NativeTool); CompanionSHA256=(Hash $CompanionRuntime); ParamSHA256=(Hash $ParamJson); IconSHA256=(Hash $Icon); PublicToolSource='https://github.com/SynoPiia/PSXS5/tree/5c17ea31aec5791c12451e915386fa98d670adb4/tooling/native'}
if($ValidateOnly){[pscustomobject]@{State='inputs-validated-only'; ConsoleValidated=$false; Provenance=$provenance}; return}
$out=Join-Path $OutputParent ('application-'+[Guid]::NewGuid().ToString('N'))
if(Test-Path -LiteralPath $out){throw 'Output already exists'}
$null=New-Item -ItemType Directory -Path $out
function SaveResult([string]$state){
 [ordered]@{Schema=1; State=$state; ConsoleValidated=$false; ProductionReady=$false; Provenance=$provenance; Outputs=$script:outputHashes} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$out/RESULT.json"
}
function Native([string]$log,[string[]]$arguments){
 & $NativeTool @arguments 2>&1 | Tee-Object -FilePath "$out/$log"
 if($LASTEXITCODE -ne 0){throw "Public tool failed: $log"}
}
$script:outputHashes=@()
SaveResult 'building'
try{
 & "$PSScriptRoot/audit-application-imports.ps1" -Elf $elf -StubDirectory $StubDirectory -ReadElf "$LlvmBin/llvm-readelf.exe" -Output "$out/imports.json"
 if((Load "$out/imports.json").ImportCount -le 0){throw 'Expected audited native application imports'}
 & "$PSScriptRoot/audit-application-runtime.ps1" -Elf $elf -ImportAudit "$out/imports.json" -LlvmBin $LlvmBin -Output "$out/runtime.json"
 if((Load "$out/runtime.json").Passed -ne $true){throw 'Runtime audit did not explicitly pass'}
 Native 'link.log' @('link','--in',$elf,'--out',"$out/eboot.elf",'--stub-dir',$StubDirectory,'--file-name','eboot.elf')
 & "$PSScriptRoot/inspect-executable.ps1" -Path "$out/eboot.elf" -RequireApplicationElf | Set-Content "$out/shape.json"
 if((Load "$out/shape.json").HasExpectedApplicationShape -ne $true){throw 'Application shape did not explicitly pass'}
 $null=New-Item -ItemType Directory -Path "$out/app/sce_module","$out/app/sce_sys"
 Native 'sign.log' @('self','--sign','--in',"$out/eboot.elf",'--out',"$out/app/eboot.bin")
 Native 'inspect.log' @('self','--inspect','--file',"$out/app/eboot.bin")
 $inspection=Get-Content -LiteralPath "$out/inspect.log" -Raw
 if($inspection -notmatch '(?m)^integrity:\s*valid\s*$' -or $inspection -match '(?im)\binvalid\b'){throw 'SELF integrity not explicitly valid'}
 Copy-Item -LiteralPath $CompanionRuntime -Destination "$out/app/sce_module/libc.prx"
 Copy-Item -LiteralPath $ParamJson -Destination "$out/app/sce_sys/param.json"
 Copy-Item -LiteralPath $Icon -Destination "$out/app/sce_sys/icon0.png"
 foreach($pair in @(@('sce_module/libc.prx',$provenance.CompanionSHA256),@('sce_sys/param.json',$provenance.ParamSHA256),@('sce_sys/icon0.png',$provenance.IconSHA256))){
  if((Hash "$out/app/$($pair[0])") -ne $pair[1]){throw 'Copied input changed during assembly'}
 }
 if((Hash $elf) -ne $provenance.ElfSHA256 -or (Hash $NativeTool) -ne $provenance.NativeToolSHA256){throw 'Conversion input/tool changed during assembly'}
 $script:outputHashes=@(Get-ChildItem -LiteralPath "$out/app" -File -Recurse | ForEach-Object {@{Path=$_.FullName.Substring($out.Length+1); SHA256=(Hash $_.FullName); Bytes=$_.Length}})
 SaveResult 'succeeded'
 Write-Output $out
}catch{SaveResult 'failed'; throw}


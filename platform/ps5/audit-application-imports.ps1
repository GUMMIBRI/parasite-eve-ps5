# Records the public converter's first-exporting-needed-module policy.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$Elf,
 [Parameter(Mandatory)][string]$StubDirectory,
 [Parameter(Mandatory)][string]$ReadElf,
 [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$dynamic=& $ReadElf -d $Elf
if($LASTEXITCODE){throw 'Cannot read application dynamic table'}
$needed=@($dynamic | ForEach-Object {if($_ -match '\(NEEDED\).*\[(.+)\]'){$Matches[1]}})
$providers=@()
foreach($name in $needed){
 $stub=Join-Path $StubDirectory ($name -replace '\.sprx$|\.prx$','.so')
 $symbols=& $ReadElf --dyn-syms --wide $stub
 if($LASTEXITCODE){throw "Cannot inspect public stub $stub"}
 $exports=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($line in $symbols){
  if($line -match '^\s*\d+:\s+\S+\s+\d+\s+\S+\s+(GLOBAL|WEAK)\s+\S+\s+(?!UND\b)\S+\s+(\S+)'){
   $null=$exports.Add($Matches[2])
  }
 }
 $providers+=@{Module=$name; Exports=$exports; SHA256=(Get-FileHash $stub).Hash}
}
$symbols=& $ReadElf --dyn-syms --wide $Elf
if($LASTEXITCODE){throw 'Cannot read game imports'}
$imports=@()
foreach($line in $symbols){
 if($line -match '^\s*\d+:\s+\S+\s+\d+\s+\S+\s+(GLOBAL|WEAK)\s+\S+\s+UND\s+(\S+)'){
  $binding=$Matches[1]; $name=$Matches[2]
  $provider=@($providers | Where-Object {$_.Exports.Contains($name)} | Select-Object -First 1)
  if(!$provider.Count){throw "Unresolved import: $name"}
  $imports+=@{Symbol=$name; Binding=$binding; Module=$provider[0].Module}
 }
}
if(@($imports | Where-Object {$_.Symbol -match '^(__dl|__crt|payload_)'}).Count){throw 'Payload callback import remains'}
[ordered]@{
 ELF=(Resolve-Path $Elf).Path
 SHA256=(Get-FileHash $Elf).Hash
 Modules=@($providers | ForEach-Object {@{Module=$_.Module; StubSHA256=$_.SHA256}})
 ImportCount=$imports.Count
 Imports=$imports
 Policy='First exporting module in DT_NEEDED order, matching pinned public converter'
 Limit='Public import availability only; runtime semantics and firmware13.60 remain untested'
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $Output

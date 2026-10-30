# Apply the reviewed built-in candidate to an exported game or patched PsyZ stage.
# No Git, console, package or runtime operations. Validate all files before writing.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Root,
      [Parameter(Mandatory)][ValidateSet('Game','PsyZ')][string]$Tree,
      [string]$Revision,
      [switch]$CheckOnly)
$ErrorActionPreference='Stop'
$Root=(Resolve-Path -LiteralPath $Root).Path
$versions=@{
 'b1c225aec3065b932b1a35229502c7666c38685c'='0.7.0'
 '8c52cdde5d60ee52f00913517ffb593e9521bf2d'='0.8.0'
}
if($Tree -eq 'Game'){
 $manifest=Join-Path (Split-Path $Root) 'CANDIDATE.json'
 if(Test-Path -LiteralPath $manifest){
  $candidate=ConvertFrom-Json -InputObject ([IO.File]::ReadAllText($manifest))
  if($Revision -and $Revision -ne $candidate.Theirs){throw 'Plugin revision differs from candidate'}
  $Revision=$candidate.Theirs
 }
 if(!$Revision -or !$versions.ContainsKey($Revision)){throw 'Unsupported game plugin revision; review anchors first'}
 $cmake=[IO.File]::ReadAllText((Join-Path $Root 'CMakeLists.txt'))
 $project='(?m)^project\(parasite-eve-pc VERSION '+[regex]::Escape($versions[$Revision])+' LANGUAGES C\)\r?$'
 if([regex]::Matches($cmake,$project).Count -ne 1){throw 'Plugin game version differs from selected revision'}
}elseif($Revision -notin @('daf1adc38a2a346462964e281dfa32fd74f6888e','db796262b2a696241ec8f3965ebbfbcbc45bfc30')){
 throw 'Unsupported PsyZ plugin revision; pass the selected build pin'
}
# Direct assignment avoids a nested array from Windows PowerShell 5.1 JSON output.
$entries=ConvertFrom-Json -InputObject ([IO.File]::ReadAllText("$PSScriptRoot/plugin-source.json"))
$pending=@()
foreach($entry in $entries | Where-Object Tree -eq $Tree){
 $path=Join-Path $Root $entry.Path
 $text=if(Test-Path -LiteralPath $path){[IO.File]::ReadAllText($path).Replace("`r`n","`n")}else{''}
 $afterMatches=[regex]::Matches($text,[regex]::Escape($entry.After)).Count
 if($afterMatches -gt 1){throw "Duplicate integrated plugin anchor: $($entry.Path)"}
 if($afterMatches -eq 1){
  if(!$entry.Before -and $text -ne $entry.After){throw "Plugin addition already differs: $($entry.Path)"}
  if($entry.Before -and [regex]::Matches($text,[regex]::Escape($entry.Before)).Count){throw "Mixed plugin anchors: $($entry.Path)"}
  continue
 }
 if(!$entry.Before){
  if($text){throw "Plugin addition already differs: $($entry.Path)"}
  $updated=$entry.After
 }else{
  $matches=[regex]::Matches($text,[regex]::Escape($entry.Before)).Count
  if($matches -ne 1){throw "Plugin source anchor differs: $($entry.Path); review instead of overwriting"}
  $updated=$text.Replace($entry.Before,$entry.After)
 }
 $pending+=@{Path=$path;Text=$updated}
}
if(!$CheckOnly){foreach($change in $pending){
 New-Item -ItemType Directory -Force (Split-Path $change.Path) | Out-Null
 [IO.File]::WriteAllText($change.Path,$change.Text)
}}
[pscustomobject]@{Tree=$Tree;Revision=$Revision;Changed=$pending.Count;CheckOnly=[bool]$CheckOnly}

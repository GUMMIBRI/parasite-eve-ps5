# Apply the reviewed built-in candidate to an exported game or patched PsyZ stage.
# No Git, console, package or runtime operations. Validate all files before writing.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Root,
      [Parameter(Mandatory)][ValidateSet('Game','PsyZ')][string]$Tree,
      [switch]$CheckOnly)
$ErrorActionPreference='Stop'
# Direct assignment avoids a nested array from Windows PowerShell 5.1 JSON output.
$entries=ConvertFrom-Json -InputObject ([IO.File]::ReadAllText("$PSScriptRoot/plugin-source.json"))
$pending=@()
foreach($entry in $entries | Where-Object Tree -eq $Tree){
 $path=Join-Path $Root $entry.Path
 $text=if(Test-Path -LiteralPath $path){[IO.File]::ReadAllText($path).Replace("`r`n","`n")}else{''}
 if($text.Contains($entry.After)){continue}
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
[pscustomobject]@{Tree=$Tree;Changed=$pending.Count;CheckOnly=[bool]$CheckOnly}


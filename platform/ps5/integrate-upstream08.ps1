# Deliberate original 0.8 adaptations; no prepared source tree is an input.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Root, [Parameter(Mandatory)][string]$Revision,
 [ValidateSet('MainConflict','Tree')][string]$Mode='Tree')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$recipe=Get-Content "$PSScriptRoot/upstream08-recipe.json" -Raw -Encoding UTF8 | ConvertFrom-Json
if($recipe.Schema -ne 1){throw 'Unsupported original 0.8 recipe schema'}
if($Revision -ne $recipe.Revision){throw '0.8 adaptation requires the exact reviewed original revision'}
$utf8=[Text.UTF8Encoding]::new($false)
function Replace-One([string]$Text,[string]$Old,[string]$New) {
 if(([regex]::Matches($Text,[regex]::Escape($Old))).Count -ne 1){throw "0.8 adaptation anchor is absent or ambiguous: $($Old.Substring(0,[Math]::Min(100,$Old.Length)))"}
 return $Text.Replace($Old,$New)
}
if($Mode -eq 'MainConflict'){
 $text=[IO.File]::ReadAllText($Root).Replace("`r`n","`n")
 foreach($entry in $recipe.Conflicts){
  $variants=@($entry)
  if($entry.PSObject.Properties.Name -contains 'Alternatives'){$variants+=@($entry.Alternatives)}
  $matching=@($variants | Where-Object {([regex]::Matches($text,[regex]::Escape($_.Old))).Count -eq 1})
  if($matching.Count -ne 1){throw "0.8 conflict anchor absent or ambiguous: $($entry.Name)"}
  $text=Replace-One $text $matching[0].Old $matching[0].New
 }
 if($text -match '(?m)^(<<<<<<< |=======\s*$|>>>>>>> )'){throw 'Unreviewed 0.8 main conflict remains'}
 [IO.File]::WriteAllText($Root,$text,$utf8)
 return
}
$file=Join-Path $Root 'CMakeLists.txt'
$text=[IO.File]::ReadAllText($file).Replace("`r`n","`n")
$start='# Desktop mod downloads use native TLS through libcurl and a bounded ZIP reader.'
$end='# The executable starts the original game and compiles its modules in place.'
$begin=$text.IndexOf($start); $finish=$text.IndexOf($end)
if($begin -lt 0 -or $finish -le $begin){throw '0.8 desktop dependency block absent'}
$block=$text.Substring($begin,$finish-$begin)
$block=Replace-One $block "add_library(pe_download_deps INTERFACE)`n" ''
$replacement=@'
# Desktop keeps the original download implementation and dependencies by default.
set(pe_desktop_download_default ON)
if(PE_PS5_GAME)
    set(pe_desktop_download_default OFF)
endif()
option(PE_DESKTOP_MOD_DOWNLOADS "Build desktop HD pack download services" ${pe_desktop_download_default})
if(PE_PS5_GAME AND PE_DESKTOP_MOD_DOWNLOADS)
    message(FATAL_ERROR "Desktop HD downloads are unsupported on PS5")
endif()
add_library(pe_download_deps INTERFACE)
if(PE_DESKTOP_MOD_DOWNLOADS)
    target_compile_definitions(pe_download_deps INTERFACE PE_DESKTOP_MOD_DOWNLOADS=1)
'@
$replacement+="`n"+$block+"else()`n    target_compile_definitions(pe_download_deps INTERFACE PE_DESKTOP_MOD_DOWNLOADS=0)`nendif()`n`n"
$text=Replace-One $text $text.Substring($begin,$finish-$begin) $replacement
[IO.File]::WriteAllText($file,$text,$utf8)
$file=Join-Path $Root 'src/main/boot/main.c'
$text=[IO.File]::ReadAllText($file).Replace("`r`n","`n")
$text=Replace-One $text '/* Versioned desktop background packs.' "#if PE_DESKTOP_MOD_DOWNLOADS`n/* Versioned desktop background packs."
$disabled=@'
#else
/* Unsupported service: no network thread, pack I/O, or simulated success. */
static struct { int development, enabled; char active[4096], version[32]; } host_hd;
static const char *Host_HdStatus(void) { return "HD backgrounds are unavailable on this platform."; }
static int Host_HdBusy(void) { return 0; }
static int Host_HdPercent(void) { return 0; }
static const char *Host_HdValue(int row) { (void)row; return "Unavailable"; }
static void Host_HdApply(void) { host_hd.enabled=0; Psyz_GpuSetSpriteAtlasesEnabled(0); }
static void Host_HdPoll(void) {}
static void Host_HdAction(int row) { (void)row; }
static void Host_HdInit(void) { Host_HdApply(); }
static void Host_HdStop(void) {}
static int Host_HdCli(int verify) { (void)verify; puts(Host_HdStatus()); return EXIT_FAILURE; }
#endif
'@
$text=Replace-One $text '#include "settings_overlay.h"' ($disabled+"`n#include `"settings_overlay.h`"")
[IO.File]::WriteAllText($file,$text,$utf8)

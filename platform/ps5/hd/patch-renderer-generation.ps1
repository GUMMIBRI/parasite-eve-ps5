# Diagnostic-only generated patch. Never apply to a live submodule.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Stage,
      [Parameter(Mandatory)][string]$OriginalSource)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$stagePath=(Resolve-Path -LiteralPath $Stage).Path
$repo=(Resolve-Path "$PSScriptRoot/../../..").Path
$outputs=(Resolve-Path "$repo/outputs").Path+[IO.Path]::DirectorySeparatorChar
if(!$stagePath.StartsWith($outputs,[StringComparison]::OrdinalIgnoreCase)){throw 'Stage must be isolated beneath repository outputs'}
if(Test-Path "$stagePath/.git"){throw 'Refusing a Git checkout'}
$pin='daf1adc38a2a346462964e281dfa32fd74f6888e'
$expected=& git -C "$repo/third_party/psyz" show "${pin}:psyz/src/platform/sdl3_software.c"
if($LASTEXITCODE){throw 'Pinned public source unavailable'}
$original=[IO.File]::ReadAllText((Resolve-Path $OriginalSource)).Replace("`r`n","`n")
if($original.TrimEnd() -cne (($expected -join "`n").TrimEnd())){throw 'OriginalSource differs from pinned daf1adc rasterizer'}
$dest="$stagePath/psyz/src/platform/sdl3_software.c"
$current=[IO.File]::ReadAllText($dest).Replace("`r`n","`n")
if($current.TrimEnd() -cne $original.TrimEnd()){throw 'Staged rasterizer is not pristine pinned source; refusing overwrite'}
$script:s=$original
function ReplaceOnce([string]$old,[string]$new){
 if(([regex]::Matches($script:s,[regex]::Escape($old))).Count -ne 1){throw "Unsupported seam: $old"}
 $script:s=$script:s.Replace($old,$new)
}
$diagnostics=@'
#if defined(PE_HD_RENDERER_GENERATION_DIAGNOSTICS) && PE_HD_RENDERER_GENERATION_DIAGNOSTICS
/* Renderer-owner thread only. No logging, allocation, sampling or replacement.
 * Saturating counters; rollover latches invalid until process lifetime ends.
 * Consumers MUST reject all epochs while failed != 0. No automatic reset. */
#include "pe_hd_generation.c"
static pe_hd_generation pe_hd_diag_tracker;
static uint64_t pe_hd_diag_counts[5]; /* pixel, clear, upload, move, reset */
static int pe_hd_diag_failed;
static void pe_hd_diag_touch(int kind, int x, int y, int w, int h) {
    int64_t x1=(int64_t)x+w, y1=(int64_t)y+h;
    if (w<=0 || h<=0) return;
    if (x<0) x=0;
    if (y<0) y=0;
    if (x1>1024) x1=1024;
    if (y1>512) y1=512;
    if (x1<=x || y1<=y) return;
    if (pe_hd_diag_counts[kind]!=UINT64_MAX) ++pe_hd_diag_counts[kind];
    if (pe_hd_diag_failed) return;
    if (!pe_hd_diag_tracker.epoch) pe_hd_generation_init(&pe_hd_diag_tracker);
    pe_hd_generation_rect r={x,y,(uint32_t)(x1-x),(uint32_t)(y1-y),PE_HD_GENERATION_CLIP};
    if (pe_hd_generation_touch(&pe_hd_diag_tracker,&r)!=PE_HD_GENERATION_OK)
        pe_hd_diag_failed=1;
}
/* Borrow only on the renderer owner thread, never across a write or worker.
 * No production consumers exist; these symbols are debugger/test observation. */
const pe_hd_generation *PeHdRendererGenerationDiagnosticTracker(void) {
    if (!pe_hd_diag_tracker.epoch) pe_hd_generation_init(&pe_hd_diag_tracker);
    return &pe_hd_diag_tracker;
}
int PeHdRendererGenerationDiagnosticRead(uint64_t counts[5], uint64_t *epoch) {
    if (!counts || !epoch) return -1;
    for (int i=0;i<5;++i) counts[i]=pe_hd_diag_counts[i];
    *epoch=PeHdRendererGenerationDiagnosticTracker()->epoch;
    return pe_hd_diag_failed;
}
#define PE_HD_DIAG_TOUCH(k,x,y,w,h) pe_hd_diag_touch(k,x,y,w,h)
#else
#define PE_HD_DIAG_TOUCH(k,x,y,w,h) ((void)0)
#endif

'@
ReplaceOnce 'static u16 vram[VRAM_W * VRAM_H];' ($diagnostics+"`nstatic u16 vram[VRAM_W * VRAM_H];")
ReplaceOnce '    *out =' "    PE_HD_DIAG_TOUCH(0,x,y,1,1);`n    *out ="
ReplaceOnce '    for (int y = y0; y < y1; y++) {' "    PE_HD_DIAG_TOUCH(1,x0,y0,x1-x0,y1-y0);`n    for (int y = y0; y < y1; y++) {"
ReplaceOnce '    const u16* src = (const u16*)data;' "    PE_HD_DIAG_TOUCH(2,rect->x,rect->y,rect->w,rect->h);`n    const u16* src = (const u16*)data;"
ReplaceOnce '    for (int row = 0; row < h; row++) {' "    PE_HD_DIAG_TOUCH(3,dst_x,dst_y,w,h);`n    for (int row = 0; row < h; row++) {"
ReplaceOnce '    memset(vram, 0, sizeof(vram));' "    PE_HD_DIAG_TOUCH(4,0,0,VRAM_W,VRAM_H);`n    memset(vram, 0, sizeof(vram));"
# Existing source is retained byte-for-byte within the OFF preprocessor branch.
$patched=$script:s
$script:s="#if !defined(PE_HD_RENDERER_GENERATION_DIAGNOSTICS) || !PE_HD_RENDERER_GENERATION_DIAGNOSTICS`n#line 1`n"+$original+"`n#else`n"+$patched+"`n#endif`n"
Copy-Item "$PSScriptRoot/pe_hd_generation.c","$PSScriptRoot/pe_hd_generation.h" (Split-Path $dest)
[IO.File]::WriteAllText($dest,$script:s)
@{Pin=$pin; DiagnosticOnly=$true; EnabledBy='PE_HD_RENDERER_GENERATION_DIAGNOSTICS=1'; OriginalSHA256=(Get-FileHash $OriginalSource).Hash; PatchedSHA256=(Get-FileHash $dest).Hash; GeneratorSHA256=(Get-FileHash $PSCommandPath).Hash; TrackerSHA256=(Get-FileHash "$PSScriptRoot/pe_hd_generation.c").Hash; ConsoleTested=$false} | ConvertTo-Json | Set-Content "$stagePath/RENDERER-GENERATION.json"


# Pinned CPU renderer generation diagnostics

`patch-renderer-generation.ps1` instruments an isolated generated PsyZ tree only.
Default OFF. It does not modify root CMake, the live submodule, application
scripts, staging targets, texture fetches, drawing operations or HD matching.
This is intentionally expensive diagnostic instrumentation, not a production
primitive batching policy or renderer replacement implementation.

The generator verifies the complete original rasterizer against public PsyZ
[`daf1adc38a2a346462964e281dfa32fd74f6888e`](https://github.com/khasinski/parasite-eve-psyz/blob/daf1adc38a2a346462964e281dfa32fd74f6888e/psyz/src/platform/sdl3_software.c)
from the locally available Git object, verifies the staged rasterizer is pristine,
and requires an isolated stage beneath repository outputs. Unknown sources and
already instrumented files reject before mutation. Use trusted caller-controlled
output directories: lexical output placement is not an untrusted symlink sandbox.
The existing pinned tracker c/h is copied beside the generated rasterizer and
compiled into that translation unit only in the ON branch. Do not also link a
second copy of the tracker implementation into this diagnostic target.

## Application and build

First use the existing game generator to prepare a fresh native source stage
with the explicit daf1adc pin and reviewed SDL3. Preserve existing candidates.
Export the original with Git show using UTF-8 and LF (PowerShell example):

```powershell
$pin='daf1adc38a2a346462964e281dfa32fd74f6888e'
$original=& git -C third_party/psyz show "${pin}:psyz/src/platform/sdl3_software.c"
if($LASTEXITCODE){throw 'Missing pin'}
[IO.File]::WriteAllText($OriginalFile,($original -join "`n")+"`n")
./platform/ps5/hd/patch-renderer-generation.ps1 -Stage $IsolatedPsyZStage -OriginalSource $OriginalFile
```

Configure a fresh native build using the same game source, reviewed SDL3 prefix,
SDL source/config, public SDK toolchain and LLVM tools as the normal generator.
Set PSYZ_DIR to the patched stage; explicitly append
`-DPE_HD_RENDERER_GENERATION_DIAGNOSTICS=1` to CMAKE_C_FLAGS (retain `-std=gnu11`).
No root or game-builder option is installed by this experimental patch. Leaving
the macro undefined or defining it to 0 selects the original source verbatim,
with `#line 1` preserving its logging line numbers. The existing game generator
does not enable this patch automatically. Apply after generation; verify its
record/hash still describes the staged source before building. No console action
or delivery packaging is part of this workflow.

## Actual seams and lifetime

* WritePixel: touch one committed destination word after all clipping, mask
  rejection and blending calculations, immediately before its existing store.
  Rejected writes do not touch. There is no delayed primitive flush or batching;
  subsequent texture fetches see the same VRAM writes in the same order.
* ClearImage: conservatively touch the clipped destination once before loops,
  including mask-preserved words. Empty rectangles do nothing.
* LoadImage: clip a conservative destination before either fast row memcpy or
  checked pixel upload. Both branches are covered. There is no write wrapping.
* MoveImage: touch only the existing effective clamped destination, after the
  empty check and before the original forward row memmove loop. Existing overlap
  behavior remains unchanged, including vertical overlap; no snapshot substitution.
* SoftwareRasterizer_Reset: touch all VRAM before memset. Never reset the tracker
  here: old numerical checkpoints must not silently alias. Software_Reset and
  StoreImage are state-only/reset and readback paths, respectively; neither touches.

The tracker and five counters have static fixed storage; no workers, locks,
filesystem, pixels, key hashing, allocations or log writes are added. Counters
saturate. Epoch exhaustion latches failed permanently for this process while
original writes continue. Consumers must reject every proof when failed != 0.
No automatic numeric epoch reuse or live checkpoint reset is offered.

`PeHdRendererGenerationDiagnosticRead(counts[5], &epoch)` copies counters in
pixel/clear/upload/move/reset order and returns 0 valid, 1 latched failure, -1 bad
outputs. `PeHdRendererGenerationDiagnosticTracker()` borrows const tracker storage.
Both are renderer-owner-thread only; serialize with all writes, never retain the
borrow across a write or worker handoff. There is no cross-thread snapshot API.
Unused accessors can be removed by native linker garbage collection. Static
`pe_hd_diag_tracker`, `pe_hd_diag_counts`, `pe_hd_diag_failed` remain debugger
symbols in the validated native link; counters have no on-screen UI or persistent
output. This does not replace existing startup logging.

## Validation and remaining production work

The isolated evidence batch compiles/links the actual game with these hooks.
Synthetic checks extract the generated ON function bodies (not rewritten pixel
algorithms) and supply minimal test state: mask/clipping, clipped upload,
horizontal overlapping move, CLUT change visible to the next fetch, readback,
full reset and rollover preserving writes. No console execution is implied.
OFF preprocessing matches original including __LINE__ after normalizing the
different input filenames and outer whitespace. Reapplication rejects unchanged.

HD07 remains a production batching/self-texture ordering task; HD10 remains the
reviewed target insertion/enablement task, HD11 actual snapshot/proof ownership.
All ten READY playlist items remain genuinely unresolved; no queue dispatch or
inference was performed. Before any production hook, main must decide bounded
primitive invalidation cost, E2/page/CLUT dependency spans, immutable snapshot
ownership, rollover/scene retirement, independently computed opaque hash, UV
subrect/filter/mask/STP/resolution semantics. These diagnostics authorize no
replacement draw or compatibility claim.

## Review/publication scope and provenance

Only two new source-side files: this document and
`platform/ps5/hd/patch-renderer-generation.ps1`. No existing 53-file publication
snapshot is changed. Review/merge and future root/build integration belong to
main. Exclude all outputs, fixtures, binaries and private assets from source PRs.
The generator is independent original glue under repository licensing policy;
generated source retains the complete pinned original. PsyZ root LICENSE marks
`psyz/src/platform/` MPL 2.0 and distinguishes other source/header licensing:
[pinned license](https://github.com/khasinski/parasite-eve-psyz/blob/daf1adc38a2a346462964e281dfa32fd74f6888e/LICENSE).
Preserve that full notice and existing tracker/source provenance. No DuckStation
NCND implementation or private pack was consulted or copied. No blanket license
for game code/assets is asserted.


# HD foundation source publication manifest

Exact source allowlist below. Main owns Git review/staging; this is not a blanket
add of platform/ps5 or outputs. Do not add generated binaries, outputs, SDK/LLVM
archives, private filenames/images, discs, saves, configuration or PS5 Current.
The two older build-check.ps1/build-cache-check.ps1 scripts are excluded from this
proposed foundation PR; validate-staging.ps1 is the reproducible developer entry.

- `cmake/ps5/hd.cmake`
- `cmake/ps5/cache.cmake`
- `platform/ps5/hd/check_cache.c`
- `platform/ps5/hd/check_image.c`
- `platform/ps5/hd/check_vram.c`
- `platform/ps5/hd/filename.cmake`
- `platform/ps5/hd/FILENAME.md`
- `platform/ps5/hd/generation.cmake`
- `platform/ps5/hd/GENERATION.md`
- `platform/ps5/hd/index.cmake`
- `platform/ps5/hd/manifest.cmake`
- `platform/ps5/hd/MANIFEST.md`
- `platform/ps5/hd/pe_hd_manifest.c`
- `platform/ps5/hd/pe_hd_manifest.h`
- `platform/ps5/hd/tests/check_manifest.c`
- `platform/ps5/hd/lookup.cmake`
- `platform/ps5/hd/LOOKUP.md`
- `platform/ps5/hd/PACK.md`
- `platform/ps5/hd/pack.cmake`
- `platform/ps5/hd/pe_hd_cache.c`
- `platform/ps5/hd/pe_hd_cache.h`
- `platform/ps5/hd/pe_hd_filename.c`
- `platform/ps5/hd/pe_hd_filename.h`
- `platform/ps5/hd/pe_hd_generation.c`
- `platform/ps5/hd/pe_hd_generation.h`
- `platform/ps5/hd/pe_hd_image.c`
- `platform/ps5/hd/pe_hd_image.h`
- `platform/ps5/hd/pe_hd_index.c`
- `platform/ps5/hd/pe_hd_index.h`
- `platform/ps5/hd/pe_hd_lookup.c`
- `platform/ps5/hd/pe_hd_lookup.h`
- `platform/ps5/hd/pe_hd_pack.c`
- `platform/ps5/hd/pe_hd_pack.h`
- `platform/ps5/hd/pe_hd_pipeline.c`
- `platform/ps5/hd/pe_hd_pipeline.h`
- `platform/ps5/hd/pe_hd_vram.c`
- `platform/ps5/hd/pe_hd_vram.h`
- `platform/ps5/hd/pipeline.cmake`
- `platform/ps5/hd/staging.cmake`
- `platform/ps5/hd/STAGING.md`
- `platform/ps5/hd/PUBLICATION.md`
- `platform/ps5/hd/validate-staging.ps1`
- `platform/ps5/hd/vram.cmake`
- `platform/ps5/hd/tests/check_generation.c`
- `platform/ps5/hd/tests/check_index.c`
- `platform/ps5/hd/tests/check_lookup_faults.c`
- `platform/ps5/hd/tests/check_lookup_real.c`
- `platform/ps5/hd/tests/check_pipeline.c`
- `platform/ps5/hd/tests/check_pack.c`
- `platform/ps5/hd/tests/CMakeLists.txt`
- `platform/ps5/hd/tests/synthetic_png.h`
- `platform/ps5/hd/tests/generation/CMakeLists.txt`
- `platform/ps5/hd/tests/lookup/CMakeLists.txt`
- `platform/ps5/hd/vendor/LICENSE-MIT.txt`
- `platform/ps5/hd/vendor/PROVENANCE.md`
- `platform/ps5/hd/vendor/stb_image.h`

## Licensing and provenance

Original HD module/test/CMake/runner documentation follows LICENSE.md: the project
has selected no open-source license; author default copyright applies. Do not
label these files MIT/GPL or imply a blanket upstream license. Main must preserve
that policy or obtain explicit author authorization for any license change.

Only bundled third-party code is vendor/stb_image.h, with its full dual license
notice preserved; select MIT alternative and retain vendor/LICENSE-MIT.txt plus
vendor/PROVENANCE.md. Exact local source/blob/SHA256 and public upstream pin are
recorded there. SDL is an external caller-supplied dependency, zlib licensed:
https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt . Public SDK and LLVM are
external build tools, not PR payloads; preserve their own notices when distributing
them. No SDK/compiler runtime archive is included by this manifest.

Filename behavior reference (documentation, no implementation copied):
https://github.com/stenzek/duckstation/wiki/Texture-Replacements/014974199fab7227e3920618b1ce51c0693cdaeb
PS1 format reference: https://psx-spx.consoledev.net/graphicsprocessingunitgpu/ .
Existing PsyZ CPU SampleTexture/RasterTriangle/store seams were inspected at
pinned daf1adc (see GENERATION.md); no rasterizer source is vendored here.
No DuckStation CC BY-NC-ND implementation is copied/adapted. Test PNG and VRAM
patterns are synthetic original fixtures; no game or pack inputs are required.

## Remaining renderer decisions (not solved by staging)

- Hash: independent algorithm, byte order and exact packed-word/CLUT spans;
  filename hashes remain opaque, extraction metadata is not identity proof.
- Snapshot/state: serialized VRAM snapshot plus complete packed-word AND CLUT
  dependencies, E2/page/palette/filter identity, no state/epoch ABA; primitive
  write batching and self-texturing fallback require actual renderer ownership.
- UV: region/subrect, texture window, page/wrap and nearest sampling conversion;
  lookup currently only full P4/P8 full-CLUT uniformly scaled candidates.
- Mask: original zero/STP, primitive semi-transparency, destination mask/test and
  PNG alpha policy must be established together; READY never authorizes drawing.
- Resolution: actual CPU/GL output target and raster-coordinate scaling, primitive
  edges and framebuffer/readback effects. Bigger PNGs alone do not raise native
  raster output resolution. No renderer hook or replacement claim exists.
- Provider/manifest: bounded TSV ingestion and trusted immutable-root provider are implemented; optional untrusted-root authority backend remains unresolved. No private pack scan or preload.
- Lifecycle: one serialized lookup owner, shared token retirement, staging/frame
  phase split, release every external lease before final close; retain pipeline
  and provider user until successful close. Rollover needs discard-before-reinit.

Ten genuine READY tasks remain in QWEN-PLAYLIST.md; completed foundations and
coherent validation must not be regenerated. No inference is authorized here.




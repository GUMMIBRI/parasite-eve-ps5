# Standalone tile generation tracker and CPU insertion map

Independent default-OFF foundation, no renderer modifications or active HD
matching. Include generation.cmake and explicitly set PE_PS5_HD_GENERATION=ON
to obtain pe-hd-generation. No SDL dependency, root CMake insertion or staging
target change. Existing payload/application builds retain their behavior.

pe_hd_generation owns 2048 16x16-word tile epochs plus one clock: 16,392 bytes
with uint64_t storage. No heap, I/O, workers or locks. Rectangles are half-open
WORD coordinates. Clip and wrap are explicit modes; signed origins normalize
without signed overflow. Dimensions are bounded to VRAM size. Each axis splits
into at most two spans. At most 2145 tile visits per operation (a split can visit
one boundary tile twice); no scan of image bytes. Tile precision deliberately
allows false invalidation; it cannot prove equal pixel content.

Initialize once under renderer ownership. Capture tracker.epoch together with
the immutable snapshot. Later query all source-word and palette rectangles
against that checkpoint; any changed result invalidates the snapshot-derived
request. Serialize calls with writes and snapshot acquisition; this is not an
atomic cross-thread snapshot facility. Workers receive immutable snapshots and
never borrow live tracker storage. This clock is separate from pipeline scene
generation. At rollover, touch returns PE_HD_GENERATION_ROLLOVER unchanged:
discard dependent snapshots/requests/keys/checkpoints before init and retry.
Old numeric checkpoints can alias after reset; no call can detect misuse of an
old lifetime. Caller must enforce this barrier. A rectangle with an empty clipped
intersection does not advance or require rollover. Conservative touches may
invalidate unchanged/mask-skipped pixels, which is safe but may reduce hits.

## Pinned CPU source map for main review

Reference inspected with read-only git show, leaving local submodule unchanged:
[PsyZ sdl3_software.c at daf1adc38a2a346462964e281dfa32fd74f6888e](https://github.com/khasinski/parasite-eve-psyz/blob/daf1adc38a2a346462964e281dfa32fd74f6888e/psyz/src/platform/sdl3_software.c).
Line anchors below refer to that exact source, not the local 7409317 checkout.
No third-party implementation was copied into the tracker.

| Source seam | Verified behavior | Future owner action (not installed) |
| --- | --- | --- |
| WritePixel, 65–107 | Clips draw area and VRAM, skips preserved masked pixels; final store at 106 | Cover committed destination writes from triangles, sprites and DrawLine. Per-pixel touch is correct but too costly; batch conservative primitive/tile coverage without missing writes, and define self-texturing ordering first. |
| SampleTexture, 110–130 | E2 then 8-bit UV; tpage X; packed nibble/byte reads; X wraps; CLUT X wraps within one row | Snapshot packed words AND full relevant CLUT; transform UV/window into dependency spans. No Y wrap is performed here beyond the page/8-bit construction. |
| RasterTriangle texture branch, 259; RasterSprite, 345 | Actual texture fetches before WritePixel | Future insertion must select replacement before repeated per-pixel fetches, with original fallback. Do not perform blocking lookup/decode inside SampleTexture. |
| Software_ClearImage, 409–424 | Clipped rectangle; mask preservation can skip stores | Touch conservative clipped destination once per operation; never wrap this write. |
| Software_LoadImage, 426–448 | In-bounds row memcpy or per-pixel bounds checks; no wrap | Touch clipped destination, including fast memcpy path. |
| Software_StoreImage, 450–474 | VRAM readback only | No dirty touch. Synchronize with writes if snapshot ownership crosses threads. |
| Software_MoveImage, 476–493 | Clamps origins; limits extents to both source and destination; forward row memmove | Touch only effective destination after computing exact clamped dimensions. Negative origins are clamped rather than clipped with source offsets. Preserve existing overlap behavior; do not introduce toroidal writes. |
| SoftwareRasterizer_Reset, 613–620 | Clears entire VRAM then resets draw/window/grid state | Invalidate all VRAM dependencies and state-derived keys. Software_Reset at 361 only resets draw state and does not clear VRAM. |
| Software_SetTextureWindow, 375–383; SetTexpageMode, 369 | Sampling state can change without writes | Include approved state in key/dependency construction or invalidate state-derived requests separately. |
| SoftwareRasterizer_GetVram, 622 | Returns const pointer to live static VRAM | Not an immutable snapshot. Caller must synchronize copy/use against every writer. |

Pending renderer decisions: primitive batching, read-after-write feedback within
one primitive, snapshot lifetime and request coalescing, scene/reload events,
E2 dependency mapping, opaque compatible hash producer and mask/UV replacement
semantics. This tracker cannot solve them by itself. Main owns renderer source
changes. Existing vram extractor semantics remain unchanged; no GPU/GL claim.

## Reproducible synthetic checks

All paths are caller supplied. No download or absolute developer path required:

```powershell
& $CMake -S platform/ps5/hd/tests/generation -B $Build -G Ninja `
  "-DCMAKE_MAKE_PROGRAM=$Ninja" "-DCMAKE_C_COMPILER=$Compiler" `
  "-DCMAKE_AR=$Archiver" "-DCMAKE_RANLIB=$Ranlib" `
  -DPE_PS5_HD_GENERATION=ON -DCMAKE_BUILD_TYPE=Release
& $CMake --build $Build
& $CTest --test-dir $Build --output-on-failure --no-tests=error
```

For Zig supply -DCMAKE_C_COMPILER_ARG1=cc on initial configure. Archiver must
also be specified on initial configure (CMake compiler platform rules may retain
an initial NOTFOUND value). For public SDK compilation use its prospero.cmake
toolchain in place of CMAKE_C_COMPILER, with matching LLVM tools on PATH and
explicit LLVM archiver/ranlib. Cross compilation creates no runnable CTest
entry. For OFF omit the ON option in a fresh tree: no generation target exists.
Release assertions are forced enabled in synthetic checks. Tests compare tile
coverage to an independent word-by-word oracle, signed extremes, clipped empty
rectangles, both-edge wrap/full VRAM, overlapping tile boundaries, unchanged
regions and injected UINT64_MAX rollover. No private assets or console needed.

Independent original source follows repository licensing policy; existing PsyZ
and SDK notices are unchanged. No DuckStation NCND source was used. Reviewable
files: this document, pe_hd_generation.c/h, generation.cmake,
tests/check_generation.c and tests/generation/CMakeLists.txt; queue edits are
administrative. outputs/ is evidence only and must not enter public source PR.


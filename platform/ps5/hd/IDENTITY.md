# Bounded texture-page content identity

`pe_hd_identity.c/h` computes content keys from caller-owned immutable VRAM.
`identity.cmake` is standalone, default OFF, and is intentionally not added to
the PR8 staging snapshot. It performs no renderer insertion or replacement.

## Public contract and provenance

The public DuckStation [wiki revision](https://github.com/stenzek/duckstation/wiki/Texture-Replacement/014974199fab7227e3920618b1ce51c0693cdaeb)
describes filename fields and page mode but does not specify exact hashing.
The previously cited plural `Texture-Replacements` URL is not the working page.
The implementation here uses factual byte-level behavior documented in
[psxrecomp format notes at f9db256](https://github.com/RetroPortingToolKit/psxrecomp/blob/f9db2561d0f59921978f065fbc30f947e882fdef/docs/DUCKSTATION_TEXTURE_FORMAT.md),
Git blob `855804182a6e03a17a78ae57d4979e2eff6b4723`.
That document identifies its observed DuckStation reference as
`697599c47a646a6cfcc4d246018adf4904d55001`. It also publishes independent Python
xxhash vectors. No implementation from either emulator/framework was copied,
translated or adapted. psxrecomp states PolyForm Noncommercial terms; those
terms are not used as a source-code license for this module. The contract's
factual algorithm/input descriptions and literal numeric vectors were used to
write independent C code. This is not proof of behavior at every other revision
or of compatibility with any owner-supplied pack's generator version.

Historical reuse was checked rather than assuming DuckStation was always GPL:

- [aa955b8 LICENSE](https://github.com/stenzek/duckstation/blob/aa955b8ae28314ae061613f0ddf13183a98aca03/LICENSE)
  is GPLv3. Its `texture_replacements.cpp` header offers GPL-3.0 OR
  CC-BY-NC-ND-4.0, but implements the older 128-bit `vram-write` identity.
  Its complete nontruncated Git tree has no `gpu_hw_texture_cache.cpp`.
- [license change 7f4e5d5](https://github.com/stenzek/duckstation/commit/7f4e5d55dbdef5a50e0aa4994f667fb03d854928)
  follows that parent. [PR3244](https://github.com/stenzek/duckstation/pull/3244)
  was merged September 29, 2024. Its single commit
  `e06f1f100204a60b28d34c380cc91a406b8e3e6a` has a December 2023 author date
  but September 2024 committer date; both its LICENSE and texture-cache SPDX
  header are CC-BY-NC-ND-4.0. The older author date is not GPL provenance.
- The immediate parent `4132b5ef3d120f851b553bf7367d3bf4e729c403` likewise has
  no hardware texture-cache file. This bounded audit did not locate a licensed
  historical implementation of the requested page format; it does not prove
  that no independently archived development branch could exist.

The only vendored executable algorithm source is the official
[xxHash v0.8.3 header](https://github.com/Cyan4973/xxHash/blob/e626a72bc2321cd320e953a0ccf1584cad60f363/xxhash.h),
commit `e626a72bc2321cd320e953a0ccf1584cad60f363`, under BSD-2-Clause.
`vendor/xxhash-v0.8.3.h` is byte-for-byte unmodified, SHA256
`17973c0dc49d9854ca26caa191f0e12f7a424b68858d9a78de3860d959d85e4b`.
Preserve its embedded notice and `vendor/xxhash-v0.8.3-LICENSE.txt` when publishing.
[v0.8.0 release](https://github.com/Cyan4973/xxHash/releases/tag/v0.8.0)
established stable XXH3 output. No new license for the surrounding game is inferred.

## Supported byte contract

Both fields use unseeded XXH3-64. Texture input is the selected page subrectangle
of packed PS1 words, traversed row by row, low byte then high byte, with no
stride padding, dimensions, palette or decoded indices appended. Native source
size is 64x256 words for P4 and 128x256 for P8. Filename horizontal offset and
extent are expanded texels: divide by four or two to locate packed words.
Page origin comes from the caller, never from the filename hash.

Full CLUT input is 16 or 256 native words, in the same byte order, including
bit15. Ordinary and STP names use identical content hashes; mode stays part of
the returned fieldwise key. Pixel-zero handling and STP blending are renderer
semantics, not modifications to these bytes.

Inputs require a complete synchronized 1024x512 word snapshot, explicit stride
and readable span. Page origins must be valid GP0 texture-page coordinates;
CLUT X must be 16-word aligned. Complete page and CLUT footprints must fit
without wrapping. Subrectangles must be nonempty, word aligned, inside256x256.
Budget counts texture bytes plus palette bytes hashed; maximum supported work
is 66048 bytes for a full P8 page plus CLUT. Stack scratch is512bytes plus the
xxHash state. No allocation, I/O, borrowed output, or persistent state exists.
Output remains unchanged on invalid/unsupported/overbudget input. Input span
arithmetic rejects a word extent whose byte extent could overflow size_t.

Reduced ranges are deliberately unsupported. The format notes report a
revision-specific prefix behavior (not offset by palette minimum) and a P8
right-edge truncation behavior. Neither is silently generalized here. Upload
identities, direct color, windowed or wrapped matching also remain unsupported.
Do not substitute the old GPL `vram-write` XXH3-128 algorithm for these fields.

## Build and validation

Use the new isolated harness; no SDL is required. Supply local tools, never
download during configure. In a shell with CMake, Ninja and a C compiler:

```sh
cmake -S platform/ps5/hd/tests/identity -B <new-host-dir> -G Ninja -DPE_PS5_HD_IDENTITY=ON -DCMAKE_C_COMPILER=<compiler> -DCMAKE_AR=<archiver> -DCMAKE_RANLIB=<ranlib>
cmake --build <new-host-dir>
ctest --test-dir <new-host-dir> --output-on-failure
```

For Zig supply `-DCMAKE_C_COMPILER_ARG1=cc` and its executable as compiler.
For public PS5 SDK use a fresh directory, its `toolchain/prospero.cmake`, and
matching LLVM ar/ranlib, with LLVM bin on PATH; cross builds compile/link the
fixture only and must never execute it. Omitting `PE_PS5_HD_IDENTITY` exercises
default OFF and asserts no target is created. Include is idempotent.

Tests use literal public expected vectors for4,8,16,256,8192 words plus stride,
crop, unrelated/related mutations, bit15, mode, budgets, palette/page edge and
overflow rejection. This proves the implemented subset against those vectors;
it is not a page-dump oracle from a running DuckStation or PS5 console test.

## Next renderer boundary

The caller must prove mode/page/CLUT, selected candidate geometry, immutable
snapshot generation, complete packed-word AND CLUT dependencies, and sampling
state. Compute key before passing to existing index/lookup; never iterate an
unbounded pack on every draw. The current lookup accepts only full-page,
full-CLUT ordinary P4/P8 uniformly scaled draws and rejects unsupported flags.
This helper can hash subrectangles and STP names for later work, but that does
not authorize the existing lookup to display them. Do not clear unsupported
flags to force a match. Mutation between snapshot/hash/acquire/draw requires
fallback; generation instrumentation alone does not establish synchronization.
The existing extractor's wrapping CLUT sampling and this nonwrapping identity
subset intentionally differ; edge cases must fall back, not hash decoded RGBA.

Next proof is a public synthetic PAGE-mode oracle for the exact targeted
generator revision plus a reviewed actual PsyZ snapshot/matching adapter.
No private filenames or image pixels were scanned to infer this contract.


# Synchronized full-page snapshot bridge

`pe_hd_snapshot.c/h` produces one owned content key and two generation dependency
rectangles from the SAME decoded page/CLUT request. It consumes an immutable,
caller-synchronized 1024x512 native-word snapshot, with explicit stride/readable
span, and a live initialized generation tracker. There is no allocation, I/O,
pixel decode, retained snapshot pointer, renderer insertion or replacement draw.
Maximum hashing budget is66048bytes (P8); P4 needs32800bytes. Input VRAM storage
is caller-owned and is not counted against that work budget.

Only ordinary P4/P8, full256x256 texels, full16/256-entry CLUT, no texture window
or wrapping footprint is supported. Page and CLUT origins use the existing
`pe_hd_vram_request` GP0 fields and exactly the extractor's bit decoding. Texture
dependencies are64x256 or128x256 VRAM WORDS; palette dependency is16x1 or256x1
WORDS. Crops/UV origins, reserved/direct depth, window, reserved page/CLUT bits
and right-edge wrapping P8 page/palette footprints fail closed. No reduced-CLUT
input or STP filename mode exists in this interface, so neither can accidentally
produce an ordinary fallback key. CLUT color bit15 is preserved in the hash;
rejecting STP filename mode does not mean rejecting palette words with bit15.

The caller MUST obtain the immutable snapshot and tracker epoch under the same
VRAM-owner synchronization, and hold that serialization through this call. The
adapter cannot detect an old snapshot incorrectly paired with a newer tracker.
It samples the current epoch and never creates a new scene/pipeline generation.
Tracker reinitialization requires discarding every derived snapshot/proof/token
first. Output has no borrowed memory; checkpoint validity is tracker-local.
Failure leaves the entire output unchanged, including padding. Caller storage
must be disjoint. No tracker integrity validation beyond initialized epoch is
possible for this caller-owned public structure.

## Existing index/lookup boundary

Use the returned key with the immutable index's fieldwise lookup; do not hash
decoded RGBA or change palette range/mode to force a match. For lookup, copy the
two dependency rectangles and checkpoint into `pe_hd_lookup_proof`, set count2,
retain the SAME tracker, and supply a separately observed nonzero sampling-state
ID. Populate observation from real renderer state and independently verified
replacement dimensions. Both rectangle checks are required, even if overlapping.
The helper deliberately does not claim draw compatibility or fill unsupported
flags/state IDs on the caller's behalf. Filtering, mask/blend, UV scaling and
read-after-write/self-texturing still require reviewed fallback decisions.
Validate generation/state again through actual draw consumption; snapshot
synchronization and a pinned replacement lease are distinct lifetimes.

This subset matches the full-page/full-CLUT key shape admitted by existing
lookup; index already compares fields without struct padding. It establishes
neither a specific private pack's generator version nor an actual renderer hook.

## Build and tests

No root CMake or preserved staging aggregator changes are needed. Include
`snapshot.cmake`; defaults create no targets. When opting in explicitly enable
`PE_PS5_HD_SNAPSHOT`, `PE_PS5_HD_IDENTITY`, `PE_PS5_HD_GENERATION`. Missing identity
or generation targets cause configuration failure instead of implicit stubs.
The isolated fixture also requires `PE_PS5_HD_VRAM=ON` for decoder cross-checks.

```sh
cmake -S platform/ps5/hd/tests/snapshot -B <fresh-host-dir> -G Ninja -DPE_PS5_HD_SNAPSHOT=ON -DPE_PS5_HD_IDENTITY=ON -DPE_PS5_HD_GENERATION=ON -DPE_PS5_HD_VRAM=ON -DCMAKE_C_COMPILER=<compiler> -DCMAKE_AR=<archiver> -DCMAKE_RANLIB=<ranlib>
cmake --build <fresh-host-dir>
ctest --test-dir <fresh-host-dir> --output-on-failure
```

For Zig pass `-DCMAKE_C_COMPILER_ARG1=cc` and supply writable local/global cache
directories in restricted environments. For public PS5 SDK use its
`toolchain/prospero.cmake`, matching LLVM ar/ranlib and LLVM bin on PATH, in a
fresh directory. Cross builds compile/link only; never execute the fixture.
Omit all options to validate default OFF; enable snapshot alone for the intended
missing-dependency rejection. No configure-time downloads or SDL dependency.

Regressions cover padded stride, P4 nibble/P8 byte extraction agreement,
packed-word/CLUT dependency coverage, unrelated writes, palette bit15 identity
and invalidation, packed mutation invalidation, overbudget/short/overflow span,
uninitialized tracker, unsupported window/crop/depth/reserved bits/wrap, and
error output preservation. Hash primitive/vector provenance remains in
`IDENTITY.md`; this bridge adds no hash algorithm or new external source.

## Publication addendum outside preserved PR8 snapshot

New source files only: `pe_hd_snapshot.c`, `pe_hd_snapshot.h`, `snapshot.cmake`,
`SNAPSHOT.md`, `tests/check_snapshot.c`, `tests/snapshot/CMakeLists.txt`, all under
`platform/ps5/hd/`. Review with the separately added identity source/dependency
manifest; preserve unmodified xxHash BSD-2-Clause notices and pinned provenance
in `IDENTITY.md`. Adapter/test code is independently written from existing local
API declarations; no NCND implementation copied/adapted and no new blanket game
license inferred. Existing PR8 files, root/game/app code and assets are unchanged.
Never publish `outputs/`, local SDK binaries, private pack files or user data.


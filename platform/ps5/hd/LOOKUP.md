# Independent HD candidate coordinator

`pe_hd_lookup.c/h` is original independent source under the repository licensing
policy. It uses existing filename/index/pipeline/generation APIs only. No
DuckStation implementation, compatibility hash, private pack or renderer code
was used. Existing third-party licenses are unchanged.

Include `lookup.cmake` after enabled `pipeline.cmake` and `generation.cmake`.
`PE_PS5_HD_LOOKUP` defaults OFF; inclusion with defaults creates no targets,
SDL lookup or game dependency. Repeated inclusion is safe. Enabled target
`pe-hd-lookup` links `pe-hd-pipeline` and `pe-hd-generation`. Root CMake remains
unchanged. The standalone staging aggregator now enables lookup and generation
in dependency order; the actual renderer integration still belongs to main.

## Ownership and bounded work

The coordinator borrows one fresh pipeline at scene generation 1, exclusively
administering it until coordinator destruction. One owner serializes staging,
frame acquisition/release, VRAM writes and tracker observations. This API is
deliberately not a concurrent renderer/staging scheduler. Its frame calls do no
allocation, reads or decode; pipeline/cache acquisition and release use their
try-lock contracts. `prepare`, `retire`, `generation` and `destroy` are staging
calls and may lock pipeline administration; never call them in a draw callback.

One fixed allocation contains 16 slots, each with at most four dependency
rectangles, one pipeline token and one emergency owning lease. Add the reported
metadata bytes to pipeline/index/cache budgets and runtime/allocator overhead;
this is not a total process memory cap. No encoded or decoded bytes are copied.
Proof checks have a fixed bound of four tracker queries (at most 2048 tiles
each); requests use a 16-entry linear search, with fieldwise comparisons only.

`prepare` takes an independently computed opaque key, separately observed
texture metadata, explicit expected replacement dimensions and snapshot proof.
It coalesces only identical key/observation/proof combinations; different
checkpoints are deliberately separate. Tokens are shared borrowed identities,
without reference counts. Caller decides when all token users have retired;
retiring one invalidates every coalesced alias. Every successful acquire gives
an independent owning lease, which must be released exactly once. BUSY retains
ownership for retry. Never copy a live lease or coordinator.

Missing entries, decode failure, stale dependencies/state and size mismatch
become terminal fallback until retirement or scene generation. Repeated prepare
does not submit retries for terminal slots. Pipeline FULL returns FULL without
allocating a coordinator slot. Explicit retry can still exhaust the underlying
bounded cache; no hidden eviction/backoff scheduler or guaranteed retry success.
No per-frame read/decode/retry loop is installed.

On unexpected decoded dimensions/byte count, pixels never reach the caller.
An internal lease retains ownership if try-release is BUSY. Further acquire
cannot overwrite it; staging retire/generation/destroy drain it before clearing
the slot. BUSY retains the coordinator for retry. External leases survive token
retirement and scene changes. Destroy retires tokens and frees the coordinator,
but leaves the pipeline alive: release any external leases directly through
pipeline_release and then use pipeline_close. Join/serialize releases before
final freeing close as documented by pipeline/cache.

## Proof and unsupported behavior

Caller certifies complete source-word AND CLUT coverage in 1..4 nonempty
rectangles, captured together with an immutable snapshot and tracker checkpoint.
Empty clipped dependencies, invalid/future checkpoints and changed tiles reject.
The coordinator cannot infer completeness, hash compatibility, units or safe
snapshot ownership from a filename. Tracker storage outlives all referencing
tokens. Caller supplies a nonzero sampling-state identity covering every E2,
page, CLUT, filtering and scene change, without identity reuse during lifetime.

Every acquire rechecks proof and current state. Keep them valid through draw;
self-texturing/read-after-write feedback requires original fallback. A lease
pins pixels, not the continued validity of renderer inputs. Scene generation
strictly increases and cancels all old tokens while leases survive. Tracker
epoch is a separate clock. Before tracker reinit/rollover, cancel tokens and
discard ALL old snapshots/keys/proofs; do not reuse old proof numbers. Serial
exhaustion refuses new requests. UINT64_MAX scene generation cannot advance;
drain/destroy/recreate the entire lifetime before reuse, discarding all tokens.

Only full-region P4/P8 identities with complete 16/256-color palette range,
zero subrect origin and positive uniform integer replacement scale are staged.
Observed fields must equal key fields; observed hash fields are ignored. Width
and height are bounded by the image decoder limits with overflow-safe byte
policy. Any special filter, texture window, STP/mask/blend or partial/subrect
usage must set `unsupported` and fall back. STP filename modes always fall back.
This conservative candidate subset is NOT a replacement rendering contract:
even READY does not authorize drawing. Actual UV mapping, zero/STP/mask/alpha
composition, primitive ordering and renderer output resolution remain unbound.

## Reproduction

Use explicit caller tools/paths; no download or private assets are needed:

```powershell
& $CMake -S platform/ps5/hd/tests/lookup -B $Build -G Ninja `
  "-DCMAKE_MAKE_PROGRAM=$Ninja" "-DCMAKE_C_COMPILER=$Compiler" `
  "-DCMAKE_AR=$Archiver" "-DCMAKE_RANLIB=$Ranlib" `
  "-DSDL3_DIR=$HostSDL/lib/cmake/SDL3" -DCMAKE_BUILD_TYPE=Release `
  -DPE_PS5_HD_LOOKUP=ON -DPE_HD_LOOKUP_CHECKS=ON
& $CMake --build $Build
& $CTest --test-dir $Build --output-on-failure --no-tests=error
```

For Zig supply `-DCMAKE_C_COMPILER_ARG1=cc` on first configure and writable
`ZIG_GLOBAL_CACHE_DIR`/`ZIG_LOCAL_CACHE_DIR`. Match any sanitizer runtime required
by the supplied host SDL (the local host SDL requires
`-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined`). For SDK compile/link checks,
replace compiler with public SDK `-DCMAKE_TOOLCHAIN_FILE=.../prospero.cmake`,
put matching LLVM tools on PATH, and supply PS5 SDL3_DIR. The SDK project creates
no runnable CTests and SDK binaries must not be executed on host. A fresh OFF
tree without options adds no HD/SDL targets. Assertions remain on in Release.

Real-worker check covers read/release thread ownership, one read for coalesced
requests, independent leases, CLUT invalidation, held pixels through scene
change, state cancellation, missing key, decoded-size fallback and final pin
cleanup. Separate deterministic pipeline boundary check forces BUSY hidden-lease
drain, terminal failure, bounded slot exhaustion, unsupported metadata, empty/
oversized proof, unrelated versus source dirty tiles, P8 palette, cancellation
and final generation refusal. Fault injection replaces pipeline functions only
inside its test executable; production code has no stub path.

Review source: pe_hd_lookup.c/h, lookup.cmake, this document,
tests/check_lookup_real.c, tests/check_lookup_faults.c and
tests/lookup/CMakeLists.txt. outputs/ is evidence only, excluded from public PR.


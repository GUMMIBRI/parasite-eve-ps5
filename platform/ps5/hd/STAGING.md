# HD staging developer validation

`staging.cmake` is a standalone aggregator, default OFF. Include it from a
public SDK project after `project()`, explicitly enable `PE_PS5_HD_STAGING`,
and link `pe-hd-staging` to use lookup, generation, VRAM extraction and the trusted-root pack provider. Dependency order is
image -> cache -> index (filename) -> pipeline -> lookup, with generation required
by lookup, independent VRAM extraction and pack -> index. SDL3 is
found by the cache fragment. Repeated includes are safe. Dependency enablement
is scoped inside a function; it does not force persistent cache options. Root
CMake and game targets never include this file.

This is staging infrastructure, not active replacement support. Hashes remain
opaque. Renderer key production, immutable snapshot ownership, texture-window/
UV mapping, scene generations and renderer threading are still review gates.
No pack scanning or file writes are added to the libraries. The read-only pack
provider requires caller-guaranteed trusted immutable roots; see PACK.md. It
does not enforce a sandbox against symlink/rename/mount escapes.

Use CMake 3.20+, Ninja, a C11 compiler, and a compatible SDL3 installation. Supply
all tool paths; nothing downloads or assumes a developer's C drive. For SDK mode
supply the public ps5-payload-sdk root, LLVM bin and PS5 SDL3 prefix. For host mode
supply a host SDL3 prefix and matching host compiler. SDL's original zlib license
and vendored stb dual MIT/public-domain notice remain in their source trees;
this harness uses independent synthetic fixtures and existing module code only.
No DuckStation implementation or private pack/game asset is used.

```powershell
./platform/ps5/hd/validate-staging.ps1 -Mode Host -CMake $cmake -CTest $ctest -Ninja $ninja -HostCompiler $cc -SDL3Prefix $hostSdlPrefix
./platform/ps5/hd/validate-staging.ps1 -Mode SDK -CMake $cmake -Ninja $ninja -Sdk $sdk -LlvmBin $llvm -SDL3Prefix $ps5SdlPrefix
./platform/ps5/hd/validate-staging.ps1 -Mode Off -CMake $cmake -Ninja $ninja -HostCompiler $cc
```

For Zig pass `-HostCompilerArg1 cc`. Extra toolchain/archiver flags can be passed
via `-ExtraCMakeArgs`; they must match the supplied SDL/compiler ABI. A sanitized
SDL requires matching runtime linker flags (for the local Zig fixture,
`-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined`). Use a separate build directory
for each compiler/target; output defaults to a fresh repository outputs tree.
`-BuildDirectory` may resume that same configuration. No console commands exist.
The script checks prerequisites, restores PATH/cache environment variables and
writes arguments, source hashes and logs beside the build. Do not publish outputs.
A missing prerequisite fails; it never supplies an SDL or SDK stub.

Direct CMake works on other hosts too:

```sh
cmake -S platform/ps5/hd/tests -B build-hd -G Ninja -DPE_PS5_HD_STAGING=ON -DPE_HD_CHECKS=ON -DCMAKE_PREFIX_PATH=/your/host/sdl
cmake --build build-hd
ctest --test-dir build-hd --output-on-failure --no-tests=error
```

Ten assertion-enabled tests cover synthetic decode/budgets, real SDL cache
workers/leases/shutdown, manifest key/path/pin lifetime, pipeline cancellation/
worker callbacks/fallback, VRAM pixel order/CLUT/wrap/STP/bounds, generation
overlap/wrap/rollover, and real-worker plus deterministic-fault lookup checks.
The pack check uses only temporary synthetic fixtures and tests path/byte/pin
limits, cross-thread release and actual pipeline decode/cancel/lease/close.
The coherent lookup check extracts synthetic VRAM, mutates a CLUT tile and
rejects its old proof, then retains a pixel lease through scene cancellation,
coordinator destruction and pipeline close/retry. Fault checks deliberately
substitute pipeline boundary functions; real-worker checks use real SDL.
Index
concurrency uses SDL threads/atomics, not a Windows-only test implementation.
Assertions remain enabled in Release. SDK builds compile/link the same tests,
but register no CTest execution and never launch a target binary. This proves
public payload toolchain compatibility only, not application runtime or console
success. Default OFF configures without SDL and creates no HD targets.

Bounded versioned TSV ingestion is included via pe-hd-manifest; see MANIFEST.md.



# PS5 bootstrap checkpoint — 2026-10-27

Base: `49341628f0756817ab997b3ae8d75e98201cc47b`, branch
`ps5/bootstrap-platform`. Manager owns Git. No console connection or gameplay
test was performed. Human testing is required for every console claim.

## Deliverable and capability boundary

`PE_PS5_BOOTSTRAP=ON` returns from root CMake before the normal project,
PsyZ, SDL, game sources, assets, and submodules are configured. Defaults are
unchanged. Without `PE_PS5_HOST_VALIDATE=ON`, configuration fails explicitly:
the confirmed retail firmware 13.60 + kstuff-lite + ShadowMount+ combination
has no verified public payload runtime contract in this review.

The native host executable contains real stdio logging, a caller-selected
read probe, and an exclusively created scratch-file write/read/compare/remove
probe. It never truncates existing files. Exit 1 means usage or I/O failure;
exit 2 means portable probes passed but console/controller/graphics capabilities
remain unavailable. It never reports full platform success. Flush/readback does
not establish power-loss durability. Do not pass saves or assets as scratch paths.

## Public SDK and samples reviewed

Source-only review checkout (ignored, not a new submodule):
`platform/ps5/sdk-review`, https://github.com/ps5-payload-dev/sdk at
`a7685ce8e97ce1f8a4f38253a41d54241c9124f1`.
Recreate with `git clone https://github.com/ps5-payload-dev/sdk.git platform/ps5/sdk-review`
then `git -C platform/ps5/sdk-review checkout --detach a7685ce8e97ce1f8a4f38253a41d54241c9124f1`.
No SDK binary or runtime dependency was installed or linked. SDK README license:
GPLv3-or-later except FreeBSD headers under their individual BSD licenses;
retain COPYING and per-file notices if distributing SDK material. Game licensing
and eventual combined distribution require separate review.

Reviewed README, host/toolchain/prospero.cmake, host/win compiler wrapper,
samples/hello_cmake, sample inventory, include/ps5, and sce_stubs. The SDK
documents dynamic-linked payloads with elfldr, websrv, bdj-ipv6-hen,
elfloader/ps5-jar-loader, and remote_lua_loader. This list does not verify the
human's particular loader/firmware/process context. ShadowMount+ availability
alone does not identify a compatible ELF entry ABI or stdio route.

The maintained SDK's hello_cmake demonstrates ordinary `main` and `printf`.
No controller or graphics example exists in its reviewed sample inventory.
libScePad/libSceVideoOut symbol stubs exist, but the reviewed public headers
do not establish their structures, ownership, buffer registration, or usable
application context. No function prototypes were guessed. Controller and
graphics clear are explicitly unavailable. A compiler or successful link against
symbol stubs would not resolve these questions.

Public upstream SDL docs (`third_party/psyz/external/SDL/docs/README-ps5.md`)
say the PS5 port lives separately and requires Sony NDA access. That port is
outside this public-SDK-only task. No proprietary SDK, decrypted libraries,
game assets, kernel installation, or network deployment is part of this build.

## Reproduce host validation

With CMake 3.21+, Ninja and a native C11 compiler available:

```sh
cmake -S . -B platform/ps5/build-host -G Ninja -DPE_PS5_BOOTSTRAP=ON -DPE_PS5_HOST_VALIDATE=ON
cmake --build platform/ps5/build-host
# Windows uses pe-ps5-bootstrap-host.exe.
./platform/ps5/build-host/pe-ps5-bootstrap-host ./platform/ps5/bootstrap.c ./platform/ps5/probe-unused.tmp
# Expected exit 2: both file stages passed, console capabilities unavailable.
cmake -S . -B platform/ps5/build-blocked -G Ninja -DPE_PS5_BOOTSTRAP=ON
# Expected runtime-unavailable configuration error.
```

Specify `CMAKE_C_COMPILER` and `CMAKE_MAKE_PROGRAM` explicitly when tools are
not on PATH. Local verification used existing CMake 4.4.4, Ninja and Zig 0.15.2
with a local compiler wrapper and build-local caches. That machine-specific
wrapper is not part of the contribution.

Native compilation and executable linking passed. Real file read and write
roundtrip passed with exit 2. Missing read input returned 1. Existing scratch
file returned 1 and retained its contents. Console configuration returned the
intended runtime-unavailable error before compiler detection. `git diff --check`
passed. No PC game build or console run was performed. The normal default CMake
path has only the new default-OFF option added.

## Upstream and local integration policy

Track `https://github.com/khasinski/parasite-eve-pc` as `upstream` and fetch before
starting each integration batch. At this checkpoint, fork HEAD and upstream/main
both equal the base revision above. Keep PS5 changes on a feature branch; merge
reviewed upstream changes without replacing local PS5 work. PsyZ remains pinned
to the parent project's verified submodule revision, rather than a moving branch.

The earlier local Windows port can supply behavior references and source fixes
with recorded provenance. Compare available original and HD asset sets for visual
quality and renderer compatibility, preserving both until user comparison. Retail
game data, HD packs, saves and proprietary SDK files stay local and out of PRs.
The existing Windows builds and save baselines must remain recoverable.

## Incremental platform audit

| Area | Existing dependency / next proof required |
| --- | --- |
| `sdl3_gl.c` | SDL window, GL context, shader compile, swap; no public PS5 GL context established. |
| `sdl3_gpu.c` | SDL GPU device, shaders, textures, render passes and swapchain; no public PS5 SDL GPU driver established. |
| `sdl3_software.c` | CPU rendering still needs SDL presentation/window support; not an automatic console fallback. |
| `sdl3_audio.c` | SDL audio device stream, callback, mutex and frequency control; requires a real public audio backend. |
| `sdl3_os.c` | SDL monotonic nanoseconds, thread IDs, detached threads, mutexes, semaphores and priority; validate semantics before substituting POSIX calls. |
| Input | SDL event/gamepad mapping in `sdl3_common.h`; public controller structures and application context still required. |
| Settings | `src/main/boot/main.c` uses SDL_GetPrefPath and host preference persistence; console sandbox paths unverified. |
| Disc | Host options contain two disc paths and feed image loading; choose explicit read-only user-supplied mounts only after runtime path proof. |
| Saves | `main.c` stores per-user memory-cards and preserves legacy imports; keep migration logic and old saves untouched. Test a separate scratch root first. |

Next step: obtain public evidence identifying a compatible loader and userland
entry/stdio context for 13.60, then build the pinned SDK's unmodified hello sample
on this host. SDK Windows wrappers need clang plus the SDK sysroot, CRT and
prospero-lld wrapper; Zig alone is not that toolchain. Only after that evidence
should a PS5 adapter replace the deliberate configuration gate. Human records
boot/log and scratch I/O output before any game integration. Graphics, input,
audio, threads and timing each remain separate proof stages.

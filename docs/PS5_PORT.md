# PS5 bootstrap checkpoint — 2026-10-27

## Tooling handoff — 2026-10-29 UTC

Actual public SDK ELF builds now pass on Windows without WSL. This supersedes
the older source-only/toolchain-not-installed statements below. No console run,
deployment, loader validation, graphics, input or audio success is claimed.

Public release checked against GitHub releases/latest: SDK `v0.45` (published
2026-10-21), source checkout `a7685ce8e97ce1f8a4f38253a41d54241c9124f1`.
Release archive SHA-256:
`5fac64093d0e3e69f2584ce7935f24f703f622d9681f1495c85b9f00d1291a2c`.
Official portable LLVM 18.1.8 Windows archive SHA-256, measured locally:
`22c5907db053026cc2a8ff96d21c0f642a90d24d66c23c6d28ee7b1d572b82e8`.
The LLVM hash pins the downloaded official release asset; it is not a publisher
signature. URLs and license details are in `outputs/PS5-TOOLCHAIN.json` and
`platform/ps5/setup-public-sdk.ps1`. LLVM is Apache-2.0 WITH LLVM-exception, with
additional bundled notices. SDK GPLv3+ and individual BSD notices remain relevant.
The pinned source's license is named `LICENSE`, despite header references to
`COPYING`; setup retains it as `LICENSE.SDK` next to unchanged release files.

```powershell
./platform/ps5/setup-public-sdk.ps1
./platform/ps5/build-public-payloads.ps1 `
  -CMake C:/.pe/tools/cmake/cmake-4.4.4-windows-x86_64/bin/cmake.exe `
  -Ninja C:/.pe/tools/ninja/ninja.exe
```

The paths above identify existing local CMake/Ninja executables used in this
verification; on other machines provide equivalent paths or put both on PATH.
Setup downloads/extracts only public archives under ignored `platform/ps5` tools
and downloads directories. It preserves existing installations. It does not run
an installer or change persistent PATH, registry, accounts or credentials.
Builds use the release's unmodified `win/prospero-clang.cmd` and
`win/prospero-lld.exe`, its sysroot, CRT and linker script. LLVM 18 is enforced.
The upstream Windows linker wrapper requires SDK/output paths without spaces;
the build script rejects those paths explicitly. Each build gets a fresh directory
under `outputs/ps5`, with a transcript, ELF header inspection, hashes and an
`UNTESTED ON CONSOLE` marker. Do not publish SDK/tools or build artifacts as part
of the source PR without separate distribution review.

Validated final output: `outputs/ps5/20261029T003225Z-b7d03c58`.
`hello-sdk.elf` comes from the unmodified release sample `samples/hello_cmake/main.c`;
`pe-ps5-io.elf` is the isolated CMake target (`PE_PS5_PAYLOAD=ON`), returning before
PsyZ/SDL/game configuration. Both linked with Clang/LLD 18.1.8 and passed
ELF64, little-endian, x86-64, ET_DYN, nonzero-entry and PT_LOAD inspection.
Hashes and detailed commands are in the output directory and `outputs/RESULT.md`.

The I/O payload requires two loader argv paths: a readable fixture and a new
scratch file in a private writable directory. It uses exclusive creation,
handles partial/interrupted descriptor I/O, reads back the same descriptor,
compares contents, and removes its scratch file. It never writes when fixture
opening/reading fails. Existing scratch files are preserved. Use a directory
without concurrent path replacement. Exit 1 means failure; exit 2 means probes
passed or unavailable argv, with console capabilities still unverified. This is
not a game adapter. Native Windows validation passed roundtrip/removal, existing
file preservation, missing-fixture write prevention and absent-argv cases.

WSL `--list --verbose` confirmed no installed distro. The authorized
`wsl --install -d Ubuntu-24.04 --no-launch` returned success with **changes will
not be effective until the system is rebooted**. No reboot was performed.
After the owner chooses to reboot, complete Ubuntu 24.04 installation/first-run
account setup as needed, then run:

```sh
bash platform/ps5/setup-public-sdk.sh --install-dependencies
bash platform/ps5/build-public-payloads.sh
```

Linux dependency install/build remains untested here. Shell scripts now have
enforced LF checkout, correct archive root and LICENSE handling, staged extraction,
LLVM 18 checks and unique output directories. WSL is no longer a blocker to the
actual Windows ELF build milestone. Human console validation remains outstanding.

Read-only upstream fetch found `upstream/main` at `da1f775e6` (0.7-alpha release).
No checkout, reset, commit, integration or submodule pin change was performed;
main owns integration. Qwen playlist and dispatch remain main's exclusive queue.

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

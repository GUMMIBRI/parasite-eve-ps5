# Experimental public application ABI

The default build-game.ps1 output uses the public SDK payload ABI. A payload
loader provides arguments/callbacks expected by its CRT; an application loader
does not. Renaming that ELF or wrapping it in SELF does not adapt the ABI.

`-ApplicationTooling <directory>` opts into a separate source rebuild using the
public application CRT/linker layout instead. This remains experimental and has
not been validated on a console. It does not install an application or modify a
delivery folder. Use a fresh build; keep payload and application outputs separate.

## Public prerequisites

- Installed ps5-payload-dev SDK v0.45, LLVM18, CMake/Ninja and the static PS5 SDL3
  package matching the source/build directories passed to build-game.ps1.
- Public application source from PSXS5 pin
  `5c17ea31aec5791c12451e915386fa98d670adb4`, directory `tooling/native`:
  app_crt.cpp, ps5-pie.ld and app-symbols.map. Retain upstream GPL notices/license.
- The host ps5-native-tool built from those pinned public sources, with host
  zlib (not PS5 target zlib). See build-public-native-tool.ps1 and
  reproduce-public-runtime.ps1. No proprietary SDK/runtime is required by this
  selected route. Read the upstream runtime shim limitations before use.

Sources: [public SDK](https://github.com/ps5-payload-dev/sdk),
[pinned native tooling](https://github.com/SynoPiia/PSXS5/tree/5c17ea31aec5791c12451e915386fa98d670adb4/tooling/native),
[runtime scope](https://github.com/SynoPiia/PSXS5/blob/5c17ea31aec5791c12451e915386fa98d670adb4/docs/boilerplate/RUNTIME_SHIM.md).

## Build and inspect

Application builds now verify `application-inputs.json` before compilation.
It pins public CRT/linker/license bytes and the reviewed installed v0.45 SDK
runtime archives, import stubs and selected ABI headers. Host converter/runtime
builders additionally verify the full pinned native tooling source set.
Changed inputs fail closed: review a deliberate public update and its ABI/import
audit before changing the manifest. These are installed-build fingerprints;
they do not assert that every independent SDK rebuild has identical bytes, or
that unchecked headers/tools are certified. Source line endings affect hashes.
Application resume is rejected to avoid mixing generated CRT/backend inputs.
Ordinary payload builds do not run this application preflight.

For an upstream checkout, pass its `tooling/native` directory; LICENSE is read
from its repository root. An isolated native copy must retain LICENSE alongside
the copied source. Validate without compiling:

```powershell
./platform/ps5/validate-application-inputs.ps1 -NativeSource <native-source> `
  -Sdk <sdk> -FullTooling
```

Run from repository root, substituting actual local public tool/source paths.
Use the new output directory printed by build-game.ps1 as `<game-output>`.

```powershell
./platform/ps5/build-game.ps1 -ApplicationTooling <native-source> `
  -SDL3Prefix <reviewed-sdl-install> -SDL3Source <matching-sdl-source> `
  -SDL3Build <matching-sdl-build> `
  -PsyZRevision daf1adc38a2a346462964e281dfa32fd74f6888e -StartupLog

./platform/ps5/audit-application-imports.ps1 `
  -Elf <game-output>/b/parasite-eve-ps5.elf -StubDirectory <sdk>/target/lib `
  -ReadElf <llvm-bin>/llvm-readelf.exe -Output <fresh-output>/imports.json
./platform/ps5/audit-application-runtime.ps1 `
  -Elf <game-output>/b/parasite-eve-ps5.elf -ImportAudit <fresh-output>/imports.json `
  -LlvmBin <llvm-bin> -Output <fresh-output>/runtime.json

& <native-tool> link --in <game-output>/b/parasite-eve-ps5.elf `
  --out <fresh-output>/eboot.elf --stub-dir <sdk>/target/lib --file-name eboot.elf
& <native-tool> self --sign --in <fresh-output>/eboot.elf --out <fresh-output>/eboot.bin
& <native-tool> self --inspect --file <fresh-output>/eboot.bin
./platform/ps5/inspect-executable.ps1 -Path <fresh-output>/eboot.elf -RequireApplicationElf
```

The snippets are command templates; create a fresh output directory first and
check each native command's exit code. Keep logs and INPUTS/APPLICATION-ABI JSON.
Structural/import audits and public FSELF integrity do not establish launchability.

## Application-only adaptations

The generator replaces payload crt1.o/kernel_web linkage with app_crt and
application libkernel imports. SDK libc.a supplies real missing POSIX adapters;
libc_stub_weak contributes declarations only. Audit the resulting imports rather
than assuming every public stub implements a runtime API on the target firmware.

Dynamic plugins/runtime replacement and child processes use SDL's existing
unsupported backends. Application dlfcn reports ENOSYS; it never fabricates a
successful handle. A private licensed copy of the SDL unix filesystem source
uses the mounted `/app0/eboot.bin` executable path. This removes /proc probing
through the SDK inline readlink syscall wrapper. Child-process backend replacement
removes inline vfork/setsid wrappers. Payload builds retain their existing policy.

`-StartupLog` instruments a private CRT copy. Early raw libkernel markers precede
libc/TLS initialization; later wrappers log actual SDL/game results. Log paths are
`/data/parasite-eve/startup.log` and fallback `/data/parasite-eve-startup.log`.
Failure before entry or inaccessible log paths can leave no evidence. A last
flushed marker narrows the stage; it does not prove a particular call crashed.

## Runtime and delivery limits

The public companion is a loader shim, not a full libc. Keep stdio/allocation
imports on SceLibcInternal and pthread/errno on libkernel. The static runtime
audit checks providers, TLS shape, payload dependencies and inline syscalls;
system FILE layout, heap initialization, pthread cleanup, loader teardown,
permissions and target firmware semantics remain unproven until owner testing.

Packaging is a separate explicit milestone. A future application root needs
eboot.bin, sce_module/libc.prx, sce_sys/param.json and a512x512 icon0.png. Signed
public companion must match upstream SHA256
`e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036`,
1,284,674 bytes. Raw emitted ELF is not a deployable PRX. Choose unique homebrew
identity, then use validate-application-metadata.ps1; it is template consistency
checking, not Sony certification. Do not send eboot.bin to a payload server.
Application assets resolve under /app0; preserve the existing /data/parasite-eve
config/discs/cards/save policy. Do not publish retail assets or local SDK/binaries
with source changes. No package/install/deployment follows from these commands.

Optional -AllocationLog requires both -ApplicationTooling and -StartupLog and a fresh application output. It observes linked malloc/calloc/realloc failures and requests of at least 256KiB without replacing the allocator, changing returns/errno, or retrying. Logging is bounded to 128 attempted messages and can omit concurrent events. It does not measure total heap usage, shared-library internal allocations or establish a firmware heap limit. INPUTS.json records the mode and selected diagnostic source hashes; STARTUP-LOG.json also records AllocationLog.


# PS5 compile-time debug information

`build-game.ps1 -DebugSymbols` explicitly selects CMake `RelWithDebInfo`.
Without the switch it still selects `Release`. The standalone application CRT
compile also receives `-g` only when enabled; its existing `-O2` is retained.
Startup logging and allocation logging remain separate options. Existing
OutputRoot selection and startup linker wrapping are preserved.

Example from the repository root (substitute your retained candidate source):

```powershell
./platform/ps5/build-game.ps1 -DebugSymbols -StartupLog -AllocationLog `
  -ApplicationTooling outputs/ps5/application-abi-20261029/source `
  -SourceRoot C:/.pe/ps5-native/builds/upstream-20261029T174640Z-c665049a650240c190698b3c997c39b4/src `
  -PsyZRevision daf1adc38a2a346462964e281dfa32fd74f6888e `
  -SDL3Prefix C:/.pe/tools/pe-ps5-sdl3/20261029T010754Z-68fad8/install-reviewfix `
  -OutputRoot C:/.pe/ps5-native/debug-symbols
```

Use a fresh output. Application resume remains forbidden. Payload resume must
retain the original configuration; older provenance without BuildConfiguration
is treated as Release. A mode mismatch fails before build mutation.

The script requires `.debug_info`, `.debug_abbrev`, and `.debug_line` in the
linked ELF and retains `parasite-eve-ps5-unstripped.elf` in the build directory.
`INPUTS.json` records DebugSymbols and BuildConfiguration alongside existing
source/input hashes. `RESULT.json` records configuration, linked ELF SHA256 and
UnstrippedELF. `DEBUG-SYMBOLS.json` records the retained ELF hash and source paths;
`DWARF-SECTIONS.txt` records the explicit llvm-readelf check. Preserve the entire
build tree, candidate source tree and source manifests with that ELF.

For inspection, obtain addresses from that same ELF's symbol table:

```powershell
$llvm = './platform/ps5/tools/llvm-18.1.8/bin'
$elf = '<build-output>/parasite-eve-ps5-unstripped.elf'
& "$llvm/llvm-readelf.exe" --sections --wide $elf
& "$llvm/llvm-nm.exe" --defined-only $elf
# Replace these arguments with known symbol addresses printed above:
& "$llvm/llvm-addr2line.exe" -e $elf -a -f -C <_start-address> <main-address>
```

Acceptance build on 2026-10-29:
`C:/.pe/ps5-native/debug-symbols/game-20261029T180357Z-0beda208475943e88022944a30e93fbd`.
LLVM 18.1.8 / public SDK v0.45 compiled and linked all 404 build steps with the
reviewfixed SDL archive. Generated game and PsyZ commands contain `-O2 -g
-DNDEBUG`; the SDK compiler wrapper forwards those arguments without `-g0`.
The application linker uses `--gc-sections`, without stripping debug sections.
Readelf confirms the required DWARF sections. This ELF is 7,781,592 bytes, SHA256
`DE804897BACA7DE4DFD433ABE6DAEEB98C744C9B7B24A3672C9AE74A0734E65B`.
Its `_start` address `0xe5500` resolves to the private generated
`native-debug/app_crt.cpp:69`; its `main` address `0xe6660` resolves to the retained
candidate `src/main/boot/main.c:2000`. Full paths and evidence are in the private
`C:/.pe/ps5-native/debug-symbols/RESULT.md`.

This is optimized debug information: inlining and optimization can affect line
attribution and variable availability. Prebuilt SDL/SDK libraries are not rebuilt
with debug info, so source lines are not promised inside those archives.
No console/runtime validation, conversion, package creation or Current replacement
is performed. The Release default was verified from source and an existing Release
cache without rebuilding it. Source lines above apply only to the matching new
ELF. Do not apply old crash addresses, assume an ASLR base, or treat a backtrace
value as an executable RVA. Runtime address mapping requires independent module
mapping evidence and the exact matching executable/build provenance.


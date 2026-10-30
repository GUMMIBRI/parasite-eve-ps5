# Original 0.8 public SDK source recipe

Run from the public checkout with locally available original Git objects and the
public SDK, LLVM, CMake, Ninja, application tooling and matching fixed SDL inputs.
Preparation reads Git and writes an isolated export; it does not change the index,
checkout, submodule pins, Current, packages or the console.

```powershell
$evidence = 'C:\.pe\ps5-native\upstream-08-reproducible'
./platform/ps5/prepare-upstream-source.ps1 `
  -Revision 8c52cdde5d60ee52f00913517ffb593e9521bf2d `
  -PsyZRevision db796262b2a696241ec8f3965ebbfbcbc45bfc30 `
  -AcceptVersionResolution -OutputRoot $evidence
# Select the newly printed candidate, whose CANDIDATE.json must be Ready=true.
./platform/ps5/build-game.ps1 -SourceRoot '<candidate>/src' `
  -PsyZRevision db796262b2a696241ec8f3965ebbfbcbc45bfc30 `
  -SDL3Prefix 'C:\.pe\tools\pe-ps5-sdl3\20261029T010754Z-68fad8\install-eventfix' `
  -OutputRoot "$evidence/build" -DebugSymbols -StartupLog -AllocationLog `
  -ApplicationTooling '<public application tooling source>'
```

`integrate-upstream08.ps1` and `upstream08-recipe.json` are the adaptation inputs.
They require the exact original revision and unique checked anchors. Three main.c
conflicts combine PS5 paths/card/startup guards with the upstream save declaration,
Unicode-aware preferences and HD CLI dispatch. CMake retains version 0.8.0. The mods
conflict selects the original upstream list; native PE_MODS remains OFF. No root
CMake/main file is replaced from a prepared candidate.

The startup conflict accepts the two exact reviewed forms, before and after the
PS5 widescreen normalization already present on public fork PR16 at
`a2b1efe4a63ad2c1d9a6d162df43b2ac904c1fa9`. Both retain that normalization in
the prepared source. Recipe schema 1 and unique anchors are required; other
conflicts still require review. Publish this recipe together with the compatible
`integrate-plugins.ps1` and `PLUGINS.md`; the older public adapter does not accept
the selected revision argument or handle the recipe correctly on PowerShell 5.1.

This selects pinned original 0.8 source while retaining fork history. It performs
no merge, fetch, checkout, history synchronization or publication by itself.

Desktop `PE_DESKTOP_MOD_DOWNLOADS` defaults ON and keeps the upstream implementation.
The native default is OFF; explicitly requesting ON for PS5 fails configuration.
OFF excludes curl/cJSON/miniz and provides an unsupported service with failing CLI,
unavailable UI, no thread, no pack I/O and no simulated download success.

The selected PsyZ project itself declares libwebp v1.6.0 with SHA256
`93a852c2b3efafee3723efd4636de855b46f9fe1efddd607e1f42f60fc8f2136`.
Its public FetchContent download is the only new software dependency. Retain its
archive, source, BSD COPYING and PATENTS; PNG uses existing SDL_LoadPNG. No private
CMake override or prepared 0.8 tree is needed. The installed SDL archive must be
the fixed event-pump build, SHA256
`6E0A9A10C51A3813D56DA21FB7BFE3132C54047F666EBC673D35B6DE7ADA71FF`.
Public PR16 itself predates the SDL event-pump fix: retain the accompanying
`platform/ps5/sdl3/video/SDL_ps5video.c`, the same callback fix as merged public PR15 at
`f46ad6f6dd382dd1a3c300f53349f147984f5118`, when rebuilding that SDL archive.
Always regenerate PsyZ with the active patch-game.ps1 before building: earlier
0.8 objects predate the display-mask correction.

Plugins default OFF. To prepare optional plugins, add `-IncludePS5Plugins`; enable
them in the builder separately with `-Plugins`. The established b1 path remains
available with revision `b1c225aec3065b932b1a35229502c7666c38685c`, PsyZ
`daf1adc38a2a346462964e281dfa32fd74f6888e`, `-AcceptVersionResolution` and
`-IncludePS5Plugins`. Without that existing mods resolution, b1 continues to report
the mods CMake conflict for deliberate review.

The adaptation uses Windows PowerShell 5.1 syntax and explicit UTF-8 recipe reads
and generated source writes. Build evidence records explicit Revision/PsyZRevision,
recipe hashes, all candidate source hashes, SDL provenance, DWARF and schema-2
RESULT/INPUTS binding. Import/runtime audits are static checks; compilation does
not establish console gameplay or video success. Packaging and deployment are
separate manager-owned actions.

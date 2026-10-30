# Opt-in built-in PS5 playtest panel

`prepare-upstream-source.ps1 -IncludePS5Plugins` applies the checked source
recipe in `plugin-source.json` to the exported game. It preserves the selected
upstream mods list/build glue when resolving that file's known merge conflict;
the PS5 CMake option forces the static registry to `pe_assists` only when ON.
Unexpected source anchors fail instead of overwriting upstream changes.

Pass that export's `src` as `build-game.ps1 -SourceRoot ... -Plugins`.
The builder checks that the game integration is present and patches the freshly
exported, software-renderer-patched PsyZ stage with the same recipe. Its original
checkout/submodule stays untouched. `-Plugins` defaults absent/OFF; the builder
always passes explicit `PE_PS5_PLUGINS=OFF` or `ON`, and rejects resume across
plugin modes. Exporting the source does not enable the plugin by itself.
Use the existing `-DebugSymbols -StartupLog -AllocationLog -ApplicationTooling`
arguments when building the diagnostic application. There is no package step here.

The recipe is the exact game/PsyZ source delta from the reviewed isolated
candidate `C:/.pe/ps5-native/plugin-integration/candidate-20261029`, whose
REPORT.md, BASE-CANDIDATE.json and SOURCE-MANIFEST.json retain provenance.
Its baseline is the startup-paths export of upstream
`b1c225aec3065b932b1a35229502c7666c38685c`, PsyZ
`daf1adc38a2a346462964e281dfa32fd74f6888e`; the current card diagnostics are
preserved independently in main.c, outside the recipe. The new source is
Apache-2.0 under existing project notices; existing component licenses remain.
No absolute local candidate path is required by the recipe at build time.

L1+R1+physical touchpad click opens/closes the CPU panel. Existing All mods OFF,
per-mod restart, assist defaults OFF and loaded-game guards remain. Settings
follow the validated absolute game preferences directory. This is a static
built-in panel, not a native folder loader. Compilation/static audits establish
neither console compatibility nor playability. The owner performs runtime work.


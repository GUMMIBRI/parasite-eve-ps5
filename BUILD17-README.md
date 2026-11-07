# Public Build17 source reconstruction

This recipe selects the already-built **Build17 test candidate**, content01.000.017 / PPSA77022: stable Original0/Plex1/Retro2 plus Atkinson3, source-owned4x title-label presentation, and retained Build16 Off/Scanlines/CRT/defaultOff plus bounded atlas-index optimization. **External label PNG admission is disabled unconditionally.** No AI artwork, finger replacement, all-menu/chapter-card remaster, runtime/FPS qualification or firmware-crash fix is claimed.

Using the three archives and glslang pinned in the retained manifests:

```powershell
node reconstruct-build17.cjs <pinned-archive-directory> <fresh-absent-output-outside-export> <pinned-glslang-executable>
```

The wrapper verifies the complete export inventory/checksums and requires a fresh external root. It then performs this complete chain:

```powershell
node verify-export.cjs
node prepare-source.cjs <pinned-archive-directory> <fresh-absent-output>
node apply-optional-choice.cjs <prepared-output>
node build14-upgrade/apply-build14.cjs <prepared-output>
node apply-build15.cjs <prepared-output>
node apply-build16.cjs <prepared-output>
node apply-build17.cjs <prepared-output>
node generate-all-shaders.cjs <prepared-output> <pinned-glslang-executable> --choice
node verify-source.cjs <prepared-output>
```

Build12/13 preparation, exact historical overlays, dependency/source licenses and three installer removals remain retained from the fixed parent chain. The final Build17 upgrade validates **16 pre/postimages**:14 fixed combined menu/font postimages plus the two Build17 build-identity files. The new font include and its unchanged OFL license are included as source; `ATKINSON-PROVENANCE.json` pins the official Google Fonts upstream. No network download is performed during reconstruction.

The retained shader stage regenerates **8 selected headers /13 SPIR-V modules**, with exact pinned source/compiler/output checks. Build17 adds no new shader. Final caller-free verification covers **355 selected source/mirror/support/shader/license records**, the retained removals, empty public pack index and absence of generated owner inputs. The4x title companion requires an exact native320×240 source-frame witness and keeps the original display route on rejection; appearance/runtime coverage is not established by compilation or reconstruction.

For a later separately authorized SDK build:

```powershell
node build-native.cjs <prepared-output> <retained-public-prefix> <tools-config.json>
```

The build validates final source before owner-input generation; selected support is instantiated without copying baseline templates over the selection. It rebuilds native CRT/helpers, SDL, PsyZ/game and shaders from retained source/pins. See `tools-config.example.json`, dependency/input manifests and notices. A caller's authorized **216-byte PointSprite input stays outside this export**, per the established approved compiled-table policy; no private input hash/initializer, private title/cohort identities, seed, source artwork, reference images, original game files or pack index is distributed. No new runtime file dependency is introduced.

Source reconstruction proves correspondence to the selected authored source, not byte-reproducible executables, store installation, all menus, performance or firmware compatibility. This source-integration work does not execute another binary SDK build or change prior test ZIPs. Earlier Build16/15/14/13 entry points/docs are intermediate-stage history only; use `reconstruct-build17.cjs` for final Build17. Manager owns future package/publication decisions; the human is the gameplay playtester.

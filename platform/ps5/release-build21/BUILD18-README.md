# Parasitic Event Revisited — Build18 source reconstruction

This recipe selects **Build18 test source**, display title **Parasitic Event Revisited**, content01.000.018 / **PPSA77022**. Branding affects the PS5 title/native heading and successor documentation only: original configuration/data/card paths, SDL preference namespace, contentID, original disc identity, P5 icon and attribution/history remain unchanged.

Build18 retains Atkinson3/stable Original0/Plex1/Retro2, native4x source-owned title text, Off/Scanlines/CRT defaultOff and the2KiB atlas lookup index. It adds app-contained automatic data-default priority/error clarity and the selected **bounded uncompressed `.pelabel` admission**. The label root remains unset by default and **no artwork is included or activated**. Runtime PNG decoding stays disabled for the new label path. No all-menu, finger, DAY-card, material, FPS, firmware or crash-fix claim is made.

Using the retained three archive pins and pinned glslang:

```powershell
node reconstruct-build18.cjs <pinned-archive-directory> <fresh-absent-output-outside-export> <pinned-glslang-executable>
```

The wrapper verifies export inventory/checksums, requires an absent external root, then runs:

```powershell
node verify-export.cjs
node prepare-source.cjs <pinned-archive-directory> <fresh-absent-output>
node apply-optional-choice.cjs <prepared-output>
node build14-upgrade/apply-build14.cjs <prepared-output>
node apply-build15.cjs <prepared-output>
node apply-build16.cjs <prepared-output>
node apply-build17.cjs <prepared-output>
node apply-build18.cjs <prepared-output>
node generate-all-shaders.cjs <prepared-output> <pinned-glslang-executable> --choice
node verify-source.cjs <prepared-output>
```

The chain retains fixed baseline support, Build12/13 preparation and all exact previous overlays/three installer removals. The final **9 pre/postimage upgrade** combines the newer Build17 source with only the app-priority error hunks (not a replacement older main), branding/build identity and the selected2-file raw loader delta. Source verification checks **355 selected source/mirror/support/shader/license records**. No new GPU shader was added: retained generation reproduces **8 headers /13 SPIR-V modules** through pinned source/compiler/output hashes.

## Data and optional art policy

App-contained readable/nonempty complete disc pairs supply automatic defaults before external roots. App data access errors do not silently select another copy. Missing/incomplete app data can use the candidate's unique readable external pair. Explicit saved/imported/command-line paths remain authoritative; cards/preferences/logs remain APP/user. This changes defaults/diagnostics, not files, permissions, migration or mounted-disc validation. Discovery is startup-cached, not a hotplug claim.

Optional label data uses five fixed dimensions/IDs, a32-byte versioned header, exact uncompressed length, successful full read/EOF/close and fixed allocation/cache budgets. No approved label file is supplied. See `tools/title-labels/FORMAT.md` and `CONVERTER.md`. `convert-label.py` operates offline only on explicitly approved owner-authored/generated PNGs using the caller's existing Pillow; it installs/downloads nothing and is not a runtime PNG decoder. Its source/document provenance and retained project terms are included. No art was converted during this selection, and no private reference image/fingerprint is included.

## Later build, when authorized

```powershell
node build-native.cjs <prepared-output> <retained-public-prefix> <tools-config.json>
```

The build validates final selected source before generating caller input, instantiates selected support without restoring baseline templates, and rebuilds CRT/helpers/SDL/PsyZ/game/shaders from retained pins. The established authorized **216-byte PointSprite input must remain outside this export**, per `REQUIRED-OWNER-INPUT.md` and the approved compiled-table policy; no private input hash/initializer, title/cohort identity, test seed, original game files, reference images or pack index is distributed. No new runtime file dependency is added for the caller table.

Fresh source correspondence is separate from executable reproducibility, runtime/game/visual/store qualification or publication. Earlier Build17/16/15/14/13 commands are intermediate history; use the complete Build18 wrapper. Manager owns further integration/console/release decisions, and the human remains the gameplay playtester.

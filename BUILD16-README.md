# Public Build16 source reconstruction

This recipe selects the already-built **Build16 test candidate**, content01.000.016 / PPSA77022: default-Off Scanlines/CRT settings and bounded exact-key HD atlas lookup optimization, retaining Build15 gameplay/data policy. It is source preparation, **not a new release, startup-crash fix, performance measurement or human qualification**. No game discs, optional art/texture packs, private identity/cohort, test seed, caller input or generated caller initializer is distributed.

From this export, using the three archive files and glslang revision pinned by `manifest.json`:

```powershell
node reconstruct-build16.cjs <pinned-archive-directory> <fresh-absent-output-outside-export> <pinned-glslang-executable>
```

The wrapper verifies this export's complete file inventory/checksums, requires an absent external output and runs the complete sequence below. It performs no network requests, Git operations, SDK compilation, title execution or console access.

```powershell
node verify-export.cjs
node prepare-source.cjs <pinned-archive-directory> <fresh-absent-output>
node apply-optional-choice.cjs <prepared-output>
node build14-upgrade/apply-build14.cjs <prepared-output>
node apply-build15.cjs <prepared-output>
node apply-build16.cjs <prepared-output>
node generate-all-shaders.cjs <prepared-output> <pinned-glslang-executable> --choice
node verify-source.cjs <prepared-output>
```

Preparation reuses the fixed Build15 recipe's exact archive/patch pins and baseline support, Build12/13 changes, choice adapter, Build14/15 upgrades and three removed disc-installer files. The **Build16 upgrade validates 11 pre/postimages**, including additions, before copying any new source. Its combined generated filter header is deliberately not copied: the adjacent MIT GLSL vertex/fragment sources compile through the pinned tool and exact SPIR-V/header hashes must match.

The complete shader stage regenerates **8 selected headers containing 13 SPIR-V modules**: the retained 7 headers/11 modules plus the 2 filter modules in one new header. This uses standalone filter shader creation; no game numeric shader-role registration is invented. Final caller-free verification covers **350 selected source/mirror/support/shader records**, the old installer removals, public empty pack index and absence of generated owner inputs. Source verification is a source-selection proof, not byte-reproducible executables or visual/firmware qualification.

For a future, separately authorized SDK build after reconstruction:

```powershell
node build-native.cjs <prepared-output> <retained-public-prefix> <tools-config.json>
```

Keep the retained dependency/compiler pins, licensed notices and input-integrity checks. The build first validates final selected source before generating caller input, checks the exact selected support and instantiates placeholders in place; baseline templates are never copied over the final selection. Native CRT/helpers, SDL, shaders, PsyZ and game are rebuilt by this recipe. This export-integration task did **not** invoke that SDK build again or change any existing ZIP/eboot.

`tools-config.example.json`, `REQUIRED-OWNER-INPUT.md`, `manifest.json`, dependency input manifests and notices retain the established boundaries: a caller may supply its authorized **216-byte PointSprite input outside this export** for local validation/generation, under the prior approved compiled-table policy. No private input hash or initializer is included here, and no new runtime file dependency is added. Settings/normal memory cards, final P5 icon, app identity and optional-data layouts remain preserved. Default display filter is Off; no measured FPS gain is claimed.

Earlier BUILD15/14/13 documents describe historical intermediate stages only; use this document or the wrapper to select final Build16. A later public binary release still requires manager selection/review and its own authorization. The unrelated CE-108255-1 startup report remains an investigation, not a claimed fix from this recipe.

# Public Build15 reconstruction

Run from this export with retained archives named and pinned in manifest.json, an absent output directory outside the export, and the pinned glslang executable. No network or Git action is required.

```powershell
node verify-export.cjs
node prepare-source.cjs <archive-directory> <fresh-output>
node apply-optional-choice.cjs <fresh-output>
node build14-upgrade/apply-build14.cjs <fresh-output>
node apply-build15.cjs <fresh-output>
node generate-all-shaders.cjs <fresh-output> <pinned-glslang-executable> --choice
node verify-source.cjs <fresh-output>
```

Preparation checks three archive pins, patches, 213 outputs, 40 mirrors, baseline auxiliary files and Build12/13 upgrades. Only preparation copies baseline-support. The choice adapter is required for this Build15 selection. Build14/15 retain exact overlays and preimage checks; Build15 checks and removes three obsolete installer files. Seven shader headers are regenerated. Final verification checks 346 selected source/mirror/support/shader records, removals, empty pack and omitted caller inputs.

For a separately authorized SDK build after these commands:

```powershell
node build-native.cjs <fresh-output> <retained-public-prefix> <tools-config.json>
```

The build validates final selected source before generating owner input or invoking SDK tools. It validates selected support and instantiates placeholders in place without copying templates over the selection. support/LINK-ARGS.template.rsp is the retained pinned link template. Source CRT is rebuilt from retained licensed source. This recipe repair was verified without an SDK rebuild; the existing binary is unchanged.

Build15 selects PPSA77022 and app-contained or unique external complete data roots. No assets, backgrounds, private pack, seed, private title initializer or cohort are exported. The approved compiled 216-byte caller table policy remains: generator input and generated initializer stay outside this export, with no new runtime file dependency. See REQUIRED-OWNER-INPUT.md, dependency pins and notices.

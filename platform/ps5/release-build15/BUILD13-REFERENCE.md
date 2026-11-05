# Build13 source reconstruction

Source-only preparation for the delivered Build13 selection. No release version is assigned. Based heavily on khasinski/parasite-eve-pc:main. This is correspondence/provenance evidence, not gameplay qualification or independent license clearance.

The baseline is the public `platform/ps5/release-build12` recipe at commit `367870a986c75726829f44c857afa9711f2141f8`. Its pinned game, PsyZ and SDL archives, public patches, four-file Build12 upgrade, notices and required caller-input boundary are retained. `.gitattributes` preserves recipe and digest bytes (`* -text`). No Git operation is required by these scripts.

With the three already obtained archives named as in `manifest.json`, run:

```
node prepare-source.cjs <archive-directory> <fresh-output-outside-this-export>
node generate-all-shaders.cjs <prepared-output> <pinned-glslang-executable>
node verify-source.cjs <prepared-output>
```

Preparation checks archive pins, 213 baseline patched source outputs, 40 SDL mirrors, auxiliary support/state digests, the four Build12 upgrade files, then every preimage and postimage of the 18-file Build13 upgrade. `build13-upgrade/manifest.json` records these changes; `source.patch` is an equivalent exact source overlay. The operational upgrade script uses digest-pinned replacement source files. Added source has a null preimage. Seven PS5 SPIR-V headers are regenerated from GLSL and checked against delivered source header digests; no new generated shader arrays are carried in this upgrade. DXIL/MSL are outside this PS5 verification.

Build13 removes the Save states tab and its registered actions/work in both native roles. The menu retains Graphics, Mods, Cheats, Controls and System; ordinary memory cards remain separate and enabled. Selected source includes directional normal/bone receivers, PM4/5 shooter and impact context, P109 fire/intensity and exact M005/P117 beam endpoint admission, plus the selected title identity and bounded admission diagnostics. This does not establish universal attack coverage, DAY artwork identity, appearance or performance. DAY producer identity remains unknown. The choice adapter below is not part of this selection.

The recipe selects `PE_NATIVE_PUBLIC_RELEASE=ON` / definition `1`, `PE_NATIVE_STATE_CHANGED_COHORTS=ON`, and `GAME_TEST`. Delivered compiler definitions were inspected for those public/native selections and `PE_BUILD_ID="Build13"`. The inherited CMake project version remains `0.1.2`; that is not a newly assigned Build13 release version. Portable build templates replace machine paths and rebuild the CRT from public source. The public testkit implementation is the delivered unconfigured stub: private seeding is forbidden. Caller-free preparation generates an unavailable empty native pack sentinel, not an owner index.

For a later authorized build, use `build-native.cjs <prepared-output> <retained-public-prefix> <tools-config.json>`. Actual dependencies/toolchains and the authorized 216-byte PointSprite input with caller-pinned hash must be supplied outside this export. Missing caller input fails before toolchain work. See `REQUIRED-OWNER-INPUT.md`, dependency pins and all retained notices. Both burst/state consumers retain the shared required-input macro boundary. No owner initializer, original table, private seed, pack, asset fingerprint, private artwork, local source cohort/preflight, build receipt, executable or SDK is supplied. Prepared/build outputs must remain outside Git and outside this export. Do not publish caller-generated input or cohort records.

## Optional choice pointer source

This is a separate, unintegrated source upgrade, not released artwork. Its parent is the exact Build13 source reconstructed above. `optional-choice/manifest.json` pins the parent upgrade manifest and eight source preimages/postimages; `optional-choice/source.patch` includes only source. It carries the generic producer marker, classifier and selector10 handling, with original fallback. It carries no PNG, index, source-hash record or preview.

Use a second fresh prepared output, then:

```
node apply-optional-choice.cjs <second-Build13-output>
node generate-all-shaders.cjs <second-Build13-output> <pinned-glslang-executable> --choice
node verify-source.cjs <second-Build13-output> --choice
```

The later build recipe detects that explicit optional selection and validates its source postimages before generating the matching shader. Do not apply it to or edit frozen Build13. The pointer remains the original when no caller-bound permitted artwork matches. The adapter preserves producer position, hotspot, choice behavior and original packets; human appearance testing remains necessary.

Manager integration is separate: review and select source first. If privately testing permitted art, merge its caller-generated record into a NEW copy of the selected owner index, preserve all existing 171 records byte-for-byte, reject filename/category conflicts, then verify 172 records and all referenced files before selection. Never replace the owner index with the one-entry candidate. Keep this merge, PNG and metadata outside public Git. Frozen Build13, Build12 and published bundles remain immutable.

The generic curated UI metadata tool requires an explicit caller-owned `PE_PRIVATE_INPUT_ROOT` outside this export; all its inputs/output must stay beneath that root. Its example contains no mappings and deliberately fails closed. The choice-specific private index is not generated by the public tool or included here.

Source reconstruction and shader checks passed for both selections without a full SDK build, conversion, ZIP, runtime, console action or publication. `SOURCE-VERIFICATION.json` describes the bounded checks. Manager owns review, Git, integration and release decisions.

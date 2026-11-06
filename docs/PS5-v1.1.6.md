# Parasitic Event Revisited v1.1.6 (Build20)

## Setup first

Install the single PPSA77022 app folder. Put your own compatible North American images together at:

```
PPSA77022/parasite-eve-data/discs/disc1.bin
PPSA77022/parasite-eve-data/discs/disc2.bin
```

The download contains no game discs or private HD packs. Recomp users must use the data folder **inside the app**, not a second sibling folder beside it. If missing, create parasite-eve-data/discs inside the installed folder containing eboot.bin. Read the root readme.txt before starting.

A complete app-contained pair now wins automatic defaults. Existing explicit saved/imported paths still take priority; correct those if you moved their files. External layouts remain supported. The new read-only setup screen explains ordinary missing/incomplete default data before mounting; System > Game files also opens the guide. Access errors, invalid explicit paths and native startup crashes are distinct failures, not all covered by this screen.

## Saves and updates

Back up the entire installed app/user folder and any personal app files before updating. Preserve discs, mods and use-existing-config.txt. Active cards stay in that app's user/memory-cards/bu00 and bu10. Keep your old app/user folder until your saves are verified in the intended installation.

PS5GameTest / PEVG00022 cards do not automatically migrate to PPSA77022. USB /saves is manual import staging only. Individual compatible native-PC saves are8192-byte BASLUS files; whole .mcr/.mcd images are not interchangeable. No automatic import, overwrite, file move or deletion is performed by the release.

## Changes since v1.1.5

- New display name **Parasitic Event Revisited**; stable PPSA77022 identity and save/config namespaces.
- Clearer app-contained data defaults and distinct missing/incomplete/unreadable diagnostics; short lowercase root readme.txt and reachable missing-data setup guide.
- Off / Scanlines / CRT presentation filters, default Off.
- Exact-key HD texture-atlas lookup index (+2KiB static); no measured frame-rate claim.
- Original / Plex / Retro / Atkinson font options with stable IDs; revised Retro/Atkinson crop and spacing alignment. Original/Plex behavior retained.
- Source-owned4x title-label presentation with original frame/layout/timing validation; original background and selector remain.
- Bounded optional raw label-asset hook; disabled by default with no approved replacement artwork included. No runtime PNG label decoder.

## Qualification and known limits

SDK compilation/link, application conversion/import/layout/SELF checks, package checks and fresh public-source correspondence passed. The owner has the successor installed for testing; restored personal saves and new font appearance await owner confirmation. This is not a claim of full-game, universal firmware or performance qualification. The internal diagnostic label remains Build20-test.

The reported firmware10.60 immediate-launch failure remains unresolved; this release does not claim to fix it. Chicago hack/color-coded names, all-menu remastering, alternative finger pointer and Day1–6 chapter-card remastering remain unfinished. No ray tracing or DLSS. Private HD background pack redistribution remains pending permission.

Complete reconstruction recipe and component notices are included in source/native-public and platform/ps5/release-build20. Original game assets, private saves/settings/logs/fingerprints, caller initializer/input and proprietary Sony modules are excluded. The source-derived public GPL loader companion is included with notices.

## Community and thanks

Community: https://discord.gg/Yg88tHYXK

Based heavily on khasinski/parasite-eve-pc. Thanks to its developers, Matthew Dwinell, community playtesters, the original Parasite Eve team, PsyZ authors, GUMMIBRI and PS5 contributors with Codex assistance, and all component authors named in the retained notices. Original-game rights remain with their holders.

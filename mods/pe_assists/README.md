# Optional native playtest assists

Manifest: `pe_assists.c` and this README only. Newly authored independent implementation, Apache-2.0 (SPDX in source); no legacy mixed-provenance bodies, original game bodies, guest addresses or phase-identity material copied. Relies on the current native project's existing typed public mod interfaces; their own licenses remain applicable. No builds, tests or gameplay performed.

## Integration

Copy this folder to native source `mods/pe_assists/`. Build with `PE_MODS=ON` and append **pe_assists** to the existing semicolon-separated `PE_MOD_LIST` without replacing other selected mods. Existing `mods/CMakeLists.txt:28-33` globs its C file and generates `extern const PeMod pe_mod_pe_assists;` and `&pe_mod_pe_assists,`; no manual registry entry is needed with that CMake flow. A custom source-list build must include this source and the host, add that extern and list entry itself, and provide native `include/` and `mods/host/` include paths. No source changes outside this folder are included.

Both toggles initialize to 0. The host's F1 panel exposes **Playtest assists > Refill HP while alive** and **Zero table-based PE cost**. Host controls also require **F1 > Manage mods > All mods** plus this mod enabled; runtime enable/disable changes apply after restart per upstream mods/README.md. The host saves row settings when its panel closes and restores them when rows register, in `pe_mods.ini` through `pe_mod_path`. Exact saved row keys are `Playtest assists/Refill HP while alive` and `Playtest assists/Zero table-based PE cost`, values0/1. These are persisted user choices, so a prior saved1 overrides the initial0. No environment-variable switches or guessed controller shortcuts are introduced.

F1 is the verified upstream keyboard entry. This module registers rows only: it supplies neither a controller shortcut nor a software-renderer overlay. PS5 software-build panel rendering/input accessibility must be established by the integrator; do not promise the menu appears on console merely because this module compiles. Host persistence also requires a working writable preferences path.

## Behavior and boundaries

HP is refilled through FRAME and BATTLE_UPDATE only for a loaded native Aya core with positive current HP, maxHP1..9999, and current HP below maximum. Dead Aya stays dead; it does not prevent a lethal hit, remove ailments, edit saves or guarantee protection between updates. It does not write hpMirror or any other field.

Zero cost changes only a positive mutable `PeHookPowerCost.cost` to0. It does not refill PE, unlock powers, override availability, or touch HP/ammo requirements. In current `src/main/item/actions.c:943`, the hook covers table-derived raw cost before armor discount; special earlier branches bypass it. The label deliberately says table-based. A later higher-priority mod may change cost again.

No XP multiplier, item restoration, damage scaling, one-hit mode, raw-address access, boss-phase assumptions or memory-card behavior.

## Source ABI evidence

Inspected exact current source at `C:\.pe\ps5-native\builds\upstream-20261029T172419Z-92bd954522f14898925968d4e9195956\src` (owner identifies b1c225 native source):

- `mods/host/pe_mod_host.h`: API version1; PeHookFn is `int (*)(void *, void *)`; pe_hook_add returns0 or-1; PeMod fields are id/name/version/requires/init/shutdown.
- `mods/host/pe_mod_menu.h`: pe_menu_register_toggle takes sub/label/description/int-pointer/optional tick; returns0 or-1; static toggle rows persist automatically. No custom serialization necessary.
- `mods/host/pe_game.h` and `.c`: pe_game_loaded returns int; pe_aya_core returns native Combatant pointer from current player core.
- `include/pe1/battle.h`: native Combatant has typed u16 curHP/maxHP and host-pointer-aware layout. Source uses field access, never assumed offsets.
- `include/pe_hooks.h`: FRAME/BATTLE_UPDATE contextsNULL; POWER_COST context is `{int power; int *cost;}`.
- `src/main/boot/frame_loop.c:133`, `src/main/battle/encounter.c:246`, `src/main/item/actions.c:943`: actual dispatch sites.

ABI was checked by reading declarations and call sites, not by compilation; integration and on-console validation remain pending.

PS5 integration status: cmake/ps5/game.cmake currently forces PE_MODS=OFF. This module is source-ready and cross-compiles against current typed headers, but is not linked into the PS5 delivery. The current F494 crash-trace app remains unchanged. Do not remove the platform gate or claim working console cheats without integrating the native mod host. No XP hook or item-preservation change is included.


# Mods

Mods for the native Parasite Eve port, built only with `-DPE_MODS=ON`. With it off the game is
the unchanged original: the `PE_HOOK(...)` lines in the game compile to nothing.

```
cmake -S . -B build-mods -G Ninja -DPE_MODS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-mods
```

`PE_MOD_LIST` (CMake cache) picks the mods, by folder name. Default:
`pe_qol;pe_rebalance` (add `pe_hello` for the example).

Two switches, so a modded build is safe to hand out:
- **Build**: `PE_MODS` off (the default) leaves no mod code in the game at all.
- **Runtime**: F1 > Manage mods > **All mods**, **off by default**. Until a player turns it on and
  restarts, no mod starts and nothing but the F1 panel touches the game. Below it, one toggle per
  mod; a mod turned off doesn't start, nor do the mods that need it. Changes apply at the next start.

The upstream test suite (`-DBUILD_TESTING=ON`) gives the same results with `PE_MODS` on and off.

## Layout

```
include/pe_hooks.h     hook ids and context structs: the only file the game includes
mods/
  host/                the mod host, shared by every mod
    pe_mod_host.*        mod registry (start order by requirements, Manage mods), hooks, services,
                         events, pe_mods.log, pe_play.log (each row also the "play.log" event), pe_toast
    pe_mod_menu.*        rows of the Mods panel and pe_mods.ini
    pe_mod_panel.c       the F1 panel: the list of mods, pages, navigation, input
    pe_pages.*           where PE Enhancements rows live (one layout for the whole suite)
    pe_services.h        every service struct mods share (no mod includes another's headers)
    pe_game.*            game access and the game facts every mod shares (see below)
    pe_game_facts.c      those facts as tables: mod effect names and groups, power tags
    pe_main_menu.*       extra rows in Aya's main menu, and screens from the game's own widgets
    pe_gametext.h        text in the game's glyph code; pe_item_table.h the game's item names
    ui/                  drawing (pe_ui_raster), the overlay quad (pe_ui_gpu), toasts
  pe_qol/              quality of life: movement, encounters, key item pocket, bag sort, Key Items menu
  pe_rebalance/        mod effects, Parasite Energy balance, half-bar turns / Quick powers, Combine
  pe_hello/            example mod (not built by default)
```

## A mod

A folder with `.c` files, one of which defines its descriptor:

```c
const PeMod pe_mod_<folder> = {"<folder>", "Readable name", "0.1", requires, init, shutdown};
```

Versions: the mod API is `PE_MOD_API_VERSION` (pe_mod_host.h, now 1), raised only when a change
would break existing mods; the PE Enhancements suite is `PE_ENHANCEMENTS_VERSION` (pe_pages.h,
now 0.9.0), one version for all of its mods.

`init` registers the mod's rows, hooks and services. Each feature lives in its own file with
one `pe_<feature>_register(mod_id)` function that the mod's main file calls, so a feature can be
moved, disabled or updated without touching the others. Add the folder name to `PE_MOD_LIST`;
`pe_hello` is the smallest complete example (rows, a hook, a service, an event, its own top page).

## Talking to the game

- **Hooks** (`pe_hook_add`): one line in the game at a function entry or a fixed moment, with a
  context struct (`include/pe_hooks.h` lists them). Handlers run in priority order; a handler
  that changes a value sees the previous one's result. Add a hook only where a mod needs one,
  and keep it a single line.
- **Game state**: use the game's own globals and structs (`pe_game.h` has the common ones),
  never PS1 addresses: native structs hold host pointers and their own layouts.
- **Shared game facts live once, in the host** (`pe_game.h`): `pe_free_field()` (no battle, menu,
  transition or event), `pe_save_loaded()` (also mid room load, when Aya's actor is rebuilt),
  `pe_room_number()`, `pe_wayne_count()` / `pe_set_wayne_count()`, `pe_mod_effect_name()` /
  `pe_mod_effect_group()`, `pe_power_tag()`. Never copy one of these into a mod.

## Talking to other mods

- **Services**: `pe_service_publish("junk", 1, &api)` / `pe_service_get("junk", 1)`. A service is
  a versioned struct of functions; a mod that finds none carries on without it.
- **Events**: `pe_event_id("name")` and `pe_event_emit(id, ctx)` for announcements.

Service definitions live in `host/pe_services.h`, never in a mod's folder.

## The Mods panel

- F1 always opens on the list of mods: each mod's top page (`pe_menu_begin("My Mod")`, described
  and ordered with `pe_menu_top_page`), then the host's Manage mods. The PE Enhancements mods share
  one top page, laid out by topic.
- PE Enhancements rows go on the pages in `host/pe_pages.h` (Movement, Combat, Economy, Inventory,
  Collection, Debug); tuning and test rows always go under Debug.
- Values the player sets: `toggle`, `choice`, `int` rows (saved in `pe_mods.ini` by page and
  label: renaming a row resets it). Values read from the game: `live_*` rows (not saved).
- Read-only values (Junk, prices, results) are `info` rows, never live rows that refuse changes.
- Every row has a one-line description; actions that cost or destroy something are press-twice.

Movement speed uses the local `PLAYER_MOVE` hook in the original input movement
handlers. It never writes the actor's `moveFactor`: cutscenes and battle-entry
scripts own that value, including values that match a mod speed option.
Always Run changes a local input mask, never the game's mapping table. Battle-start
speed scales the local script speed before its destination clamp, and the local
animation increment; it never writes the actor's velocity or `animStep`.
These options keep no per-actor ownership state to restore after a scene or save load.
## Saves

Per-save data goes through the host, the same way for every mod (`host/pe_save.h`): register each
piece of state once in your init, and the host saves, loads and carries it. Never write into the card
block yourself.

```c
static struct { uint16_t count, pad, ids[32]; } s_keys;
static const PeSaveRecord s_record = { "pe_qol.keys", 1, &s_keys, sizeof s_keys, check, upgrade, reset };
pe_save_register(&s_record);
```

- **Save:** the host copies every running mod's registered state into the card block's free tail
  (0x1C90..0x1FFF, inside the game's CRC) as named, versioned records, after every mod's own
  `PE_HOOK_SAVE_WRITE` handler.
- **Load:** the records go back into each mod's memory before any mod's `PE_HOOK_SAVE_READ` handler
  runs. `check()` can fix up or refuse what was loaded; a refused or missing record gets `reset()`
  (zeros without one).
- **Layouts:** raise the version when the layout changes and read older ones in `upgrade()`. A record
  newer than the build can read is kept as it is, so playing on an older build doesn't destroy it.
  The same goes for the whole tail when its container format is newer: every save writes it back
  unchanged and the mods start from their defaults. Write the state as a struct with a
  `_Static_assert` on its size, so a changed layout can't ship without a new version.
- **Mods off:** a record whose mod is off, failed to start or isn't in the build carries from the
  loaded save into every later save unchanged.
- **New game:** `PE_HOOK_NEW_GAME` (`PeHookNewGame`: the title's New Game or Tutorial, or the EX Game).
  The host resets every record and drops the carried ones before any mod hears it, so a record
  needs no handler of its own; reset anything else per game there. Don't test for script command
  1117: only the EX Game sends it, and a New Game from the title after a loaded save sends nothing.
- **Crash reports:** `pe_save_crash_tail()` copies the same records from memory without calling any
  mod code, so a save rebuilt from a report holds every mod's data. The room snapshot the crash
  report takes when a room starts uses it, and the report lists its records.
- **Older builds:** tail bytes 0x200..0x287 stay the old PKEY (key pocket) and SORT blocks, which
  pe_qol still writes itself, because the official releases read and write them there. The records
  run around those bytes and their CRC leaves them out, so a save made in an official release keeps
  every mod's data and its pocket, and the pocket it saved wins when the save comes back here.
- **Damage:** each record carries its own CRC. If another build writes over part of the records,
  only the records it hit reset; the rest load. The header also records who wrote the save (mod API
  version and the host's save writer revision), so later builds can tell older saves apart.
- **Room:** 744 bytes (the tail's 880 less those 136; 20 for the header, 12 per record). A record
  that doesn't fit is refused when it registers, with a line in `pe_mods.log`.

Records today:

| Record | Holds | Owner |
|---|---|---|
| `pe_qol.keys` | key item pocket | pe_qol/pe_key_items.c |
| `pe_qol.sort` | bag sort | pe_qol/pe_bag_sort.c |

Saves from earlier builds kept these as fixed blocks (`PKEY` +0x200, `SORT` +0x260 from 0x1C90).
They still load: each block reaches its owner's `upgrade()` as version 0, and the next save writes
records. pe_qol still writes both blocks in the window too, so builds without records keep reading
them. Checks: tests/run_pe_save_test.ps1.

## Optional encounter controls

No random battles (Combat) reads the game's own saved switch. Turning it off
clears only that bit; loading another save or starting a new game clears the
session override. A save made with the switch on retains it.

Each feature has its own file and registration function in pe_qol.

## Half-bar turns

Combat / Parasite Energy includes the recomp's half-bar ATB rules. Rebalanced
allows Items, Quick PE powers and weapon changes at half a bar; armor changes
and escape remain full-bar by default. Quick powers default to Heal 1/2,
Detox, Medic, Scan, Slow and Confuse. On a half bar every other power shows
dimmed in the PE menu, like a power Aya can't afford (PE_HOOK_POWER_AVAILABLE),
so Energy Shot and the rest need a full bar. Cancelling refunds the borrowed
gauge; attacks still pay a full bar through refill debt. Vanilla disables
half-bar actions. The gold gauge indicator uses native HUD quads.

Runtime turn accounting lives in pe_rebalance/pe_atb_items.c; settings remain
on the PE rules page. No card-save blocks are required.

Regression coverage is retained in tests/pe_mod_atb_test.c. It links only the
half-bar and PE-balance feature sources against native headers, with test-owned
game state and mod-host callbacks, to exercise turn accounting and rule presets.

## Battle frequency

Combat / Battle frequency ports the recomp's Normal, More (2x), Lots (4x),
and Every check choices. Normal is the default. It changes the random-roll
range only for the encounter script sequence; unrelated RNG is unchanged.
Every check still respects story gates, room encounter rules and rooms with
no encounters. No random battles overrides it. This increases fight frequency,
not the number of enemies in a fight.

pe_qol/pe_battle_frequency.c owns matching and settings. A single SCRIPT_RANDOM
hook supplies local RNG bounds; loaded scripts and save data are not patched.

## Panacea and medicine combining

Panacea (item 0x19) restores 400 HP and cures ailments in battle or the field,
while preserving ailment immunities. It uses Medicine 4's icon and healing
effect. The Parasite Energy rules page enables it in Rebalanced and disables
it in Vanilla; names remain readable for already-carried Panacea.

Combat / Combine items provides the recomp recipes through the shared F1
panel: two Medicine 1/2/3 become the next tier, or Medicine 4 plus Full Cure
become Panacea. Junk economy must be enabled, both ingredients must be in
Aya's bag, and combining is outside battle and game menus. Press Combine
twice; the bag is checked again before consuming anything.

In Aya's item menu a medicine whose partner is in the bag gets a fourth
action, Combine, under Use / Move / Discard; it combines at once and closes
the list (Combine items / Combine in item menu). Items with an available Combine
recipe remain white in Aya's bag even at full health; Use keeps its native
availability and dimming. The Combine module owns this presentation through
PE_HOOK_INVENTORY_ROW without changing selection bits. Checks:
tests/run_pe_combine_colour_test.ps1. It uses PE_HOOK_ITEM_ACTIONS:
the action list just opened, whose row count, row drawer and panel input step
a mod may replace. Text helpers for the game's glyph code are in
host/pe_gametext.h.

pe_rebalance/pe_panacea.c owns effects, labels and use flags; pe_combine.c
owns recipes and confirmation. They share only their feature header and
use the versioned Junk service for the optional economy dependency. Existing
item IDs use the normal game save; no new save block is needed.

## Inventory convenience

Inventory / Key item pocket moves story keys and Chrysler elevator keys out
of Aya's bag. Inventory / Key items browses them. Story count/removal hooks
query the pocket directly, including with a full bag; no equipment or items
are parked elsewhere. Turning it off returns keys as slots become free, while
remaining pocketed keys still work for story checks. Key pickups and full-bag
found-list rewards also use the pocket. The keys v1 service remains compatible
with pe_economy's loot handling.

Bag sort starts Manual for a new game. Use the game's Sort screen to choose
an order and enable Auto, or choose Auto in F1 / Inventory. Bag changes are
sorted only in free field; manually rearranging the same items in the game
menu switches back to Manual. The native sorter preserves equipped slots.
Square on Aya's Use Item list opens the native Sort screen. Sort is hidden
from the main menu; Wayne's storage is untouched by automatic sorting.
pe_qol/pe_inventory_menu.c owns the shortcut and transient row hiding, separate
from sorting policy. It restores the original menu mask on save and menu exit.
Inventory tips (on by default) say so: the title bar on the main menu's Items
row and on an empty bag slot, and a message the first time the bag opens each
session, including whether Auto sort is on.

Key Items in Aya's main menu (pe_qol/pe_key_menu.c, while the pocket is on): a row with the
Dressing Room Key's icon opens a view-only list of the pocket, each key with its icon and name and
the highlighted key's game description in the title bar. Circle or Cross goes back.

Extra main-menu rows and screens are a host service (host/pe_main_menu.h): a mod adds a row
(title, icon item or its own icon drawer, shown test, open callback, order) and builds its screen
with pe_screen_open_game (flush where the game's own screens sit) / pe_screen_rows /
pe_screen_close, which also hide Aya's status panels and keep scrolling inside the list. The host
owns the main-menu hooks (LIST_ROWS, MAIN_MENU_ICON, HELP_TEXT for the row names,
MAIN_MENU_ACTION). Order: Mod Catalog 10, Key Items 20, Wardrobe 30.

pe_qol/pe_key_items.c and pe_bag_sort.c each own their state, hooks and rows.
pe_inventory.h holds private save/field helpers. PKEY and SORT use the exact
recomp v1 card layouts and checksums. SORT stores original table identifiers,
which map explicitly to native tables; no host pointers are serialized.

Run the inventory regression checks on Windows with
`tests/run_pe_inventory_test.ps1` after configuring build-mods. The test
links both feature files and the real native inventory sorter; unrelated
game paths are isolated by explicit test stubs.

## Tests

`tests/run_pe_*_test.ps1` (Windows, after configuring build-mods) build and run each mod's checks
against the production files, with the game and host stubbed where they don't matter.


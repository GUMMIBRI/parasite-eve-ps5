#ifndef PE_GAME_H
#define PE_GAME_H

/* Game access shared by mods: the recomp build's address helpers, in terms
 * of the native game's own structs and globals. */

#include "common.h"
#include "pe1/aya.h"
#include "pe1/battle_runtime.h"
#include "pe1/inventory_slots.h"

BattleEntity *pe_player(void);       /* Aya's actor, or NULL */
Combatant *pe_aya_core(void);        /* her Combatant (stats, gauge, action), or NULL */
int pe_game_loaded(void);            /* a game is running (not the title screens) */
int pe_save_loaded(void);            /* a game's data is loaded, even mid room load (no player actor yet) */
int pe_in_battle(void);
/* Free field: Aya walking about -- no battle, menu, room transition or scripted event
 * (g_GameStateFlags & 0x2006, g_GameState.flags & 0xB600). */
int pe_free_field(void);
int pe_room_number(void);            /* the room's map number ("M374..." -> 374), or -1 */

/* ---- facts about the game, one copy for every mod ---- */

/* Wayne's Junk count toward his next gun (script variable 48; his crafts take 300). */
unsigned pe_wayne_count(void);
void pe_set_wayne_count(unsigned count);

/* The game's own gun / armor mod effects (effect = low 5 bits of a mod byte): names in title case
 * ("Rate of Fire 2", "Bag +4"), NULL for armor effects 16-20 (unused by the game; the economy uses
 * 17-20), and the exclusivity group (high 3 bits of a mod byte; one mod per non-zero group). */
#define PE_GUN_EFFECTS_GAME   20
#define PE_ARMOR_EFFECTS_GAME 15
const char *pe_mod_effect_name(int armor, int effect);
int pe_mod_effect_group(int armor, int effect);

/* Parasite Energy powers by index (0 Heal 1 .. 19 Liberate): a compact tag for logs ("EnergyShot"). */
const char *pe_power_tag(int power);

/* ---- host features (started by the host) ---- */
/* Equipment sync: whenever the equipped gun or armor record changes in free field (Catalog, Tools,
 * editors, Rebuild ...), the game's own equipment rebuild runs, so the change reaches the next
 * battle without a room change. */
void pe_equip_sync_init(void);

/* Equipped weapon (armor = 0) or armor (armor = 1) record, or NULL. */
ItemDataRecord *pe_equipped(int armor);
/* Base item data for item id (1-based), or NULL. */
ItemDataRecord *pe_base_item(unsigned id);
/* The record carries mod effect `effect` (low 5 bits of a mod byte). */
int pe_rec_has_effect(const ItemDataRecord *rec, int effect);

unsigned pe_isqrt(unsigned v);

#endif

#ifndef PE_ECONOMY_H
#define PE_ECONOMY_H

/* The economy mod's own calls, shared by its files. Other mods use the services in
 * host/pe_services.h instead. */

#include <stddef.h>
#include <stdint.h>
#include "pe_services.h"   /* PE_FX_*, PeJunkApi, PeCatalogApi, PeKeysApi, PeWardrobeApi */

/* Direct calls inside the economy mod. */
int pe_loot_enabled(void);
uint32_t pe_loot_junk(void);
int pe_loot_spend_junk(uint32_t n);
void pe_loot_give_junk(uint32_t n);
int pe_loot_armor_has(int effect);
uint32_t pe_loot_price(uint32_t base);
int pe_loot_wayne_waiting(void);
int pe_loot_handin_blocked(void);

/* The Mod Catalog (pe_catalog.c, pe_dismantle.c). which: 0 = the equipped gun, 1 = the equipped armor.
 */
enum {
    PE_CAT_OK, PE_CAT_NO_GAME, PE_CAT_OFF, PE_CAT_NO_ITEM, PE_CAT_NO_SLOTS, PE_CAT_LOCKED,
    PE_CAT_ALREADY, PE_CAT_BAG, PE_CAT_JUNK, PE_CAT_TOOL, PE_CAT_EMPTY,
};
struct ItemDataRecord *pe_cat_record(int which);  /* the equipped gun / armor record, or NULL */
int pe_cat_slots(int which);
int pe_cat_slot_effect(int which, int slot);
int pe_cat_board_count(int which);
int pe_cat_board_effect(int which, int i);
int pe_cat_unlocked(int which, int e);
uint32_t pe_cat_price(int which, int e);
int pe_cat_needs_tool(int which, int e);
int pe_cat_is_bag_mod(int which, int e);
int pe_cat_install(int which, int slot, int e, int dry_run);
int pe_cat_remove(int which, int slot, int dry_run);
int pe_cat_replaces(int which, int slot, int e, int *out, int max);
int pe_catalog_armor_unlocked(int effect);
int pe_catalog_unlock_armor(int effect);
uint32_t pe_catalog_dismantle_preview(const struct ItemDataRecord *rec);
uint32_t pe_catalog_dismantle_reward(const struct ItemDataRecord *rec, int *unlocks_out);
int pe_rebuild_count(void);
uint8_t pe_rebuild_item(int i);
uint32_t pe_rebuild_price_index(int i);
unsigned pe_rebuild_bought_slots(int i);
int pe_rebuild_index(int i, char *msg, size_t msg_size);

#endif

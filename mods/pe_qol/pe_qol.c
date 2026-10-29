/* PE Enhancements: quality of life (movement; more as they are ported), from the psxrecomp
 * build. Rows at the top of F1 > PE Enhancements. */
#include "pe_mod_host.h"
#include "pe_pages.h"
#include "pe_inventory.h"

#define MOD_ID "pe_qol"

void pe_move_register(const char *mod_id);
void pe_encounters_register(const char *mod_id);
void pe_battle_frequency_register(const char *mod_id);

static int qol_init(void) {
    pe_move_register(MOD_ID);
    pe_key_items_register(MOD_ID);
    pe_key_menu_register(MOD_ID);
    pe_bag_sort_register(MOD_ID);
    pe_inventory_menu_register(MOD_ID);
    pe_encounters_register(MOD_ID);
    pe_battle_frequency_register(MOD_ID);
    return 0;
}

const PeMod pe_mod_pe_qol = {MOD_ID, "PE Enhancements: quality of life", PE_ENHANCEMENTS_VERSION, NULL, qol_init, pe_inventory_menu_shutdown};

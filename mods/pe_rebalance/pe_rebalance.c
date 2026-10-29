/* PE Enhancements: rebalance (mod effects, Parasite Energy, weapon power), ported from the
 * psxrecomp build. Rows under F1 > PE Enhancements. */
#include "pe_mod_host.h"
#include "pe_pages.h"
#include "pe_atb_items.h"
#include "pe_panacea.h"

#define MOD_ID "pe_rebalance"

void pe_mod_effects_register(const char *mod_id);
void pe_pe_balance_register(const char *mod_id);
void pe_weapon_power_register(const char *mod_id);
void pe_mod_effects_shutdown(void);

static int rebalance_init(void) {
    pe_atb_items_register(MOD_ID);
    pe_panacea_register(MOD_ID);
    pe_combine_register(MOD_ID);
    pe_weapon_power_register(MOD_ID);
    pe_mod_effects_register(MOD_ID);
    pe_pe_balance_register(MOD_ID);
    return 0;
}

static void rebalance_shutdown(void) {
    pe_atb_items_shutdown();
    pe_panacea_shutdown();
    pe_mod_effects_shutdown();
}

const PeMod pe_mod_pe_rebalance = {MOD_ID, "PE Enhancements: rebalance", PE_ENHANCEMENTS_VERSION, NULL, rebalance_init, rebalance_shutdown};

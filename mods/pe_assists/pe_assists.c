/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 PE-PORT contributors.
 * Independent typed-hook assists; no legacy implementation or address patches.
 */
#include "pe_mod_host.h"
#include "pe_mod_menu.h"
#include "pe_game.h"

#define MOD_ID "pe_assists"
static int s_refill_hp = 0;
static int s_free_powers = 0;

static int refill_hp(void *ctx, void *user)
{
    Combatant *aya;
    (void)ctx;
    (void)user;
    if (!s_refill_hp || !pe_game_loaded()) return 0;
    aya = pe_aya_core();
    if (aya && aya->curHP > 0 && aya->maxHP > 0 && aya->maxHP <= 9999 &&
        aya->curHP < aya->maxHP)
        aya->curHP = aya->maxHP;
    return 0;
}

static int free_power_cost(void *ctx, void *user)
{
    PeHookPowerCost *query = ctx;
    (void)user;
    if (s_free_powers && pe_game_loaded() && query && query->cost &&
        *query->cost > 0)
        *query->cost = 0;
    return 0;
}

static int assists_init(void)
{
    int failed = 0;
    pe_menu_begin("Playtest assists");
    failed |= pe_menu_register_toggle(NULL, "Refill HP while alive",
        "Refills between updates; no revival or lethal-hit immunity.",
        &s_refill_hp, NULL);
    failed |= pe_menu_register_toggle(NULL, "Zero table-based PE cost",
        "Waives hooked PE costs only; special power requirements remain.",
        &s_free_powers, NULL);
    pe_menu_begin(NULL);
    failed |= pe_hook_add(PE_HOOK_FRAME, refill_hp, NULL,
        PE_PRIORITY_LATE, MOD_ID);
    failed |= pe_hook_add(PE_HOOK_BATTLE_UPDATE, refill_hp, NULL,
        PE_PRIORITY_LATE, MOD_ID);
    failed |= pe_hook_add(PE_HOOK_POWER_COST, free_power_cost, NULL,
        PE_PRIORITY_LATE, MOD_ID);
    return failed ? -1 : 0;
}

const PeMod pe_mod_pe_assists = {
    MOD_ID, "Playtest assists (optional)", "0.1", NULL, assists_init, NULL
};


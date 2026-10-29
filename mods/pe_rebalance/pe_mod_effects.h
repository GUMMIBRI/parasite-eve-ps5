#ifndef PE_MOD_EFFECTS_H
#define PE_MOD_EFFECTS_H

/* What the mod effects rebalance (pe_mod_effects.c) adds to a gun, for the DPS line
 * (pe_weapon_power.c). Both are 0 under Vanilla rules. */

int pe_fx_attack_bonus(unsigned gun);   /* flat attack bonus (G3A3, M203 4, progression guns) */
int pe_fx_rof_volley_pct(void);         /* installed Rate of Fire: whole-volley damage % */

#endif

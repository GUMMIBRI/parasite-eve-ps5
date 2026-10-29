/* Facts about the game every mod shares, as tables (pe_game.h): no game state is read here, so
 * tests link this file as it is. */
#include <stddef.h>

#include "pe_game.h"

/* In characters the game's own menu font has too (no brackets or dashes: pe_gametext.h). */
static const char *const s_gun_effects[PE_GUN_EFFECTS_GAME + 1] = {
    "Empty", "Rate of Fire 2", "Rate of Fire 3", "Rate of Fire 5", "Rate of Fire 7", "Rate of Fire 10",
    "Random Fire", "Burst Spray", "Attack All", "Heat Bullets", "Frost Bullets", "Cyanide Bullets",
    "Acid Bullets", "Tranquilizer", "Counterattack", "Quickdraw", "Steal, half attack", "Steal on Attack",
    "Critical Up", "2 Commands", "3 Commands",
};
static const char *const s_armor_effects[PE_ARMOR_EFFECTS_GAME + 1] = {
    "Empty", "Resist Poison", "Resist Stiffness", "Resist Darkness", "Less Stolen", "Resist Confusion",
    "Auto Medicine", "Auto Cure", "Bag +1", "Bag +2", "Bag +4", "Max HP Up", "Attack Up", "Slower Active Time",
    "PE Burden Down", "Attack Down",
};

const char *pe_mod_effect_name(int armor, int effect) {
    if (effect < 0) return NULL;
    if (armor) return effect <= PE_ARMOR_EFFECTS_GAME ? s_armor_effects[effect] : NULL;
    return effect <= PE_GUN_EFFECTS_GAME ? s_gun_effects[effect] : NULL;
}

/* As read from every base item's mod bytes. */
int pe_mod_effect_group(int armor, int effect) {
    if (armor) return effect >= 8 && effect <= 10 ? 5 : 0;   /* bag capacity */
    if (effect >= 1 && effect <= 5) return 1;                /* rate of fire */
    if (effect >= 6 && effect <= 8) return 2;                /* multi-shot */
    if (effect == 19 || effect == 20) return 3;              /* extra commands */
    if (effect == 16 || effect == 17) return 4;              /* steal */
    return 0;
}

const char *pe_power_tag(int power) {
    static const char *const tags[20] = {
        "Heal1", "Heal2", "Heal3", "Detox", "Medic", "Barrier", "EnergyShot", "Scan", "Slow", "Haste", "Confuse",
        "GeneHeal", "Preraise", "Mito14", "Mito15", "Mito16", "Mito17", "Mito18", "FullRecover", "Liberate",
    };
    return power >= 0 && power < 20 ? tags[power] : "?";
}

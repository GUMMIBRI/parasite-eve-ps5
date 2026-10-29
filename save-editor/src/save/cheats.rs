//! Cheats operations for PeSaveSlot.
use super::*;

impl PeSaveSlot {
    // =========================================================================
    // Quick Cheat & Preset Helpers
    // =========================================================================

    /// Repair icon/model classes written by older editor templates without
    /// replacing a player's upgrades, ammunition, effects or engraving.
    pub fn repair_equipment_types(&mut self) -> usize {
        let mut repaired = 0;
        for item in self.equipment.iter_mut().filter(|item| item.is_active()) {
            if (0x3F..=0xC7).contains(&item.item_id) {
                let meta = item_metadata(item.item_id);
                if item.icon_id != meta.default_icon_id || item.kind != meta.default_kind {
                    item.icon_id = meta.default_icon_id;
                    item.kind = meta.default_kind;
                    repaired += 1;
                }
            }
        }
        repaired
    }

    pub fn apply_cheat_max_aya(&mut self) {
        self.set_display_level(99);
        self.total_exp = 9_999_999;
        // The game's own top: 869, or 999 with HP Max Up armor equipped (see `to_bytes`).
        self.max_hp = self.max_hp_limit();
        self.current_hp = self.max_hp;
        self.bonus_points = 99_999;
        self.inventory_slot_count = MAX_STAT_BAG_SLOTS;
        self.pe_bonus_pool = 999;
        self.stats_levels = [1000; 7];
    }

    pub fn apply_cheat_unlock_all_pe(&mut self) {
        // Unlock all standard playable PE spells (0..=12, 18, 19), skipping Mito14..Mito18 (13..=17)
        for spell in PARASITE_SPELLS.iter() {
            if !spell.category.contains("Dummy") {
                self.set_spell(spell.bit_index, true);
            }
        }
    }

    pub fn apply_cheat_unlock_all_pe_including_hidden(&mut self) {
        self.parasite_spell_flags |= 0x000F_FFFF;
    }

    pub fn apply_cheat_max_ammo(&mut self) {
        self.category_counts[0] = 999; // 9mm / Standard Bullets
        self.category_counts[1] = 999; // Rockets
        self.category_counts[2] = 999; // DNA Bullets
        for rec in &mut self.equipment {
            if rec.is_active() && rec.is_weapon() {
                let cap =
                    (rec.base_stats[2] as i32 + rec.bonus_stats[2] as i32).clamp(1, 999) as u16;
                rec.ammo = cap;
            }
        }
    }

    pub fn apply_cheat_max_equipped_gear(&mut self) {
        for rec in &mut self.equipment {
            if rec.is_active() {
                rec.bonus_stats = [999, 999, 999];
                rec.tail_capacity = 10;
                rec.tail_count = 10;
                rec.set_engraved(true);
                if rec.is_weapon() {
                    rec.ammo = 999;
                }
            }
        }
    }

    /// Adds an ultimate P90 and Cm Suit 2 and equips them (the old gear stays in the bag), plus
    /// tool kits and medicine. The suit carries HP Max Up, so with Max Aya max HP reaches 999.
    pub fn apply_cheat_give_ultimate_loadout(&mut self) -> Result<()> {
        self.inventory_slot_count = MAX_STAT_BAG_SLOTS;
        self.stats_levels[ITEM_CAPACITY_STAT] =
            self.stats_levels[ITEM_CAPACITY_STAT].max(MAX_BAG_ITEM_CAPACITY_STAT);
        self.apply_cheat_max_ammo();

        // Create Ultimate DE50AE7 / P90 and Cm Suit 2 if free slots exist
        if let Some(weapon_eq_val) = self.allocate_equipment_from_item(0x78) {
            // 0x78 = P90 Machine Gun
            let eq_idx = (weapon_eq_val - 0x100) as usize;
            let rec = &mut self.equipment[eq_idx];
            rec.bonus_stats = [900, 900, 900];
            rec.ammo = 950;
            rec.tail_capacity = 10;
            rec.tail_count = 10;
            rec.set_engraved(true);
            for (slot, mod_id) in [0x03u8, 0x14, 0x0F, 0x0E, 0x12, 0x0C, 0x09, 0x0A, 0x0B, 0x0D]
                .into_iter()
                .enumerate()
            {
                rec.set_mod_at(slot, mod_id);
            }
            if let Some(pos) = self.inventory_items.iter().position(|&v| v == 0) {
                self.inventory_items[pos] = weapon_eq_val;
                self.equipped_weapon_slot = pos as i8;
            }
        }

        if let Some(armor_eq_val) = self.allocate_equipment_from_item(0xC7) {
            // 0xC7 = Cm Suit 2
            let eq_idx = (armor_eq_val - 0x100) as usize;
            let rec = &mut self.equipment[eq_idx];
            rec.bonus_stats = [900, 900, 900];
            rec.tail_capacity = 10;
            rec.tail_count = 10;
            rec.set_engraved(true);
            for (slot, mod_id) in [0x0Au8, 0x0B, 0x06, 0x07, 0x0E, 0x01, 0x02, 0x03, 0x05, 0x04]
                .into_iter()
                .enumerate()
            {
                rec.set_mod_at(slot, mod_id);
            }
            if let Some(pos) = self.inventory_items.iter().position(|&v| v == 0) {
                self.inventory_items[pos] = armor_eq_val;
                // Worn, so its HP Max Up counts: with Max Aya, max HP reaches 999.
                self.equipped_armor_slot = pos as i8;
            }
        }

        // Also add Super Tool Kit (0x36), Tool Kit (0x35), Full Recover (0x0A), Revive (0x12), Chrysler Keys (0x38..=0x3E)
        for &item_id in &[0x36u8, 0x35, 0x0A, 0x12, 0x11] {
            if let Some(pos) = self.inventory_items.iter().position(|&v| v == 0) {
                self.inventory_items[pos] = item_id as i16;
            }
        }
        Ok(())
    }
}

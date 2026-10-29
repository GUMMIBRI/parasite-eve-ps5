//! Stats operations for PeSaveSlot.
use super::*;

impl PeSaveSlot {
    /// The game sets the bag size from the Item Capacity stat whenever it recomputes Aya's stats
    /// (`Inv_RecalcSlotStats`: level ups, equipment changes, the status menu), so an edited bag
    /// size only sticks when the stat gives it. A changed stat sets the bag; otherwise a changed
    /// bag sets the stat. The stat tops out at 46 slots; armor mods add the rest (to 50) at run
    /// time and are not stored here.
    pub(super) fn sync_bag_with_item_capacity(&mut self, orig_slots: u8, orig_stat: u16) {
        let stat = &mut self.stats_levels[ITEM_CAPACITY_STAT];
        if *stat != orig_stat {
            self.inventory_slot_count = BAG_CAPACITY_BY_LEVEL[(*stat / 10).min(98) as usize];
        } else if self.inventory_slot_count != orig_slots {
            self.inventory_slot_count = self.inventory_slot_count.clamp(10, MAX_STAT_BAG_SLOTS);
            if BAG_CAPACITY_BY_LEVEL[(*stat / 10).min(98) as usize] != self.inventory_slot_count {
                *stat = (BAG_CAPACITY_BY_LEVEL
                    .iter()
                    .position(|&n| n == self.inventory_slot_count)
                    .unwrap()
                    * 10) as u16;
            }
        }
    }

    /// Whether the equipped armor carries HP Max Up, which lifts max HP by 3/20
    /// (`BattleCmd_LoadWeaponModifiers` reads the mods in the armor's first `tail_count` slots).
    pub fn armor_has_hp_max_up(&self) -> bool {
        Self::has_hp_max_up(self.equipped_armor_slot, &self.inventory_items, |i| {
            self.equipment.get(i).cloned()
        })
    }

    /// `armor_has_hp_max_up` as the save stood when loaded or last written (`raw_bytes`).
    pub(super) fn raw_armor_has_hp_max_up(&self) -> bool {
        let rt = &self.raw_bytes[RT_OFFSET..];
        let inventory: Vec<i16> = (0..50)
            .map(|i| i16::from_le_bytes([rt[0x68 + i * 2], rt[0x69 + i * 2]]))
            .collect();
        Self::has_hp_max_up(rt[0x42] as i8, &inventory, |i| {
            (i < 128).then(|| EquipmentRecord::parse(i, &rt[0xCC + i * 0x20..0xEC + i * 0x20]))
        })
    }

    pub(super) fn has_hp_max_up(
        armor_slot: i8,
        inventory: &[i16],
        record: impl Fn(usize) -> Option<EquipmentRecord>,
    ) -> bool {
        let Some(&val) = usize::try_from(armor_slot)
            .ok()
            .and_then(|slot| inventory.get(slot))
        else {
            return false;
        };
        let Some(rec) = usize::try_from(val as i32 - 0x100)
            .ok()
            .and_then(record)
            .filter(|r| r.is_active())
        else {
            return false;
        };
        (0..(rec.tail_count as usize).min(11)).any(|i| rec.mod_id_at(i) == ARMOR_MOD_HP_MAX_UP)
    }

    /// The highest max HP the game allows with the armor Aya has on now.
    pub fn max_hp_limit(&self) -> u16 {
        hp_with_armor(MAX_STAT_MAX_HP, self.armor_has_hp_max_up())
    }

    /// The game recomputes max HP from the HP stat at the start of every battle, on level ups and
    /// in the status menu (`Inv_RecalcSlotStats`), so an edited max HP only sticks when the stat
    /// gives it. A changed stat sets max HP; otherwise a changed max HP sets the stat. The top
    /// is 869, or 999 while the equipped armor has HP Max Up, so putting on or taking off such
    /// armor also resets max HP. Current HP is kept within max HP, and a full bar stays full.
    pub(super) fn sync_max_hp_with_hp_stat(&mut self, orig_max_hp: u16, orig_stat: u16) {
        let hp_max_up = self.armor_has_hp_max_up();
        let armor_changed = hp_max_up != self.raw_armor_has_hp_max_up();
        let was_full = self.current_hp >= self.max_hp;
        let limit = self.max_hp_limit();
        let stat = &mut self.stats_levels[HP_STAT];
        match game_data() {
            Some(g) => {
                if *stat != orig_stat || armor_changed {
                    self.max_hp = g.max_hp_for_stat(*stat, hp_max_up);
                } else if self.max_hp != orig_max_hp {
                    let wanted = self.max_hp.min(limit);
                    if g.max_hp_for_stat(*stat, hp_max_up) != wanted {
                        if let Some(v) = g.stat_for_max_hp(wanted, hp_max_up) {
                            *stat = v;
                        }
                    }
                    self.max_hp = g.max_hp_for_stat(*stat, hp_max_up);
                }
            }
            None => {
                if self.max_hp != orig_max_hp && self.max_hp >= limit {
                    *stat = (*stat).max(TOP_LEVEL_STAT);
                }
                if *stat >= TOP_LEVEL_STAT
                    && (*stat != orig_stat || self.max_hp != orig_max_hp || armor_changed)
                {
                    self.max_hp = limit;
                }
            }
        }
        self.current_hp = if was_full {
            self.max_hp
        } else {
            self.current_hp.min(self.max_hp)
        };
    }
}

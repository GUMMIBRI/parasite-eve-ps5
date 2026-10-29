//! Inspect commands.
use super::*;

pub(super) fn cmd_inspect(path: &PathBuf, slot: Option<usize>, verbose: bool) -> Result<()> {
    let container = MemoryCardContainer::open(path)?;
    println!("================================================================================");
    println!(" Container : {}", container.path.display());
    println!(" Format    : {}", container.kind.label());
    println!(
        " Active    : {} / 15 slots",
        container.active_slot_indices().len()
    );
    println!("================================================================================");

    if let Some(s) = slot {
        let idx = resolve_slot_index(&container, Some(s))?;
        let save = container.slots[idx].as_ref().unwrap();
        print_slot_summary(save, true);
    } else {
        for idx in container.active_slot_indices() {
            let save = container.slots[idx].as_ref().unwrap();
            print_slot_summary(save, verbose);
            println!(
                "--------------------------------------------------------------------------------"
            );
        }
    }
    Ok(())
}

pub(super) fn print_slot_summary(save: &PeSaveSlot, verbose: bool) {
    let slot_letter = (b'A' + save.slot_index as u8) as char;
    let crc_status = if save.is_crc_valid() {
        format!("VALID (0x{:04X})", save.stored_crc)
    } else {
        format!(
            "MISMATCH (stored 0x{:04X} != expected 0x{:04X})",
            save.stored_crc, save.calculated_crc
        )
    };
    println!(
        "Slot {:2} ({}) | File: {} | CRC-16: {}",
        save.slot_index + 1,
        slot_letter,
        save.filename,
        crc_status
    );
    println!("  Header Title : {}", save.header_title);
    match crate::mod_save::ModData::inspect(&save.raw_bytes) {
        Ok(data) => {
            println!("  Mod save data: {}", data.format);
            match crate::junk_counter::read(&save.raw_bytes) {
                Ok(Some(junk)) => println!("    Saved Junk counter: {}", junk),
                Ok(None) => {}
                Err(e) => println!("    Junk counter: {}", e),
            }
            for warning in &data.warnings {
                println!("    Warning: {}", warning);
            }
            if verbose {
                for record in &data.records {
                    println!(
                        "    {:08X} {} v{} ({} bytes)",
                        record.id,
                        record.name(),
                        record.version,
                        record.data.len()
                    );
                }
                if let Ok(Some(keys)) = data.keys() {
                    println!("    Pocketed keys: {:02X?}", keys);
                }
                if let Ok(Some(sort)) = data.sort() {
                    println!("    Bag sorting: {:?}", sort);
                }
            }
        }
        Err(e) => println!("  Mod save data: {}", e),
    }

    println!(
        "  Aya Name     : {:?} (Save Name: {:?})",
        save.aya_name, save.save_name
    );
    println!(
        "  Level / HP   : Lv {:2} | HP {:3} / {:3} | EXP: {} | BP: {}",
        save.display_level(),
        save.current_hp,
        save.max_hp,
        save.total_exp,
        save.bonus_points
    );
    println!(
        "  Story / Map  : Day {} (Story Day {}) | Map {:3} ({}) | EX Round: {}{}",
        save.play_time_class,
        save.current_story_day,
        save.current_map_number,
        save.location_description(),
        save.new_game_count + 1,
        if save.is_clear_data {
            " [CLEAR DATA]"
        } else {
            ""
        }
    );
    println!(
        "  Play Time    : {} ({} s) | Item Capacity: {} slots | PE Bonus: {}",
        save.formatted_play_time(),
        save.play_time_seconds,
        save.inventory_slot_count,
        save.pe_bonus_pool
    );
    println!(
        "  Ammo Pools   : 9mm Bullets = {} | Rockets = {} | DNA Bullets = {}",
        save.category_counts[0], save.category_counts[1], save.category_counts[2]
    );
    println!(
        "  Equipped     : Weapon Slot = {} | Armor Slot = {}",
        save.equipped_weapon_slot, save.equipped_armor_slot
    );

    let unlocked_spells: Vec<&str> = PARASITE_SPELLS
        .iter()
        .filter(|sp| save.has_spell(sp.bit_index))
        .map(|sp| sp.name)
        .collect();
    println!(
        "  PE Spells    : [0x{:05X}] {}",
        save.parasite_spell_flags,
        if unlocked_spells.is_empty() {
            "None".to_string()
        } else {
            unlocked_spells.join(", ")
        }
    );

    if verbose {
        println!("  Stat Growth Levels (base = Lv*10, max = 1000):");
        for (i, &val) in save.stats_levels.iter().enumerate() {
            println!("    {} = {}", STAT_GROWTH_NAMES[i], val);
        }

        println!(
            "  Aya's Inventory (up to {} active slots):",
            save.inventory_slot_count
        );
        for (i, &val) in save.inventory_items.iter().enumerate() {
            if val != 0 || i < save.inventory_slot_count as usize {
                let eq_tag = if save.equipped_weapon_slot == i as i8 {
                    " [EQUIPPED WEAPON]"
                } else if save.equipped_armor_slot == i as i8 {
                    " [EQUIPPED ARMOR]"
                } else {
                    ""
                };
                if val != 0 {
                    println!(
                        "    Slot {:02}: 0x{:04X} -> {}{}",
                        i,
                        val as u16,
                        save.describe_slot_value(val),
                        eq_tag
                    );
                }
            }
        }

        println!("  Active Equipment Records (0x100..0x17F):");
        for rec in save.equipment.iter().filter(|r| r.is_active()) {
            let eff = rec.effective_stats();
            let axes = if rec.is_armor() {
                format!(
                    "Def {:3} ({}+{:3}), PE {:3} ({}+{:3}), Crit {:3} ({}+{:3})",
                    eff[0],
                    rec.base_stats[0],
                    rec.bonus_stats[0],
                    eff[1],
                    rec.base_stats[1],
                    rec.bonus_stats[1],
                    eff[2],
                    rec.base_stats[2],
                    rec.bonus_stats[2],
                )
            } else {
                format!(
                    "Atk {:3} ({}+{:3}), Rng {:3} ({}+{:3}), Bul {:3} ({}+{:3}), Ammo {}",
                    eff[0],
                    rec.base_stats[0],
                    rec.bonus_stats[0],
                    eff[1],
                    rec.base_stats[1],
                    rec.bonus_stats[1],
                    eff[2],
                    rec.base_stats[2],
                    rec.bonus_stats[2],
                    rec.ammo
                )
            };
            println!(
                "    [E#{:02X} / 0x{:03X}] {:14} (ID 0x{:02X}, Kind {}, Slots {}/{}{}) | {} | Mods: {}",
                rec.index,
                0x100 + rec.index,
                rec.display_name(),
                rec.item_id,
                rec.kind,
                rec.tail_count,
                rec.tail_capacity,
                if rec.is_engraved() { ", Wayne Engraved" } else { "" },
                axes,
                PeSaveSlot::format_equipment_mods(rec)
            );
        }

        let wayne_items: Vec<(usize, i16)> = save
            .storage
            .iter()
            .copied()
            .enumerate()
            .filter(|&(_, v)| v != 0)
            .collect();
        let wayne_equip: Vec<(usize, i16)> = save
            .special_storage
            .iter()
            .copied()
            .enumerate()
            .filter(|&(_, v)| v != 0)
            .collect();
        if !wayne_items.is_empty() || !wayne_equip.is_empty() {
            println!(
                "  Wayne's Storage: {} regular items, {} special/equipment items",
                wayne_items.len(),
                wayne_equip.len()
            );
            for (idx, val) in wayne_items {
                println!("    Storage[{:02}]: {}", idx, save.describe_slot_value(val));
            }
            for (idx, val) in wayne_equip {
                println!(
                    "    SpecialStorage[{:02}]: {}",
                    idx,
                    save.describe_slot_value(val)
                );
            }
        }
    }
}

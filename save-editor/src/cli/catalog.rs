//! Catalog commands.
use super::*;

pub(super) fn cmd_list_items(filter: Option<&str>) -> Result<()> {
    let needle = filter.map(|s| s.to_lowercase());

    println!("=== PARASITE EVE ITEMS (0x00..=0xFF) ===");
    for meta in crate::tables::all_item_metadata() {
        let line = format!(
            "0x{:02X} ({:3}) | {:18} | {:24} | {}",
            meta.id,
            meta.id,
            meta.category.label(),
            meta.display_name,
            meta.description
        );
        if let Some(ref n) = needle {
            if !line.to_lowercase().contains(n) && !meta.code_name.to_lowercase().contains(n) {
                continue;
            }
        }
        println!("  {}", line);
    }

    println!("\n=== WEAPON EFFECT DEFINITIONS ===");
    for m in crate::custom_effects::choices(false) {
        let line = format!(
            "0x{:02X} | Weapon   | {:24} | {}",
            m.id, m.name, m.description
        );
        if let Some(ref n) = needle {
            if !line.to_lowercase().contains(n) && !m.code_name.to_lowercase().contains(n) {
                continue;
            }
        }
        println!("  {}", line);
    }

    println!("\n=== ARMOR EFFECT DEFINITIONS ===");
    for m in crate::custom_effects::choices(true) {
        let line = format!(
            "0x{:02X} | Armor    | {:24} | {}",
            m.id, m.name, m.description
        );
        if let Some(ref n) = needle {
            if !line.to_lowercase().contains(n) && !m.code_name.to_lowercase().contains(n) {
                continue;
            }
        }
        println!("  {}", line);
    }

    println!("\n=== PARASITE ENERGY SPELLS (Bits 0..19) ===");
    for sp in PARASITE_SPELLS.iter() {
        let line = format!(
            "Bit {:2} (0x{:05X}) | {:16} | {:18} | {}",
            sp.bit_index,
            1u32 << sp.bit_index,
            sp.category,
            sp.name,
            sp.pe_cost_note
        );
        if let Some(ref n) = needle {
            if !line.to_lowercase().contains(n) {
                continue;
            }
        }
        println!("  {}", line);
    }
    Ok(())
}

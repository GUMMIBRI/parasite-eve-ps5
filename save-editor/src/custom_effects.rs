//! PE Enhancements effect registry. Retail IDs/groups remain in tables.rs.
//! Weapon rebalancing uses existing IDs; it does not add weapon-only effect IDs.
use crate::tables::{ModMetadata, ARMOR_MOD_TABLE, WEAPON_MOD_TABLE};
pub const ARMOR_EFFECTS: [ModMetadata; 4] = [
    ModMetadata {
        id: 0x11,
        code_name: "PE_FX_BARGAIN",
        name: "Bargain (mod)",
        description: "PE Enhancements economy mod: shop prices -15%",
        is_armor_mod: true,
        default_group_bits: 0x00,
    },
    ModMetadata {
        id: 0x12,
        code_name: "PE_FX_SCAVENGER",
        name: "Scavenger (mod)",
        description: "PE Enhancements economy mod: battle Junk +50%",
        is_armor_mod: true,
        default_group_bits: 0x00,
    },
    ModMetadata {
        id: 0x13,
        code_name: "PE_FX_LUCKY_FIND",
        name: "Lucky Find (mod)",
        description: "PE Enhancements economy mod: the rare-drop pity counter builds twice as fast",
        is_armor_mod: true,
        default_group_bits: 0x00,
    },
    ModMetadata {
        id: 0x14,
        code_name: "PE_FX_SALVAGER",
        name: "Salvager (mod)",
        description: "PE Enhancements economy mod: dismantle Junk +50% before the payout cap",
        is_armor_mod: true,
        default_group_bits: 0x00,
    },
];
// Existing weapon IDs with PE Enhancements-aware descriptions. The save encoding
// is identical to retail; only the running game's rules change behavior.
const WEAPON_EFFECTS: [ModMetadata; 5] = [
    ModMetadata {
        name: "Random Fire",
        description: "Random-target firing; behavior follows the running game's rules",
        ..WEAPON_MOD_TABLE[6]
    },
    ModMetadata {
        name: "Attack All",
        description: "Attacks all targets; PE Enhancements can cap crowd damage",
        ..WEAPON_MOD_TABLE[8]
    },
    ModMetadata {
        name: "Cyanide Bullets",
        description: "Chance of an instant kill against susceptible enemies",
        ..WEAPON_MOD_TABLE[11]
    },
    ModMetadata {
        name: "Quickdraw",
        description: "Chance to act first; PE Enhancements can add a chance bonus",
        ..WEAPON_MOD_TABLE[15]
    },
    ModMetadata {
        name: "Critical Up",
        description: "Increases critical chance; PE Enhancements uses the configured bonus",
        ..WEAPON_MOD_TABLE[18]
    },
];
pub fn metadata(id: u8, armor: bool) -> Option<&'static ModMetadata> {
    if armor {
        ARMOR_EFFECTS.iter().find(|m| m.id == id)
    } else {
        WEAPON_EFFECTS.iter().find(|m| m.id == id)
    }
}
pub fn choices(armor: bool) -> Vec<&'static ModMetadata> {
    (0..=31u8)
        .filter_map(|id| {
            if let Some(external) = crate::definitions::effect(id, armor) {
                return Some(external);
            }
            if armor {
                metadata(id, true)
                    .or_else(|| ARMOR_MOD_TABLE.get(id as usize).filter(|m| m.id != 16))
            } else {
                metadata(id, false).or_else(|| WEAPON_MOD_TABLE.get(id as usize))
            }
        })
        .collect()
}
pub fn note(armor: bool) -> &'static str {
    if armor {
        "Bargain, Scavenger, Lucky Find and Salvager require the PE Enhancements economy mod. Installing an effect here does not unlock it in the Catalog."
    } else {
        "PE Enhancements weapon effects use the original effect IDs. Their rebalanced behavior follows the game's Mod effects settings; this editor changes installed effects, not those settings or weapon-specific attack buffs."
    }
}

// These are always available as editor definitions, without loading game mods.
const ITEMS: [crate::tables::ItemMetadata; 2] = [
    crate::tables::ItemMetadata {
        code_name: "PE_PANACEA",
        display_name: "Panacea (mod)",
        description: "PE Enhancements: restores 400 HP and cures ailments; requires the mod",
        ..crate::tables::ITEM_TABLE[0x19]
    },
    crate::tables::ItemMetadata {
        code_name: "PE_SMOKE_GRENADE",
        display_name: "Smoke Grenade (mod)",
        description: "PE Enhancements: escapes an eligible ordinary fight; requires the mod",
        ..crate::tables::ITEM_TABLE[0x18]
    },
];
pub fn item(id: u8) -> Option<&'static crate::tables::ItemMetadata> {
    ITEMS.iter().find(|i| i.id == id)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn custom_effects_and_items_are_available_without_game_mods() {
        for (id, name) in [
            (17, "Bargain (mod)"),
            (18, "Scavenger (mod)"),
            (19, "Lucky Find (mod)"),
            (20, "Salvager (mod)"),
        ] {
            let meta = crate::tables::mod_metadata(id, true);
            assert_eq!(meta.name, name);
            assert_eq!(crate::tables::encode_mod_byte(id, true), id);
            assert!(choices(true).iter().any(|m| m.id == id));
        }
        assert!(metadata(20, true).unwrap().description.contains("+50%"));
        assert_eq!(
            crate::tables::item_metadata(0x18).display_name,
            "Smoke Grenade (mod)"
        );
        assert_eq!(
            crate::tables::item_metadata(0x19).display_name,
            "Panacea (mod)"
        );
        assert_eq!(choices(false).len(), 21);
    }
    #[test]
    fn editing_custom_effect_keeps_unknown_raw_effects_and_other_equipment() {
        use crate::{memcard::MemoryCardContainer, save::EquipmentRecord};
        let p = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("samples/Parasite_Eve_PS1_Sample_Card.srm");
        let mut card = MemoryCardContainer::open(&p).unwrap();
        let s = card.slots[0].as_mut().unwrap();
        let mut armor = EquipmentRecord::from_item_template(0, 0xc7);
        armor.tail_count = 4;
        armor.tail_capacity = 10;
        armor.tail_data[0] = 0xff;
        armor.tail_data[1] = 0xb5;
        armor.tail_data[2] = 0x09;
        armor.tail_data[10] = 0xda;
        let reserved = armor.tail_data;
        armor.set_mod_at(3, 20);
        s.equipment[0] = armor;
        let other = s.equipment[1].clone();
        let out = s.to_bytes();
        let r = crate::save::PeSaveSlot::from_bytes(&out, 0, &s.filename).unwrap();
        for i in [0, 1, 2, 10] {
            assert_eq!(r.equipment[0].tail_data[i], reserved[i]);
        }
        assert_eq!(r.equipment[0].mod_id_at(3), 20);
        assert_eq!(r.equipment[1].tail_data, other.tail_data);
        assert!(
            crate::save::PeSaveSlot::format_equipment_mods(&r.equipment[0])
                .contains("Unknown effect 31 (raw 0xFF)")
        );
        assert_eq!(
            crate::tables::mod_metadata(0xff, true).code_name,
            "MOD_UNKNOWN"
        );
    }
}

//! Optional, data-only editor definitions. Never loads a game mod or executes code.
//! One immutable pack per editor process; saved bytes remain authoritative.
use crate::tables::{ItemCategory, ItemMetadata, ModMetadata};
use anyhow::{ensure, Context, Result};
use serde::Deserialize;
use std::{collections::HashSet, path::Path, sync::OnceLock};

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Pack {
    pub format_version: u32,
    #[serde(default)]
    pub items: Vec<ItemDef>,
    #[serde(default)]
    pub effects: Vec<EffectDef>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ItemDef {
    pub id: u8,
    pub name: String,
    pub description: String,
    pub category: ItemCategory,
    /// Complete saved equipment template, required for a custom weapon/armor.
    pub equipment_template: Option<Vec<u8>>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct EffectDef {
    pub id: u8,
    pub name: String,
    pub description: String,
    pub armor: bool,
    /// Unshifted saved group ID, 0 means no exclusive group.
    pub group: u8,
}
struct Definitions {
    items: Vec<ItemMetadata>,
    templates: Vec<(u8, [u8; 32])>,
    effects: Vec<ModMetadata>,
}
static LOADED: OnceLock<Definitions> = OnceLock::new();
fn text_ok(s: &str) -> bool {
    !s.trim().is_empty() && s.len() <= 4096 && !s.chars().any(|c| c.is_control())
}
impl Pack {
    pub fn parse(bytes: &[u8]) -> Result<Self> {
        ensure!(
            bytes.len() <= 1024 * 1024,
            "Definition pack is larger than 1 MiB"
        );
        let p: Self = serde_json::from_slice(bytes).context("Invalid editor definition pack")?;
        ensure!(p.format_version == 1, "Unsupported definition pack version");
        ensure!(
            p.items.len() <= 255 && p.effects.len() <= 62,
            "Too many definitions"
        );
        let mut ids = HashSet::new();
        for i in &p.items {
            ensure!(
                i.id != 0 && ids.insert(i.id),
                "Duplicate/empty item ID {}",
                i.id
            );
            ensure!(
                text_ok(&i.name) && text_ok(&i.description),
                "Invalid item text"
            );
            ensure!(
                i.category != ItemCategory::Empty,
                "Custom item cannot have Empty category"
            );
            if i.category.is_equipment() {
                let b = i
                    .equipment_template
                    .as_ref()
                    .context("Custom equipment needs a complete 32-byte template")?;
                ensure!(
                    b.len() == 32,
                    "Equipment template must contain exactly 32 bytes"
                );
                let (icon, kind) = crate::tables::game_icon_and_kind(i.category).unwrap();
                ensure!(
                    b[4] == i.id && b[0] != 0 && b[6] == kind,
                    "Template ID or equipment class does not match definition"
                );
                if i.category.is_armor() {
                    ensure!((17..=22).contains(&b[0]), "Invalid armor icon class");
                } else {
                    ensure!(b[0] == icon, "Invalid weapon icon class");
                }
                ensure!(b[1] <= 10 && b[20] <= b[1], "Invalid equipment slot counts");
            } else {
                ensure!(
                    i.equipment_template.is_none(),
                    "Only equipment can supply a template"
                );
            }
        }
        let mut ids = HashSet::new();
        for e in &p.effects {
            ensure!(
                (1..=31).contains(&e.id) && e.group <= 7 && ids.insert((e.armor, e.id)),
                "Invalid or duplicate effect ID/group"
            );
            ensure!(
                text_ok(&e.name) && text_ok(&e.description),
                "Invalid effect text"
            );
        }
        Ok(p)
    }
}
fn keep(s: String) -> &'static str {
    Box::leak(s.into_boxed_str())
}
/// Called once before opening any save. Memory is bounded by the validated pack.
pub fn load(path: &Path) -> Result<()> {
    ensure!(
        LOADED.get().is_none(),
        "Definition pack already loaded; restart to change packs"
    );
    let size = std::fs::metadata(path)?.len();
    ensure!(size <= 1024 * 1024, "Definition pack is larger than 1 MiB");
    let p = Pack::parse(&std::fs::read(path)?)?;
    let mut d = Definitions {
        items: vec![],
        templates: vec![],
        effects: vec![],
    };
    for i in p.items {
        let base = crate::tables::ITEM_TABLE[i.id as usize];
        let mut m = ItemMetadata {
            id: i.id,
            code_name: "EDITOR_CUSTOM_ITEM",
            display_name: keep(i.name),
            description: keep(i.description),
            category: i.category,
            ..base
        };
        if let Some(b) = i.equipment_template {
            let b: [u8; 32] = b.try_into().unwrap();
            m.default_icon_id = b[0];
            m.default_kind = b[6];
            m.default_stats = [b[7], b[8], b[9]];
            m.default_slots = (b[20], b[1]);
            d.templates.push((i.id, b));
        }
        d.items.push(m);
    }
    for e in p.effects {
        d.effects.push(ModMetadata {
            id: e.id,
            code_name: "EDITOR_CUSTOM_EFFECT",
            name: keep(e.name),
            description: keep(e.description),
            is_armor_mod: e.armor,
            default_group_bits: e.group << 5,
        });
    }
    LOADED
        .set(d)
        .map_err(|_| anyhow::anyhow!("Definition pack already loaded"))?;
    Ok(())
}
pub fn item(id: u8) -> Option<&'static ItemMetadata> {
    LOADED.get()?.items.iter().find(|i| i.id == id)
}
pub fn effect(id: u8, armor: bool) -> Option<&'static ModMetadata> {
    LOADED
        .get()?
        .effects
        .iter()
        .find(|e| e.id == id && e.is_armor_mod == armor)
}
pub fn template(id: u8) -> Option<&'static [u8; 32]> {
    LOADED
        .get()?
        .templates
        .iter()
        .find(|(k, _)| *k == id)
        .map(|(_, b)| b)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn accepts_data_only_item_and_effect_definitions() {
        let p=Pack::parse(br#"{"format_version":1,"items":[{"id":25,"name":"Custom supply","description":"A supply","category":"Consumable"}],"effects":[{"id":31,"name":"Future effect","description":"Defined by another mod","armor":false,"group":0}]}"#).unwrap();
        assert_eq!(p.items.len(), 1);
        assert_eq!(p.effects.len(), 1);
    }
    #[test]
    fn refuses_ambiguous_or_unusable_definitions() {
        for s in [
            r#"{"format_version":2}"#,
            r#"{"format_version":1,"items":[{"id":27,"name":"Weapon","description":"Test","category":"Rifle"}]}"#,
            r#"{"format_version":1,"effects":[{"id":32,"name":"X","description":"Y","armor":true,"group":0}]}"#,
            r#"{"format_version":1,"effects":[{"id":1,"name":"X","description":"Y","armor":false,"group":0},{"id":1,"name":"X","description":"Y","armor":false,"group":0}]}"#,
        ] {
            assert!(Pack::parse(s.as_bytes()).is_err());
        }
    }
}

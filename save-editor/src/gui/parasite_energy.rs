//! Parasite Energy form.
use super::*;

pub(super) fn render_tab_parasite_energy_cards(
    ui: &mut egui::Ui,
    save: &mut PeSaveSlot,
    show_hidden: &mut bool,
) {
    ui.horizontal_wrapped(|ui| {
        ui.label(
            RichText::new("CLICK ANY SPELL CARD TO UNLOCK OR LOCK IT")
                .color(TEXT_MUTED)
                .small()
                .strong(),
        );
        ui.separator();
        if ui.button("✨ Unlock All Playable Spells").clicked() {
            save.apply_cheat_unlock_all_pe();
        }
        if ui.button("Clear All").clicked() {
            save.parasite_spell_flags = 0;
        }
        ui.checkbox(show_hidden, "Show Cut / Dummy Slots (Mito14..Mito18)");
    });
    ui.add_space(6.0);

    let visible_spells: Vec<_> = PARASITE_SPELLS
        .iter()
        .filter(|sp| *show_hidden || !sp.category.contains("Dummy"))
        .collect();

    egui::ScrollArea::vertical()
        .id_salt("pe_spell_cards_scroll")
        .auto_shrink([false, false])
        .show(ui, |ui| {
            let num_cols = if ui.available_width() >= 760.0 { 3 } else { 2 };
            for row_chunk in visible_spells.chunks(num_cols) {
                ui.columns(num_cols, |cols| {
                    for (col_idx, spell) in row_chunk.iter().enumerate() {
                        let unlocked = save.has_spell(spell.bit_index);
                        let resp = interactive_card(
                            &mut cols[col_idx],
                            ("pe_spell_tile", spell.bit_index),
                            unlocked,
                            if unlocked { Some(ACCENT_GREEN) } else { None },
                            |ui| {
                                ui.horizontal(|ui| {
                                    ui.label(
                                        RichText::new(if unlocked { "🧬" } else { "🔒" })
                                            .size(15.0),
                                    );
                                    if unlocked {
                                        badge(
                                            ui,
                                            "UNLOCKED",
                                            Color32::from_rgb(24, 65, 42),
                                            ACCENT_GREEN,
                                        );
                                    } else {
                                        badge(ui, "LOCKED", BG_DARKEST, TEXT_MUTED);
                                    }
                                    ui.label(
                                        RichText::new(spell.name).size(14.0).strong().color(
                                            if unlocked { Color32::WHITE } else { TEXT_MUTED },
                                        ),
                                    );
                                });
                                ui.label(
                                    RichText::new(format!(
                                        "{}  •  {}",
                                        spell.category, spell.pe_cost_note
                                    ))
                                    .color(TEXT_MUTED)
                                    .small(),
                                );
                            },
                        );

                        if resp.clicked() {
                            save.set_spell(spell.bit_index, !unlocked);
                        }
                    }
                });
                ui.add_space(4.0);
            }
        });
}

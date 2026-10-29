//! Memory-card slot browser.
use super::*;

impl SaveEditorApp {
    pub(super) fn show_slot_browser(&mut self, ctx: &egui::Context) {
        // 3. LEFT SIDEBAR: 15 INTERACTIVE MEMORY CARD SLOTS
        // =====================================================================
        egui::SidePanel::left("slots_sidebar")
            .resizable(true)
            .default_width(270.0)
            .min_width(235.0)
            .max_width(340.0)
            .frame(
                egui::Frame::NONE
                    .fill(BG_DARKEST)
                    .inner_margin(Margin::symmetric(10, 10))
                    .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE)),
            )
            .show(ctx, |ui| {
                ui.horizontal(|ui| {
                    ui.label(
                        RichText::new("MEMORY CARD SLOTS")
                            .color(TEXT_MUTED)
                            .small()
                            .strong(),
                    );
                    if let Some(c) = &self.container {
                        badge(
                            ui,
                            &format!("{}/15", c.active_slot_indices().len()),
                            BG_CARD,
                            ACCENT_GREEN,
                        );
                    }
                });
                ui.add_space(4.0);

                let Some(container) = &self.container else {
                    ui.label(RichText::new("No memory card loaded.").color(TEXT_MUTED));
                    return;
                };

                let mut clicked_slot: Option<usize> = None;

                egui::ScrollArea::vertical()
                    .id_salt("slots_sidebar_scroll")
                    .auto_shrink([false, false])
                    .show(ui, |ui| {
                        for i in 0..MAX_CARD_SLOTS {
                            let slot_letter = (b'A' + i as u8) as char;
                            let is_selected = self.selected_slot == i;

                            if let Some(save) = &container.slots[i] {
                                let accent = if save.is_clear_data {
                                    Some(ACCENT_AMBER)
                                } else {
                                    None
                                };
                                let resp = interactive_card(
                                    ui,
                                    ("mc_slot_card", i),
                                    is_selected,
                                    accent,
                                    |ui| {
                                        ui.horizontal(|ui| {
                                            if let Some(tex) = &self.slot_textures[i] {
                                                ui.image((tex.id(), Vec2::new(34.0, 34.0)));
                                            }
                                            ui.vertical(|ui| {
                                                ui.horizontal(|ui| {
                                                    ui.label(
                                                        RichText::new(format!(
                                                            "Slot {:02} ({})",
                                                            i + 1,
                                                            slot_letter
                                                        ))
                                                        .strong()
                                                        .size(13.0),
                                                    );
                                                    if save.is_clear_data {
                                                        badge(
                                                            ui,
                                                            "CLEAR",
                                                            Color32::from_rgb(75, 55, 18),
                                                            ACCENT_AMBER,
                                                        );
                                                    } else {
                                                        badge(
                                                            ui,
                                                            &format!(
                                                                "DAY {}",
                                                                save.play_time_class
                                                            ),
                                                            Color32::from_rgb(24, 52, 38),
                                                            ACCENT_GREEN,
                                                        );
                                                    }
                                                    badge(
                                                        ui,
                                                        &format!("Lv {}", save.display_level()),
                                                        Color32::from_rgb(28, 42, 56),
                                                        ACCENT_CYAN,
                                                    );
                                                });

                                                ui.label(
                                                    RichText::new(save.location_description())
                                                        .color(Color32::from_rgb(210, 225, 215))
                                                        .size(11.5),
                                                );

                                                ui.label(
                                                    RichText::new(format!(
                                                        "HP {}/{}  •  {}",
                                                        save.current_hp,
                                                        save.max_hp,
                                                        save.formatted_play_time()
                                                    ))
                                                    .color(TEXT_MUTED)
                                                    .small(),
                                                );
                                            });
                                        });
                                    },
                                );
                                if resp.clicked() {
                                    clicked_slot = Some(i);
                                }
                            } else {
                                let resp = interactive_card(
                                    ui,
                                    ("mc_slot_empty", i),
                                    is_selected,
                                    None,
                                    |ui| {
                                        ui.horizontal(|ui| {
                                            ui.label(
                                                RichText::new(format!(
                                                    "Slot {:02} ({}) — Empty",
                                                    i + 1,
                                                    slot_letter
                                                ))
                                                .color(TEXT_MUTED)
                                                .size(12.5),
                                            );
                                        });
                                    },
                                );
                                if resp.clicked() {
                                    clicked_slot = Some(i);
                                }
                            }
                            ui.add_space(3.0);
                        }
                    });

                if let Some(idx) = clicked_slot {
                    self.selected_slot = idx;
                }
            });

        // =====================================================================
    }
}

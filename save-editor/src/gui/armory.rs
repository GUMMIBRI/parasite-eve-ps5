//! Armory screen.
use super::*;

impl SaveEditorApp {
    // =========================================================================
    // TAB 4: ARMORY RECORDS (Right Inspector SidePanel + Central Record List)
    // =========================================================================
    pub(super) fn show_tab_armory(&mut self, ctx: &egui::Context) {
        egui::SidePanel::right("armory_inspector_panel")
            .resizable(true)
            .default_width(470.0)
            .min_width(370.0)
            .max_width(640.0)
            .frame(
                egui::Frame::NONE
                    .fill(BG_DARKEST)
                    .inner_margin(Margin::symmetric(14, 12))
                    .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE)),
            )
            .show(ctx, |ui| {
                let save = self.container.as_mut().unwrap().slots[self.selected_slot]
                    .as_mut()
                    .unwrap();
                let idx = self.selected_equip_index.min(127);
                let target_val = (0x100 + idx) as i16;

                ui.horizontal_wrapped(|ui| {
                    ui.heading(format!(
                        "Record E#{:02X} — {}",
                        idx,
                        save.equipment[idx].display_name()
                    ));
                    if !save.inventory_items.contains(&target_val)
                        && ui.button("🎒 Put in Aya's Bag").clicked()
                    {
                        if let Some(free_s) = save.inventory_items.iter().position(|&v| v == 0) {
                            save.inventory_items[free_s] = target_val;
                        }
                    }
                });
                ui.separator();

                egui::ScrollArea::vertical()
                    .id_salt("armory_detail_scroll")
                    .auto_shrink([false, false])
                    .show(ui, |ui| {
                        render_equipment_record_editor(ui, &mut save.equipment[idx], 1000 + idx);
                    });
            });

        egui::CentralPanel::default()
            .frame(
                egui::Frame::NONE
                    .fill(BG_PANEL)
                    .inner_margin(Margin::symmetric(14, 10)),
            )
            .show(ctx, |ui| {
                let save = self.container.as_mut().unwrap().slots[self.selected_slot]
                    .as_mut()
                    .unwrap();

                ui.horizontal(|ui| {
                    ui.label(
                        RichText::new("EQUIPMENT RECORDS (0x100..0x17F)")
                            .color(TEXT_MUTED)
                            .small()
                            .strong(),
                    );
                    ui.checkbox(&mut self.show_only_active_equip, "Show Only Active Records");
                });
                ui.add_space(4.0);

                egui::ScrollArea::vertical()
                    .id_salt("armory_records_scroll")
                    .auto_shrink([false, false])
                    .show(ui, |ui| {
                        for i in 0..128 {
                            let rec = &save.equipment[i];
                            if self.show_only_active_equip
                                && !rec.is_active()
                                && self.selected_equip_index != i
                            {
                                continue;
                            }
                            let target_val = (0x100 + i) as i16;
                            let in_aya_bag =
                                save.inventory_items.iter().position(|&v| v == target_val);
                            let in_wayne =
                                save.special_storage.iter().position(|&v| v == target_val);

                            let owner_badge = if let Some(slot) = in_aya_bag {
                                if save.equipped_weapon_slot == slot as i8
                                    || save.equipped_armor_slot == slot as i8
                                {
                                    Some(("EQUIPPED", ACCENT_GREEN))
                                } else {
                                    Some(("AYA BAG", ACCENT_CYAN))
                                }
                            } else if in_wayne.is_some() {
                                Some(("WAYNE VAULT", ACCENT_AMBER))
                            } else {
                                None
                            };

                            let resp = interactive_card(
                                ui,
                                ("armory_rec_card", i),
                                self.selected_equip_index == i,
                                owner_badge.map(|(_, c)| c),
                                |ui| {
                                    ui.horizontal(|ui| {
                                        ui.label(
                                            RichText::new(format!("E#{:02X}", i))
                                                .monospace()
                                                .small()
                                                .color(TEXT_MUTED),
                                        );
                                        if let Some((tag, col)) = owner_badge {
                                            badge(ui, tag, BG_DARKEST, col);
                                        }
                                        if rec.is_active() {
                                            ui.label(RichText::new(if rec.is_armor() {
                                                "🛡"
                                            } else {
                                                "🔫"
                                            }));
                                            ui.label(
                                                RichText::new(rec.display_name())
                                                    .strong()
                                                    .color(Color32::WHITE),
                                            );
                                            if rec.is_engraved() {
                                                badge(
                                                    ui,
                                                    "★ WAYNE",
                                                    Color32::from_rgb(75, 55, 18),
                                                    ACCENT_AMBER,
                                                );
                                            }
                                        } else {
                                            ui.label(
                                                RichText::new("Free Record Slot").color(TEXT_MUTED),
                                            );
                                        }
                                    });

                                    if rec.is_active() {
                                        let eff = rec.effective_stats();
                                        ui.label(
                                            RichText::new(format!(
                                                "Stats: {}/{}/{}  •  Mods: {}",
                                                eff[0],
                                                eff[1],
                                                eff[2],
                                                PeSaveSlot::format_equipment_mods(rec)
                                            ))
                                            .color(TEXT_MUTED)
                                            .small(),
                                        );
                                    }
                                },
                            );

                            if resp.clicked() {
                                self.selected_equip_index = i;
                            }
                            ui.add_space(2.0);
                        }
                    });
            });
    }
}

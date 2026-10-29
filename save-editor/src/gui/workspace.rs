//! Workspace summary, navigation and screen dispatch.
use super::*;

impl SaveEditorApp {
    pub(super) fn show_workspace_header(&mut self, ctx: &egui::Context) {
        // 5. HERO SUMMARY HEADER + PRIMARY NAVIGATION BAR (Top Panel of Workspace)
        // =====================================================================
        let mut status_toast: Option<&'static str> = None;
        let slot_tex = self.slot_textures[self.selected_slot].clone();

        egui::TopBottomPanel::top("workspace_hero_and_tabs")
            .frame(
                egui::Frame::NONE
                    .fill(BG_PANEL)
                    .inner_margin(Margin::symmetric(14, 10))
                    .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE)),
            )
            .show(ctx, |ui| {
                let save = self.container.as_mut().unwrap().slots[self.selected_slot]
                    .as_mut()
                    .unwrap();

                // Hero Card (2 clean wrapped rows — never overlaps!)
                egui::Frame::NONE
                    .fill(BG_DARKEST)
                    .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE))
                    .corner_radius(CornerRadius::same(8))
                    .inner_margin(Margin::symmetric(12, 8))
                    .show(ui, |ui| {
                        // Row 1: Icon + Character Identity + Location + Play Time
                        ui.horizontal_wrapped(|ui| {
                            if let Some(tex) = &slot_tex {
                                ui.image((tex.id(), Vec2::new(36.0, 36.0)));
                            }
                            ui.label(
                                RichText::new(&save.aya_name)
                                    .size(17.5)
                                    .strong()
                                    .color(Color32::WHITE),
                            );
                            badge(
                                ui,
                                &format!("Lv {}", save.display_level()),
                                Color32::from_rgb(28, 52, 70),
                                ACCENT_CYAN,
                            );
                            if save.is_clear_data {
                                badge(
                                    ui,
                                    &format!("EX ROUND {}", save.new_game_count + 1),
                                    Color32::from_rgb(75, 55, 18),
                                    ACCENT_AMBER,
                                );
                            } else {
                                badge(
                                    ui,
                                    &format!("DAY {}", save.play_time_class),
                                    Color32::from_rgb(24, 52, 38),
                                    ACCENT_GREEN,
                                );
                            }

                            ui.separator();
                            ui.label(
                                RichText::new(format!(
                                    "📍 {} (Map {})",
                                    save.location_description(),
                                    save.current_map_number
                                ))
                                .color(Color32::from_rgb(210, 230, 215))
                                .strong(),
                            );
                            ui.separator();
                            ui.label(
                                RichText::new(format!("⏱ {}", save.formatted_play_time()))
                                    .color(TEXT_MUTED),
                            );
                            ui.separator();
                            ui.label(
                                RichText::new(format!("CRC 0x{:04X}", save.stored_crc))
                                    .color(TEXT_MUTED)
                                    .monospace()
                                    .small(),
                            );
                        });

                        ui.add_space(4.0);

                        // Row 2: HP Bar + BP / EXP / Bag + Clickable Equipped Weapon & Armor Pills
                        ui.horizontal_wrapped(|ui| {
                            let hp_ratio = if save.max_hp > 0 {
                                (save.current_hp as f32 / save.max_hp as f32).clamp(0.0, 1.0)
                            } else {
                                0.0
                            };
                            ui.label(RichText::new("HP").color(TEXT_MUTED).small().strong());
                            ui.add(
                                egui::ProgressBar::new(hp_ratio)
                                    .desired_width(130.0)
                                    .fill(Color32::from_rgb(45, 165, 95))
                                    .text(format!("{} / {}", save.current_hp, save.max_hp)),
                            );
                            if save.current_hp < save.max_hp && ui.small_button("Heal").clicked() {
                                save.current_hp = save.max_hp;
                                let _ = save.to_bytes();
                            }

                            let used_slots = save
                                .inventory_items
                                .iter()
                                .take(save.inventory_slot_count as usize)
                                .filter(|&&v| v != 0)
                                .count();
                            badge(
                                ui,
                                &format!("BP: {}", save.bonus_points),
                                BG_CARD,
                                ACCENT_AMBER,
                            );
                            badge(
                                ui,
                                &format!("EXP: {}", save.total_exp),
                                BG_CARD,
                                Color32::WHITE,
                            );
                            badge(
                                ui,
                                &format!("Bag: {}/{}", used_slots, save.inventory_slot_count),
                                BG_CARD,
                                ACCENT_GREEN,
                            );

                            ui.separator();

                            // Clickable Equipped Weapon Button
                            let w_slot = save.equipped_weapon_slot;
                            let w_val = if (0..50).contains(&w_slot) {
                                save.inventory_items[w_slot as usize]
                            } else {
                                0
                            };
                            let (_, w_title, _, _) = slot_value_icon_and_title(save, w_val);
                            if ui
                                .button(
                                    RichText::new(format!("🔫 Weapon: {}", w_title))
                                        .color(ACCENT_GREEN)
                                        .strong(),
                                )
                                .on_hover_text("Click to inspect & edit equipped weapon")
                                .clicked()
                                && (0..50).contains(&w_slot)
                            {
                                self.selected_inv_slot = w_slot as usize;
                                self.inv_replace_mode = false;
                                self.active_tab = EditorTab::InventoryAndLoadout;
                            }

                            // Clickable Equipped Armor Button
                            let a_slot = save.equipped_armor_slot;
                            let a_val = if (0..50).contains(&a_slot) {
                                save.inventory_items[a_slot as usize]
                            } else {
                                0
                            };
                            let (_, a_title, _, _) = slot_value_icon_and_title(save, a_val);
                            if ui
                                .button(
                                    RichText::new(format!("🛡 Armor: {}", a_title))
                                        .color(ACCENT_CYAN)
                                        .strong(),
                                )
                                .on_hover_text("Click to inspect & edit equipped armor")
                                .clicked()
                                && (0..50).contains(&a_slot)
                            {
                                self.selected_inv_slot = a_slot as usize;
                                self.inv_replace_mode = false;
                                self.active_tab = EditorTab::InventoryAndLoadout;
                            }
                        });
                    });

                ui.add_space(6.0);

                // Navigation Tabs Row + Quick Cheats Dropdown
                ui.horizontal_wrapped(|ui| {
                    for (tab, label) in [
                        (EditorTab::InventoryAndLoadout, "🎒 Inventory & Loadout"),
                        (EditorTab::CharacterAndStats, "👩 Character & Stats"),
                        (EditorTab::ParasiteEnergy, "🧬 Parasite Energy"),
                        (EditorTab::ArmoryRecords, "🔫 All Equipment (128)"),
                        (EditorTab::WayneVault, "🏛 Wayne's Storage"),
                        (EditorTab::StoryAndTeleport, "🗺 Story & Teleport"),
                        (EditorTab::ModSaveData, "Mod Save Data"),
                    ] {
                        let is_active = self.active_tab == tab;
                        let btn =
                            egui::Button::new(RichText::new(label).size(13.0).strong().color(
                                if is_active {
                                    ACCENT_GREEN
                                } else {
                                    Color32::from_rgb(200, 215, 205)
                                },
                            ))
                            .fill(if is_active {
                                BG_CARD_SELECTED
                            } else {
                                BG_DARKEST
                            })
                            .stroke(Stroke::new(
                                if is_active { 1.5_f32 } else { 1.0_f32 },
                                if is_active {
                                    BORDER_ACTIVE
                                } else {
                                    BORDER_SUBTLE
                                },
                            ))
                            .corner_radius(CornerRadius::same(6));

                        if ui.add(btn).clicked() {
                            self.active_tab = tab;
                        }
                    }

                    ui.separator();

                    ui.menu_button(
                        RichText::new("⚡ Quick Cheats & Presets")
                            .color(ACCENT_AMBER)
                            .strong(),
                        |ui| {
                            ui.set_min_width(270.0);
                            if ui.button("Repair Equipment Icons & Models").clicked() {
                                save.repair_equipment_types();
                                let _ = save.to_bytes();
                                status_toast =
                                    Some("Repaired equipment types; upgrades preserved.");
                                ui.close_menu();
                            }
                            if ui
                                .button("✨ Max Aya (Lv 99 • Max HP • 99,999 BP • 46 Slots)")
                                .on_hover_text(MAX_HP_HELP)
                                .clicked()
                            {
                                save.apply_cheat_max_aya();
                                let _ = save.to_bytes();
                                status_toast = Some("Applied Max Aya stats.");
                                ui.close_menu();
                            }
                            if ui
                                .button("🧬 Unlock All Playable Parasite Energy Spells")
                                .clicked()
                            {
                                save.apply_cheat_unlock_all_pe();
                                let _ = save.to_bytes();
                                status_toast = Some("Unlocked all playable PE spells.");
                                ui.close_menu();
                            }
                            if ui
                                .button("弹 Max All Ammo Reserves (999) & Refill Guns")
                                .clicked()
                            {
                                save.apply_cheat_max_ammo();
                                let _ = save.to_bytes();
                                status_toast = Some("Maxed all ammo pools to 999.");
                                ui.close_menu();
                            }
                            if ui
                                .button("🔫 Upgrade All Owned Weapons & Armor (+999 / 10 Slots)")
                                .clicked()
                            {
                                save.apply_cheat_max_equipped_gear();
                                let _ = save.to_bytes();
                                status_toast = Some("Upgraded all owned weapons & armor to +999.");
                                ui.close_menu();
                            }
                            ui.separator();
                            if ui
                                .button(
                                    "🏆 Equip Ultimate Loadout (P90 + Cm Suit 2 + Super Tool Kit)",
                                )
                                .on_hover_text(
                                    "Adds an ultimate P90 and Cm Suit 2 and puts them on (the old \
                                     gun and armor stay in the bag), plus tool kits and medicine. \
                                     The suit's HP Max Up lets Max Aya reach 999 HP.",
                                )
                                .clicked()
                            {
                                if save.apply_cheat_give_ultimate_loadout().is_ok() {
                                    let _ = save.to_bytes();
                                    status_toast = Some(
                                        "Equipped Ultimate P90 & Cm Suit 2; added Super Tool Kit.",
                                    );
                                }
                                ui.close_menu();
                            }
                        },
                    );
                });
            });

        if let Some(msg) = status_toast {
            self.set_status(msg, false);
        }

        // =====================================================================
    }
    pub(super) fn show_active_tab(&mut self, ctx: &egui::Context) {
        // 6. TAB BODY (Uses dedicated SidePanel::right for Master-Detail splits
        //    so Left List and Right Inspector NEVER overlap!)
        // =====================================================================
        match self.active_tab {
            EditorTab::InventoryAndLoadout => {
                self.show_tab_inventory(ctx);
            }
            EditorTab::CharacterAndStats => {
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
                        egui::ScrollArea::vertical()
                            .id_salt("char_stats_scroll")
                            .auto_shrink([false, false])
                            .show(ui, |ui| {
                                render_tab_character_and_stats(ui, save);
                            });
                    });
            }
            EditorTab::ParasiteEnergy => {
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
                        render_tab_parasite_energy_cards(ui, save, &mut self.show_hidden_pe_spells);
                    });
            }
            EditorTab::ArmoryRecords => {
                self.show_tab_armory(ctx);
            }
            EditorTab::WayneVault => {
                self.show_tab_wayne(ctx);
            }
            EditorTab::StoryAndTeleport => {
                self.show_tab_story_and_teleport(ctx);
            }
            EditorTab::ModSaveData => {
                egui::CentralPanel::default().show(ctx, |ui| {
                    let save = self.container.as_mut().unwrap().slots[self.selected_slot]
                        .as_mut()
                        .unwrap();
                    egui::ScrollArea::vertical().show(ui, |ui| super::mod_data::show(ui, save));
                });
            }
        }
    }
}

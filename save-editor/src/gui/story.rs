//! Story screen.
use super::*;

impl SaveEditorApp {
    // =========================================================================
    // TAB 6: STORY & TELEPORT (Right Teleport Cards + Central Story Controls)
    // =========================================================================
    pub(super) fn show_tab_story_and_teleport(&mut self, ctx: &egui::Context) {
        egui::SidePanel::right("story_teleport_panel")
            .resizable(true)
            .default_width(470.0)
            .min_width(360.0)
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

                ui.label(
                    RichText::new("CLICK ANY LOCATION CARD TO TELEPORT AYA")
                        .color(TEXT_MUTED)
                        .small()
                        .strong(),
                );
                ui.add_space(4.0);

                egui::ScrollArea::vertical()
                    .id_salt("teleport_cards_scroll")
                    .auto_shrink([false, false])
                    .show(ui, |ui| {
                        for &(map_id, day_id, desc) in MAP_TELEPORT_PRESETS.iter() {
                            let is_current = save.current_map_number == map_id;
                            let resp = interactive_card(
                                ui,
                                ("teleport_preset_card", map_id),
                                is_current,
                                if is_current { Some(ACCENT_GREEN) } else { None },
                                |ui| {
                                    ui.horizontal(|ui| {
                                        badge(
                                            ui,
                                            &format!("DAY {}", day_id),
                                            BG_DARKEST,
                                            ACCENT_GREEN,
                                        );
                                        badge(
                                            ui,
                                            &format!("Map {:03}", map_id),
                                            BG_DARKEST,
                                            ACCENT_CYAN,
                                        );
                                        ui.label(
                                            RichText::new(desc).strong().color(Color32::WHITE),
                                        );
                                    });
                                },
                            );

                            if resp.clicked() {
                                save.current_map_number = map_id;
                                save.play_time_class = day_id;
                                save.current_story_day = day_id as i8;
                            }
                            ui.add_space(3.0);
                        }
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

                egui::ScrollArea::vertical()
                    .id_salt("story_controls_scroll")
                    .auto_shrink([false, false])
                    .show(ui, |ui| {
                        egui::Frame::NONE
                            .fill(BG_DARKEST)
                            .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE))
                            .corner_radius(CornerRadius::same(8))
                            .inner_margin(Margin::symmetric(14, 12))
                            .show(ui, |ui| {
                                ui.label(
                                    RichText::new("CURRENT STORY DAY & MAP NUMBER")
                                        .color(TEXT_MUTED)
                                        .small()
                                        .strong(),
                                );
                                ui.separator();

                                egui::Grid::new("story_day_map_grid")
                                    .num_columns(2)
                                    .spacing([14.0, 10.0])
                                    .show(ui, |ui| {
                                        ui.label("Save Header Day (1..6):");
                                        ui.add(
                                            egui::Slider::new(&mut save.play_time_class, 1..=6),
                                        );
                                        ui.end_row();

                                        ui.label("Engine Story Chapter (1..8):");
                                        ui.add(
                                            egui::Slider::new(&mut save.current_story_day, 1..=8),
                                        );
                                        ui.end_row();

                                        ui.label("Room / Map ID (1..437):");
                                        ui.add(
                                            egui::Slider::new(
                                                &mut save.current_map_number,
                                                1..=437,
                                            ),
                                        );
                                        ui.end_row();

                                        ui.label("Location Name:");
                                        ui.label(
                                            RichText::new(map_location_descriptive(
                                                save.current_map_number,
                                            ))
                                            .color(ACCENT_GREEN)
                                            .strong(),
                                        );
                                        ui.end_row();
                                    });
                            });

                        ui.add_space(8.0);

                        egui::Frame::NONE
                            .fill(BG_DARKEST)
                            .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE))
                            .corner_radius(CornerRadius::same(8))
                            .inner_margin(Margin::symmetric(14, 12))
                            .show(ui, |ui| {
                                ui.horizontal_wrapped(|ui| {
                                    ui.label(
                                        RichText::new(
                                            "ADVANCED: SCRIPT VARIABLE BANK & CRC-16",
                                        )
                                        .color(TEXT_MUTED)
                                        .small()
                                        .strong(),
                                    );
                                    ui.checkbox(
                                        &mut self.show_script_hex_inspector,
                                        "Show Hex Inspector",
                                    );
                                });
                                ui.separator();
                                ui.label(
                                    RichText::new(format!(
                                        "CRC-16 Checksum at 0x1C8C: Stored 0x{:04X} | Calculated 0x{:04X}",
                                        save.stored_crc, save.calculated_crc
                                    ))
                                    .color(ACCENT_GREEN),
                                );

                                if self.show_script_hex_inspector {
                                    ui.add_space(6.0);
                                    ui.horizontal(|ui| {
                                        ui.label("Script Bank Offset (0x000..0x7F0):");
                                        ui.add(
                                            egui::Slider::new(
                                                &mut self.script_inspector_offset,
                                                0..=(SCRIPT_VAR_SIZE - 16),
                                            )
                                            .hexadecimal(3, false, true),
                                        );
                                    });
                                    let base_off = (self.script_inspector_offset / 8) * 8;
                                    egui::Grid::new("script_hex_grid")
                                        .striped(true)
                                        .num_columns(9)
                                        .spacing([6.0, 4.0])
                                        .show(ui, |ui| {
                                            for row in 0..8 {
                                                let row_off = base_off + row * 8;
                                                if row_off >= SCRIPT_VAR_SIZE {
                                                    break;
                                                }
                                                ui.label(
                                                    RichText::new(format!("0x{:03X}", row_off))
                                                        .monospace()
                                                        .color(TEXT_MUTED),
                                                );
                                                for col in 0..8 {
                                                    let off = row_off + col;
                                                    let mut b = save.read_script_u8(off);
                                                    if ui
                                                        .add(
                                                            egui::DragValue::new(&mut b)
                                                                .range(0..=255)
                                                                .hexadecimal(2, false, false),
                                                        )
                                                        .changed()
                                                    {
                                                        save.write_script_u8(off, b);
                                                    }
                                                }
                                                ui.end_row();
                                            }
                                        });
                                }
                            });
                    });
            });
    }
}

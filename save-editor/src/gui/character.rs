//! Character and stat form.
use super::*;

pub(super) fn render_tab_character_and_stats(ui: &mut egui::Ui, save: &mut PeSaveSlot) {
    ui.columns(2, |cols| {
        cols[0].vertical(|ui| {
            egui::Frame::NONE
                .fill(BG_DARKEST)
                .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE))
                .corner_radius(CornerRadius::same(8))
                .inner_margin(Margin::symmetric(14, 12))
                .show(ui, |ui| {
                    ui.label(
                        RichText::new("CHARACTER IDENTITY & VITAL ATTRIBUTES")
                            .color(TEXT_MUTED)
                            .small()
                            .strong(),
                    );
                    ui.separator();

                    egui::Grid::new("char_vital_grid")
                        .num_columns(2)
                        .spacing([16.0, 10.0])
                        .show(ui, |ui| {
                            ui.label("Character Name:");
                            ui.text_edit_singleline(&mut save.aya_name);
                            ui.end_row();

                            ui.label("Level (1..99):");
                            let mut lvl = save.display_level();
                            if ui.add(egui::Slider::new(&mut lvl, 1..=99)).changed() {
                                save.set_display_level(lvl);
                            }
                            ui.end_row();

                            ui.label("Current HP:");
                            ui.add(egui::Slider::new(&mut save.current_hp, 1..=999));
                            ui.end_row();

                            ui.label("Maximum HP:");
                            ui.add(egui::Slider::new(&mut save.max_hp, 1..=999))
                                .on_hover_text(MAX_HP_HELP);
                            ui.end_row();

                            ui.label("Total Experience (EXP):");
                            ui.add(
                                egui::DragValue::new(&mut save.total_exp)
                                    .range(0..=99_999_999)
                                    .speed(250),
                            );
                            ui.end_row();

                            ui.label("Bonus Points (BP):");
                            ui.add(
                                egui::DragValue::new(&mut save.bonus_points)
                                    .range(0..=99_999)
                                    .speed(50),
                            );
                            ui.end_row();

                            ui.label("Base Bag Capacity (armor can add up to 4):");
                            ui.add(
                                egui::Slider::new(
                                    &mut save.inventory_slot_count,
                                    10..=crate::disc::MAX_STAT_BAG_SLOTS,
                                )
                                .suffix(" slots"),
                            )
                            .on_hover_text(BAG_SIZE_HELP);
                            ui.end_row();

                            ui.label("PE Energy Bonus Pool:");
                            ui.add(egui::Slider::new(&mut save.pe_bonus_pool, 0..=999));
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
                    ui.label(
                        RichText::new("PLAY TIME, EX ROUND & UI WINDOW THEME")
                            .color(TEXT_MUTED)
                            .small()
                            .strong(),
                    );
                    ui.separator();

                    let total_s = save.play_time_seconds.max(0);
                    let mut hrs = total_s / 3600;
                    let mut mins = (total_s % 3600) / 60;
                    let mut secs = total_s % 60;
                    ui.horizontal_wrapped(|ui| {
                        ui.label("Play Time:");
                        let mut changed = false;
                        changed |= ui
                            .add(egui::DragValue::new(&mut hrs).range(0..=999).suffix(" h"))
                            .changed();
                        changed |= ui
                            .add(egui::DragValue::new(&mut mins).range(0..=59).suffix(" m"))
                            .changed();
                        changed |= ui
                            .add(egui::DragValue::new(&mut secs).range(0..=59).suffix(" s"))
                            .changed();
                        if changed {
                            save.set_play_time_hms(hrs, mins, secs);
                        }
                    });

                    ui.horizontal_wrapped(|ui| {
                        ui.label("EX Playthrough Round (0 = 1st Game):");
                        ui.add(egui::Slider::new(&mut save.new_game_count, 0..=98));
                    });

                    ui.checkbox(
                        &mut save.is_clear_data,
                        "Mark as Clear Data / EX Game Save (Starts New EX Round on Load)",
                    );

                    let mut rgb = [
                        (save.draw_blend_color & 0xFF) as u8,
                        ((save.draw_blend_color >> 8) & 0xFF) as u8,
                        ((save.draw_blend_color >> 16) & 0xFF) as u8,
                    ];
                    ui.horizontal(|ui| {
                        ui.label("In-Game Menu Window Tint:");
                        if ui.color_edit_button_srgb(&mut rgb).changed() {
                            save.draw_blend_color = (rgb[0] as u32)
                                | ((rgb[1] as u32) << 8)
                                | ((rgb[2] as u32) << 16);
                        }
                    });
                });
        });

        cols[1].vertical(|ui| {
            egui::Frame::NONE
                .fill(BG_DARKEST)
                .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE))
                .corner_radius(CornerRadius::same(8))
                .inner_margin(Margin::symmetric(14, 12))
                .show(ui, |ui| {
                    ui.horizontal_wrapped(|ui| {
                        ui.label(
                            RichText::new("STAT GROWTH ALLOCATIONS (0..1000)")
                                .color(TEXT_MUTED)
                                .small()
                                .strong(),
                        );
                        if ui.small_button("Max All to 1000").clicked() {
                            save.stats_levels = [1000; 7];
                        }
                    });
                    ui.separator();
                    ui.label(
                        RichText::new("Combat stats (Offense, Defense, PE, ATB Speed, Recovery) are computed from these 7 growth levels (base = Level × 10, max = 1000).")
                            .color(TEXT_MUTED)
                            .small(),
                    );
                    ui.add_space(8.0);

                    egui::Grid::new("stat_growth_sliders_grid")
                        .striped(true)
                        .num_columns(2)
                        .spacing([14.0, 8.0])
                        .show(ui, |ui| {
                            for i in 0..7 {
                                ui.label(RichText::new(STAT_GROWTH_NAMES[i]).strong());
                                ui.add(
                                    egui::Slider::new(&mut save.stats_levels[i], 0..=1000)
                                        .show_value(true),
                                );
                                ui.end_row();
                            }
                        });
                });
        });
    });
}

//! Application menu and status bar.
use super::*;

impl SaveEditorApp {
    pub(super) fn show_application_bar(&mut self, ctx: &egui::Context) {
        // 1. TOP APPLICATION BAR
        // =====================================================================
        egui::TopBottomPanel::top("top_app_bar")
            .frame(
                egui::Frame::NONE
                    .fill(BG_DARKEST)
                    .inner_margin(Margin::symmetric(14, 8))
                    .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE)),
            )
            .show(ctx, |ui| {
                ui.horizontal_wrapped(|ui| {
                    ui.label(
                        RichText::new("🧬 PARASITE EVE")
                            .color(ACCENT_GREEN)
                            .size(16.5)
                            .strong(),
                    );
                    ui.label(RichText::new("SAVE EDITOR").color(TEXT_MUTED).size(12.5));
                    ui.separator();

                    if ui.button("📂 Open PsyZ (bu00)").clicked() {
                        if let Some(folder) = rfd::FileDialog::new()
                            .set_title("Select PsyZ Memory Card Folder (bu00 or bu10)")
                            .pick_folder()
                        {
                            self.load_path(ctx, folder);
                        }
                    }

                    if ui.button("💾 Open PS1 Card (.srm/.mcr)").clicked() {
                        if let Some(file) = rfd::FileDialog::new()
                            .set_title("Open PS1 Memory Card or Save Slot")
                            .add_filter(
                                "PS1 & PsyZ Saves",
                                &["srm", "mcr", "mcd", "gme", "mcs", "bin"],
                            )
                            .add_filter("All Files", &["*"])
                            .pick_file()
                        {
                            self.load_path(ctx, file);
                        }
                    }

                    ui.menu_button("⚡ Quick Open", |ui| {
                        for (label, rel_path) in [
                            ("PsyZ Card 1 (./bu00)", "bu00"),
                            ("PsyZ Card 1 (../bu00)", "../bu00"),
                            ("PsyZ Card 2 (./bu10)", "bu10"),
                            ("PsyZ Card 2 (../bu10)", "../bu10"),
                            (
                                "Sample PS1 Card (7 Saves)",
                                "samples/Parasite_Eve_PS1_Sample_Card.srm",
                            ),
                            (
                                "Sample PS1 Card (save-editor/samples)",
                                "save-editor/samples/Parasite_Eve_PS1_Sample_Card.srm",
                            ),
                        ] {
                            let p = PathBuf::from(rel_path);
                            if p.exists() && ui.button(label).clicked() {
                                self.load_path(ctx, p);
                                ui.close_menu();
                            }
                        }
                    });

                    ui.menu_button("⇄ Convert / Export", |ui| {
                        let has_container = self.container.is_some();
                        if ui
                            .add_enabled(
                                has_container,
                                egui::Button::new("📤 Export All Slots to PsyZ Folder (bu00/)"),
                            )
                            .clicked()
                        {
                            ui.close_menu();
                            if let Some(target_dir) = rfd::FileDialog::new()
                                .set_title("Select Target PsyZ Directory (e.g. bu00)")
                                .pick_folder()
                            {
                                if let Some(container) = &mut self.container {
                                    match container.export_to_psyz_dir(&target_dir) {
                                        Ok(files) => self.set_status(
                                            format!(
                                                "Exported {} slot(s) to PsyZ folder {}.",
                                                files.len(),
                                                target_dir.display()
                                            ),
                                            false,
                                        ),
                                        Err(e) => {
                                            self.set_status(format!("Export error: {:#}", e), true)
                                        }
                                    }
                                }
                            }
                        }

                        if ui
                            .add_enabled(
                                has_container,
                                egui::Button::new("💿 Export All Slots to PS1 Card (.srm / .mcr)"),
                            )
                            .clicked()
                        {
                            ui.close_menu();
                            if let Some(target_file) = rfd::FileDialog::new()
                                .set_title("Export 128 KB PS1 Memory Card (.srm / .mcr)")
                                .set_file_name("ParasiteEve_Card.srm")
                                .add_filter("PS1 Memory Card", &["srm", "mcr", "mcd"])
                                .save_file()
                            {
                                if let Some(container) = &mut self.container {
                                    match container.export_to_ps1_card_image(&target_file) {
                                        Ok(()) => self.set_status(
                                            format!(
                                                "Exported PS1 card image to {}.",
                                                target_file.display()
                                            ),
                                            false,
                                        ),
                                        Err(e) => {
                                            self.set_status(format!("Export error: {:#}", e), true)
                                        }
                                    }
                                }
                            }
                        }
                    });

                    let game = crate::disc::game_data();
                    let disc_btn = ui
                        .button(if game.is_some() {
                            "💿 Game Disc ✔"
                        } else {
                            "💿 Game Disc…"
                        })
                        .on_hover_text(match &game {
                            Some(g) => format!(
                                "Item records and the bag table come from {}.",
                                g.source.display()
                            ),
                            None => {
                                "Verified USA equipment templates are available without a disc. \
                                     Pick a USA disc for exact HP growth calculations."
                                    .to_string()
                            }
                        });
                    if disc_btn.clicked() {
                        if let Some(file) = rfd::FileDialog::new()
                            .set_title("Select Parasite Eve (USA) Disc Image")
                            .add_filter("Disc Images", &["cue", "bin", "iso", "img"])
                            .add_filter("All Files", &["*"])
                            .pick_file()
                        {
                            match crate::disc::load_disc(&file) {
                                Ok(g) => self.set_status(
                                    format!("Game data loaded from {}.", g.source.display()),
                                    false,
                                ),
                                Err(e) => self.set_status(format!("{:#}", e), true),
                            }
                        }
                    }

                    ui.separator();

                    let has_container = self.container.is_some();
                    let save_btn = egui::Button::new(
                        RichText::new("💾 Save Changes (Cmd+S)")
                            .color(Color32::from_rgb(10, 20, 14))
                            .strong(),
                    )
                    .fill(ACCENT_GREEN)
                    .corner_radius(CornerRadius::same(6));

                    if ui.add_enabled(has_container, save_btn).clicked() {
                        self.save_current_container();
                    }
                });
            });

        // =====================================================================
    }
    pub(super) fn show_status_bar(&mut self, ctx: &egui::Context) {
        // 2. BOTTOM STATUS BAR
        // =====================================================================
        egui::TopBottomPanel::bottom("bottom_status_bar")
            .frame(
                egui::Frame::NONE
                    .fill(BG_DARKEST)
                    .inner_margin(Margin::symmetric(14, 5))
                    .stroke(Stroke::new(1.0_f32, BORDER_SUBTLE)),
            )
            .show(ctx, |ui| {
                ui.horizontal(|ui| {
                    let color = if self.is_error_status {
                        Color32::from_rgb(255, 110, 110)
                    } else {
                        ACCENT_GREEN
                    };
                    ui.label(RichText::new(&self.status_message).color(color).small());
                    if let Some(container) = &self.container {
                        ui.separator();
                        ui.label(
                            RichText::new(format!(
                                "{} • {}",
                                container.kind.label(),
                                container.path.display()
                            ))
                            .color(TEXT_MUTED)
                            .small(),
                        );
                    }
                });
            });

        // =====================================================================
    }
}

//! Lifecycle, file loading/saving and frame orchestration.
use super::*;

impl SaveEditorApp {
    pub fn new(cc: &eframe::CreationContext<'_>, initial_path: Option<PathBuf>) -> Self {
        configure_mito_theme(&cc.egui_ctx);

        let mut app = Self {
            container: None,
            selected_slot: 0,
            active_tab: EditorTab::InventoryAndLoadout,
            slot_textures: Default::default(),
            status_message:
                "Ready. Open a PsyZ folder (bu00/bu10) or a PS1 Memory Card (.srm/.mcr/.gme)."
                    .to_string(),
            is_error_status: false,
            selected_inv_slot: 0,
            show_locked_inv_slots: false,
            inv_replace_mode: false,
            selected_equip_index: 0,
            show_only_active_equip: true,
            wayne_subtab: WayneSubTab::EquipmentVault,
            selected_wayne_regular_slot: 0,
            selected_wayne_special_slot: 0,
            picker_category: ItemPickerFilter::All,
            picker_search: String::new(),
            show_hidden_pe_spells: false,
            show_script_hex_inspector: false,
            script_inspector_offset: 0,
        };

        crate::disc::load_configured_disc();

        if let Some(path) = initial_path {
            app.load_path(&cc.egui_ctx, path);
        } else {
            let candidates = [
                PathBuf::from("bu00"),
                PathBuf::from("../bu00"),
                PathBuf::from("save-editor/samples/Parasite_Eve_PS1_Sample_Card.srm"),
                PathBuf::from("samples/Parasite_Eve_PS1_Sample_Card.srm"),
            ];
            for cand in candidates {
                if cand.exists() {
                    app.load_path(&cc.egui_ctx, cand);
                    break;
                }
            }
        }

        app
    }

    pub(super) fn set_status(&mut self, msg: impl Into<String>, is_error: bool) {
        self.status_message = msg.into();
        self.is_error_status = is_error;
    }

    pub(super) fn load_path(&mut self, ctx: &egui::Context, path: PathBuf) {
        match MemoryCardContainer::open(&path) {
            Ok(container) => {
                let first_active = container.first_active_slot_index().unwrap_or(0);
                let active_count = container.active_slot_indices().len();
                let kind_label = container.kind.label();
                self.container = Some(container);
                self.selected_slot = first_active;
                self.selected_inv_slot = 0;
                self.rebuild_icon_textures(ctx);
                self.set_status(
                    format!(
                        "Loaded {} ({}, {} active slot(s)).",
                        path.display(),
                        kind_label,
                        active_count
                    ),
                    false,
                );
            }
            Err(err) => {
                self.set_status(format!("Error opening {}: {:#}", path.display(), err), true);
            }
        }
    }

    pub(super) fn rebuild_icon_textures(&mut self, ctx: &egui::Context) {
        self.slot_textures = Default::default();
        if let Some(container) = &self.container {
            for i in 0..MAX_CARD_SLOTS {
                if let Some(slot) = &container.slots[i] {
                    let rgba = slot.decode_icon_rgba();
                    let img = ColorImage::from_rgba_unmultiplied([16, 16], &rgba);
                    let tex = ctx.load_texture(
                        format!("pe_slot_icon_{}", i),
                        img,
                        TextureOptions::NEAREST,
                    );
                    self.slot_textures[i] = Some(tex);
                }
            }
        }
    }

    pub(super) fn save_current_container(&mut self) {
        if let Some(container) = &mut self.container {
            match container.save() {
                Ok(()) => {
                    let path_str = container.path.display().to_string();
                    self.set_status(
                        format!("✔ Saved all slots & recalculated CRC-16 to {}.", path_str),
                        false,
                    );
                }
                Err(err) => {
                    self.set_status(format!("Save failed: {:#}", err), true);
                }
            }
        }
    }
}

impl eframe::App for SaveEditorApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        if ctx.input_mut(|i| i.consume_key(egui::Modifiers::COMMAND, egui::Key::S)) {
            self.save_current_container();
        }

        self.show_application_bar(ctx);
        self.show_status_bar(ctx);
        self.show_slot_browser(ctx);
        // 4. CHECK FOR EMPTY CONTAINER OR EMPTY SLOT BEFORE RENDERING PANELS
        // =====================================================================
        if self.container.is_none() {
            egui::CentralPanel::default()
                .frame(egui::Frame::NONE.fill(BG_PANEL).inner_margin(Margin::same(20)))
                .show(ctx, |ui| {
                    ui.centered_and_justified(|ui| {
                        ui.vertical_centered(|ui| {
                            ui.add_space(100.0);
                            ui.heading("No Save Container Loaded");
                            ui.label(
                                RichText::new("Use the top bar to open a PsyZ directory (bu00) or PS1 Memory Card (.srm/.mcr).")
                                    .color(TEXT_MUTED),
                            );
                        });
                    });
                });
            return;
        }

        let is_empty_slot = self
            .container
            .as_ref()
            .and_then(|c| c.slots[self.selected_slot].as_ref())
            .is_none();

        if is_empty_slot {
            let first_active = self
                .container
                .as_ref()
                .and_then(|c| c.first_active_slot_index());
            let slot_letter = (b'A' + self.selected_slot as u8) as char;
            let mut clone_from: Option<usize> = None;

            egui::CentralPanel::default()
                .frame(egui::Frame::NONE.fill(BG_PANEL).inner_margin(Margin::same(20)))
                .show(ctx, |ui| {
                    ui.vertical_centered(|ui| {
                        ui.add_space(80.0);
                        ui.heading(format!(
                            "Slot {:02} ({}) is Empty",
                            self.selected_slot + 1,
                            slot_letter
                        ));
                        ui.add_space(8.0);
                        if let Some(src_idx) = first_active {
                            ui.label(
                                RichText::new(format!(
                                    "You can clone Slot {:02} into this empty slot to create a backup or experimental build.",
                                    src_idx + 1
                                ))
                                .color(TEXT_MUTED),
                            );
                            ui.add_space(12.0);
                            if ui
                                .button(
                                    RichText::new(format!(
                                        "📋 Clone Slot {:02} into Slot {:02} ({})",
                                        src_idx + 1,
                                        self.selected_slot + 1,
                                        slot_letter
                                    ))
                                    .strong(),
                                )
                                .clicked()
                            {
                                clone_from = Some(src_idx);
                            }
                        }
                    });
                });

            if let Some(src_idx) = clone_from {
                let target_idx = self.selected_slot;
                let ok = self
                    .container
                    .as_mut()
                    .map(|c| c.clone_slot(src_idx, target_idx).is_ok())
                    .unwrap_or(false);
                if ok {
                    self.rebuild_icon_textures(ctx);
                    self.set_status(
                        format!(
                            "Cloned Slot {:02} into Slot {:02}.",
                            src_idx + 1,
                            target_idx + 1
                        ),
                        false,
                    );
                }
            }
            return;
        }

        // =====================================================================
        self.show_workspace_header(ctx);
        self.show_active_tab(ctx);
    }
}

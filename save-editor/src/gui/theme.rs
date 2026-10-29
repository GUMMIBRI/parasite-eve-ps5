//! Shared theme palette and setup.
use super::*;

// Theme Palette Constants
pub(super) const BG_DARKEST: Color32 = Color32::from_rgb(11, 16, 14);
pub(super) const BG_PANEL: Color32 = Color32::from_rgb(16, 23, 20);
pub(super) const BG_CARD: Color32 = Color32::from_rgb(22, 32, 27);
pub(super) const BG_CARD_HOVER: Color32 = Color32::from_rgb(30, 46, 38);
pub(super) const BG_CARD_SELECTED: Color32 = Color32::from_rgb(28, 64, 45);
pub(super) const BORDER_SUBTLE: Color32 = Color32::from_rgb(42, 62, 52);
pub(super) const BORDER_ACTIVE: Color32 = Color32::from_rgb(95, 230, 150);
pub(super) const ACCENT_GREEN: Color32 = Color32::from_rgb(105, 240, 160);
pub(super) const ACCENT_AMBER: Color32 = Color32::from_rgb(245, 200, 95);
pub(super) const ACCENT_CYAN: Color32 = Color32::from_rgb(110, 210, 245);
pub(super) const TEXT_MUTED: Color32 = Color32::from_rgb(145, 168, 155);

pub(super) fn configure_mito_theme(ctx: &egui::Context) {
    let mut style = (*ctx.style()).clone();
    style.spacing.item_spacing = Vec2::new(8.0, 6.0);
    style.spacing.button_padding = Vec2::new(10.0, 5.0);
    style.visuals = egui::Visuals::dark();
    style.visuals.window_fill = BG_PANEL;
    style.visuals.panel_fill = BG_PANEL;
    style.visuals.faint_bg_color = BG_CARD;
    style.visuals.extreme_bg_color = BG_DARKEST;
    style.visuals.widgets.noninteractive.bg_stroke = Stroke::new(1.0_f32, BORDER_SUBTLE);
    style.visuals.widgets.inactive.bg_fill = Color32::from_rgb(26, 38, 32);
    style.visuals.widgets.inactive.corner_radius = CornerRadius::same(6);
    style.visuals.widgets.hovered.bg_fill = Color32::from_rgb(38, 58, 48);
    style.visuals.widgets.hovered.bg_stroke = Stroke::new(1.0_f32, ACCENT_GREEN);
    style.visuals.widgets.hovered.corner_radius = CornerRadius::same(6);
    style.visuals.widgets.active.bg_fill = BG_CARD_SELECTED;
    style.visuals.widgets.active.corner_radius = CornerRadius::same(6);
    style.visuals.selection.bg_fill = BG_CARD_SELECTED;
    style.visuals.selection.stroke = Stroke::new(1.5_f32, BORDER_ACTIVE);
    ctx.set_style(style);
}

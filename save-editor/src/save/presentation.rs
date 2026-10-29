//! Presentation operations for PeSaveSlot.
use super::*;

impl PeSaveSlot {
    pub fn is_crc_valid(&self) -> bool {
        self.stored_crc == self.calculated_crc
    }

    /// 1-based display level (`1..=99`)
    pub fn display_level(&self) -> u8 {
        self.level.saturating_add(1).clamp(1, 99)
    }

    pub fn set_display_level(&mut self, display_level: u8) {
        let clamped = display_level.clamp(1, 99);
        self.level = clamped - 1;
    }

    pub fn formatted_play_time(&self) -> String {
        let total = self.play_time_seconds.max(0);
        let hours = total / 3600;
        let minutes = (total % 3600) / 60;
        let seconds = total % 60;
        format!("{:02}:{:02}:{:02}", hours, minutes, seconds)
    }

    pub fn set_play_time_hms(&mut self, hours: i32, minutes: i32, seconds: i32) {
        self.play_time_seconds =
            hours.max(0) * 3600 + minutes.clamp(0, 59) * 60 + seconds.clamp(0, 59);
    }

    pub fn location_description(&self) -> &'static str {
        if self.is_clear_data {
            "Clear Data (EX Game)"
        } else {
            map_location_descriptive(self.current_map_number)
        }
    }

    /// Decodes the 16x16 4bpp PS1 save icon at `0x60..0x100` into a 16x16 RGBA8 image (1024 bytes).
    pub fn decode_icon_rgba(&self) -> [u8; 16 * 16 * 4] {
        let mut rgba = [0u8; 16 * 16 * 4];
        if self.raw_bytes.len() < 0x100 {
            return rgba;
        }
        let mut clut = [[0u8; 4]; 16];
        for (i, entry) in clut.iter_mut().enumerate() {
            let off = 0x60 + i * 2;
            let raw_color = u16::from_le_bytes([self.raw_bytes[off], self.raw_bytes[off + 1]]);
            if raw_color == 0 && i == 0 {
                *entry = [0, 0, 0, 0];
            } else {
                let r5 = (raw_color & 0x1F) as u8;
                let g5 = ((raw_color >> 5) & 0x1F) as u8;
                let b5 = ((raw_color >> 10) & 0x1F) as u8;
                *entry = [
                    (r5 << 3) | (r5 >> 2),
                    (g5 << 3) | (g5 >> 2),
                    (b5 << 3) | (b5 >> 2),
                    255,
                ];
            }
        }

        for y in 0..16 {
            for x_pair in 0..8 {
                let byte = self.raw_bytes[0x80 + y * 8 + x_pair];
                let idx0 = (byte & 0x0F) as usize;
                let idx1 = (byte >> 4) as usize;
                let p0 = (y * 16 + x_pair * 2) * 4;
                let p1 = p0 + 4;
                rgba[p0..p0 + 4].copy_from_slice(&clut[idx0]);
                rgba[p1..p1 + 4].copy_from_slice(&clut[idx1]);
            }
        }
        rgba
    }

    pub fn format_equipment_mods(rec: &EquipmentRecord) -> String {
        let mut names = Vec::new();
        let is_armor = rec.is_armor();
        for i in 0..(rec.tail_count.max(rec.tail_capacity).min(10) as usize) {
            let raw_mod = rec.tail_data[i];
            if (raw_mod & 0x1F) != 0 {
                let meta = mod_metadata(raw_mod, is_armor);
                if meta.code_name == "MOD_UNKNOWN" {
                    names.push(format!(
                        "Unknown effect {} (raw 0x{:02X})",
                        raw_mod & 31,
                        raw_mod
                    ));
                } else {
                    names.push(meta.name.to_string());
                }
            }
        }
        if names.is_empty() {
            "None".to_string()
        } else {
            names.join(", ")
        }
    }
}

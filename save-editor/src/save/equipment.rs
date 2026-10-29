//! Equipment record model and exact 32-byte encoding.
use super::*;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EquipmentRecord {
    pub index: usize, // 0..128 (inventory value = 0x100 + index)
    pub icon_id: u8,
    pub tail_capacity: u8,
    pub reserved02: [u8; 2],
    pub item_id: u8,
    pub flags: u8,
    pub kind: u8,
    pub base_stats: [u8; 3],
    pub ammo: u16,
    pub reserve_ammo: u16,
    pub bonus_stats: [i16; 3],
    pub tail_count: u8,
    pub tail_data: [u8; 11],
}

impl EquipmentRecord {
    pub fn empty(index: usize) -> Self {
        Self {
            index,
            icon_id: 0,
            tail_capacity: 0,
            reserved02: [0; 2],
            item_id: 0,
            flags: 0,
            kind: 0,
            base_stats: [0; 3],
            ammo: 0,
            reserve_ammo: 0,
            bonus_stats: [0; 3],
            tail_count: 0,
            tail_data: [0; 11],
        }
    }

    /// A new record for `item_id`, made the way the game makes one: a verbatim copy of the item's
    /// base record (`Inv_FindSlotByIndex`). The verified USA templates also work
    /// without a mounted disc; no guessed stats or missing factory effects.
    pub fn from_item_template(index: usize, item_id: u8) -> Self {
        if let Some(bytes) = crate::definitions::template(item_id) {
            return Self::parse(index, bytes);
        }
        if let Some(bytes) = game_data().and_then(|g| g.item_record(item_id)) {
            let mut record = Self::parse(index, &bytes);
            record.tail_capacity = record.tail_capacity.min(10);
            return record;
        }
        if (0x3F..=0xC7).contains(&item_id) {
            let mut record = Self::parse(index, &EQUIPMENT_TEMPLATES[(item_id - 0x3F) as usize]);
            // The unused retail Debug SMG has a capacity of 100, beyond the
            // record's ten editable effect slots and the port's save bounds.
            record.tail_capacity = record.tail_capacity.min(10);
            record
        } else {
            Self::empty(index)
        }
    }

    /// Icon (bag picture) and kind (weapon class: the model Aya holds and how it fires) for
    /// `item_id`: from the disc when loaded, else the game's value for the item's category.
    pub fn icon_and_kind_for(item_id: u8) -> (u8, u8) {
        if let Some(bytes) = crate::definitions::template(item_id) {
            return (bytes[0], bytes[6]);
        }
        if let Some(rec) = game_data().and_then(|g| g.item_record(item_id)) {
            return (rec[0x00], rec[0x06]);
        }
        let meta = item_metadata(item_id);
        game_icon_and_kind(meta.category)
            .unwrap_or((meta.default_icon_id.max(1), meta.default_kind))
    }

    pub fn is_active(&self) -> bool {
        self.icon_id != 0 && self.item_id != 0
    }

    pub fn is_engraved(&self) -> bool {
        (self.flags & 0x10) != 0
    }

    pub fn set_engraved(&mut self, engraved: bool) {
        if engraved {
            self.flags |= 0x10;
        } else {
            self.flags &= !0x10;
        }
    }

    pub fn is_armor(&self) -> bool {
        self.kind == 9 || item_metadata(self.item_id).category.is_armor()
    }

    pub fn is_weapon(&self) -> bool {
        !self.is_armor() && self.is_active()
    }

    pub fn display_name(&self) -> &'static str {
        item_metadata(self.item_id).display_name
    }

    pub fn effective_stats(&self) -> [i32; 3] {
        [
            self.base_stats[0] as i32 + self.bonus_stats[0] as i32,
            self.base_stats[1] as i32 + self.bonus_stats[1] as i32,
            self.base_stats[2] as i32 + self.bonus_stats[2] as i32,
        ]
    }

    pub fn mod_id_at(&self, slot: usize) -> u8 {
        if slot < 11 {
            self.tail_data[slot] & 0x1F
        } else {
            0
        }
    }

    pub fn set_mod_at(&mut self, slot: usize, mod_id: u8) {
        if slot < 11 {
            let is_armor = self.is_armor();
            self.tail_data[slot] = encode_mod_byte(mod_id, is_armor);
        }
    }

    pub fn parse(index: usize, bytes: &[u8]) -> Self {
        let mut reserved02 = [0u8; 2];
        reserved02.copy_from_slice(&bytes[0x02..0x04]);
        let mut base_stats = [0u8; 3];
        base_stats.copy_from_slice(&bytes[0x07..0x0A]);
        let ammo = u16::from_le_bytes([bytes[0x0A], bytes[0x0B]]);
        let reserve_ammo = u16::from_le_bytes([bytes[0x0C], bytes[0x0D]]);
        let bonus_stats = [
            i16::from_le_bytes([bytes[0x0E], bytes[0x0F]]),
            i16::from_le_bytes([bytes[0x10], bytes[0x11]]),
            i16::from_le_bytes([bytes[0x12], bytes[0x13]]),
        ];
        let mut tail_data = [0u8; 11];
        tail_data.copy_from_slice(&bytes[0x15..0x20]);

        Self {
            index,
            icon_id: bytes[0x00],
            tail_capacity: bytes[0x01],
            reserved02,
            item_id: bytes[0x04],
            flags: bytes[0x05],
            kind: bytes[0x06],
            base_stats,
            ammo,
            reserve_ammo,
            bonus_stats,
            tail_count: bytes[0x14],
            tail_data,
        }
    }

    pub fn write(&self, out: &mut [u8]) {
        out[0x00] = self.icon_id;
        out[0x01] = self.tail_capacity;
        out[0x02..0x04].copy_from_slice(&self.reserved02);
        out[0x04] = self.item_id;
        out[0x05] = self.flags;
        out[0x06] = self.kind;
        out[0x07..0x0A].copy_from_slice(&self.base_stats);
        out[0x0A..0x0C].copy_from_slice(&self.ammo.to_le_bytes());
        out[0x0C..0x0E].copy_from_slice(&self.reserve_ammo.to_le_bytes());
        for (i, bonus) in self.bonus_stats.iter().enumerate() {
            let off = 0x0E + i * 2;
            out[off..off + 2].copy_from_slice(&bonus.to_le_bytes());
        }
        out[0x14] = self.tail_count;
        out[0x15..0x20].copy_from_slice(&self.tail_data);
    }
}

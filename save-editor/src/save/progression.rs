//! Progression operations for PeSaveSlot.
use super::*;

impl PeSaveSlot {
    pub fn has_spell(&self, bit_index: u8) -> bool {
        if bit_index >= 32 {
            return false;
        }
        (self.parasite_spell_flags & (1u32 << bit_index)) != 0
    }

    pub fn set_spell(&mut self, bit_index: u8, unlocked: bool) {
        if bit_index >= 32 {
            return;
        }
        if unlocked {
            self.parasite_spell_flags |= 1u32 << bit_index;
        } else {
            self.parasite_spell_flags &= !(1u32 << bit_index);
        }
    }

    pub fn read_script_u8(&self, offset: usize) -> u8 {
        if offset < SCRIPT_VAR_SIZE {
            self.raw_bytes[SCRIPT_VAR_OFFSET + offset]
        } else {
            0
        }
    }

    pub fn write_script_u8(&mut self, offset: usize, value: u8) {
        if offset < SCRIPT_VAR_SIZE {
            self.raw_bytes[SCRIPT_VAR_OFFSET + offset] = value;
        }
    }
}

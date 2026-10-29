//! PE Enhancements' per-save Junk counter, independent of bag Junk items.
//! Supports the shipped PELT v1-v4 blocks and pe_economy.loot v1 (36 bytes).
//! Change only the balance and its checksums, never pity/RNG/difficulty/state.
use crate::mod_save::{self, ModData, OFFSET};
use anyhow::{ensure, Result};
const NAME: &str = "pe_economy.loot";
pub const MAX: u32 = 99_999;
fn get(b: &[u8], at: usize) -> u32 {
    u32::from_le_bytes(b[at..at + 4].try_into().unwrap())
}
fn put(b: &mut [u8], at: usize, v: u32) {
    b[at..at + 4].copy_from_slice(&v.to_le_bytes());
}
fn validate_legacy(b: &[u8]) -> Result<u32> {
    ensure!(
        b.len() == 32 && &b[..4] == b"PELT",
        "Invalid legacy Junk block"
    );
    let version = get(b, 4);
    let junk = get(b, 8);
    ensure!(junk <= MAX, "Invalid Junk balance");
    let valid = match version {
        1 => get(b, 12) == junk ^ 0xa5a5a5a5,
        2 => get(b, 16) == junk ^ get(b, 12) ^ 0xa5a5a5a5,
        3 | 4 => get(b, 20) == junk ^ get(b, 12) ^ get(b, 16) ^ 0xa5a5a5a5,
        _ => {
            anyhow::bail!("Unsupported PELT version {}; preserved unchanged", version);
        }
    };
    ensure!(valid, "Junk block checksum mismatch; preserved unchanged");
    if version == 4 {
        ensure!(
            get(b, 28) == get(b, 24) ^ 0x5a5a5a5a,
            "Junk pity checksum mismatch; preserved unchanged"
        );
    }
    Ok(junk)
}
pub fn read(raw: &[u8]) -> Result<Option<u32>> {
    let parsed = ModData::inspect(raw)?;
    let Some(r) = parsed.records.iter().find(|r| r.id == mod_save::id(NAME)) else {
        return Ok(None);
    };
    if r.version == 0 {
        return validate_legacy(&r.data).map(Some);
    }
    ensure!(
        r.version == 1 && r.data.len() == 36,
        "Unsupported Junk record schema/size; preserved unchanged"
    );
    let n = get(&r.data, 0);
    ensure!(n <= MAX, "Invalid Junk balance");
    Ok(Some(n))
}
pub fn write(raw: &mut [u8], junk: u32) -> Result<()> {
    ensure!(junk <= MAX, "Junk must be between 0 and 99,999");
    let parsed = ModData::inspect(raw)?;
    ensure!(
        parsed.editable,
        "Damaged or unsupported mod container: editing is disabled"
    );
    read(raw)?;
    if let Some(r) = parsed.records.iter().find(|r| r.id == mod_save::id(NAME)) {
        let mut b = r.data.clone();
        if r.version == 0 {
            let checksum = match get(&b, 4) {
                1 => 12,
                2 => 16,
                _ => 20,
            };
            let old = get(&b, 8);
            put(&mut b, 8, junk);
            // Every legacy primary checksum is XOR of the balance and other state.
            let c = get(&b, checksum) ^ old ^ junk;
            put(&mut b, checksum, c);
        } else {
            put(&mut b, 0, junk);
        }
        if &raw[OFFSET..OFFSET + 4] == b"PEMC" {
            return mod_save::replace_record(raw, NAME, r.version, &b, None);
        }
        return mod_save::replace_record(raw, NAME, 0, &b, Some((0, &b)));
    }
    if &raw[OFFSET..OFFSET + 4] == b"PEMC" {
        let mut b = vec![0; 36];
        put(&mut b, 0, junk);
        b[30] = 1; // Normal difficulty; empty saved state.
        mod_save::replace_record(raw, NAME, 1, &b, None)
    } else {
        ensure!(
            raw[OFFSET..OFFSET + 32].iter().all(|b| *b == 0),
            "Unrecognized data occupies the Junk block; preserved unchanged"
        );
        let mut b = vec![0; 32];
        b[..4].copy_from_slice(b"PELT");
        put(&mut b, 4, 4);
        put(&mut b, 8, junk);
        put(&mut b, 16, 1 << 16);
        put(&mut b, 20, junk ^ (1 << 16) ^ 0xa5a5a5a5);
        put(&mut b, 28, 0x5a5a5a5a);
        mod_save::replace_record(raw, NAME, 0, &b, Some((0, &b)))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn legacy(version: u32) -> Vec<u8> {
        let mut raw = vec![0; 0x2000];
        let b = &mut raw[OFFSET..OFFSET + 32];
        b[..4].copy_from_slice(b"PELT");
        put(b, 4, version);
        put(b, 8, 1234);
        match version {
            1 => put(b, 12, 1234 ^ 0xa5a5a5a5),
            2 => {
                put(b, 12, 0x10203);
                put(b, 16, 1234 ^ 0x10203 ^ 0xa5a5a5a5);
            }
            _ => {
                put(b, 12, 0x20304);
                put(b, 16, 0x01010007);
                put(b, 20, 1234 ^ 0x20304 ^ 0x01010007 ^ 0xa5a5a5a5);
                put(b, 24, 0x0506);
                put(b, 28, 0x0506 ^ 0x5a5a5a5a);
            }
        };
        raw
    }
    #[test]
    fn legacy_versions_edit_only_junk_and_primary_checksum() {
        for version in 1..=4 {
            let mut raw = legacy(version);
            raw[OFFSET + 0x40..OFFSET + 0x50].fill(0x77);
            let before = raw.clone();
            assert_eq!(read(&raw).unwrap(), Some(1234));
            write(&mut raw, 99999).unwrap();
            assert_eq!(read(&raw).unwrap(), Some(99999));
            let c = match version {
                1 => 12,
                2 => 16,
                _ => 20,
            };
            for i in 0..raw.len() {
                if (OFFSET + 8..OFFSET + 12).contains(&i)
                    || (OFFSET + c..OFFSET + c + 4).contains(&i)
                    || (0x1c8c..0x1c90).contains(&i)
                {
                    continue;
                }
                assert_eq!(raw[i], before[i], "v{} byte {:X}", version, i);
            }
        }
    }
    #[test]
    fn native_record_keeps_all_other_fields_and_unknown_records() {
        let mut raw = include_bytes!("../tests/fixtures/mod-records.bin").to_vec();
        write(&mut raw, 7654).unwrap();
        let mut p = ModData::inspect(&raw).unwrap();
        let r = p
            .records
            .iter_mut()
            .find(|r| r.id == mod_save::id(NAME))
            .unwrap();
        let mut data = r.data.clone();
        for (i, b) in data.iter_mut().enumerate().skip(4) {
            *b = (i * 3) as u8;
        }
        mod_save::replace_record(&mut raw, NAME, 1, &data, None).unwrap();
        let before = ModData::inspect(&raw).unwrap();
        write(&mut raw, 0).unwrap();
        assert_eq!(read(&raw).unwrap(), Some(0));
        let after = ModData::inspect(&raw).unwrap();
        for r in before.records {
            let a = after.records.iter().find(|a| a.id == r.id).unwrap();
            if r.id == mod_save::id(NAME) {
                assert_eq!(&a.data[4..], &r.data[4..]);
            } else {
                assert_eq!(a, &r);
            }
        }
    }
    #[test]
    fn embedded_old_block_is_kept_as_version_zero() {
        let mut raw = include_bytes!("../tests/fixtures/mod-records.bin").to_vec();
        let old = legacy(4);
        mod_save::replace_record(&mut raw, NAME, 0, &old[OFFSET..OFFSET + 32], None).unwrap();
        write(&mut raw, 42).unwrap();
        let p = ModData::inspect(&raw).unwrap();
        let r = p
            .records
            .iter()
            .find(|r| r.id == mod_save::id(NAME))
            .unwrap();
        assert_eq!(r.version, 0);
        assert_eq!(validate_legacy(&r.data).unwrap(), 42);
    }
    #[test]
    fn bad_newer_and_opaque_data_never_overwritten() {
        for v in 0..5 {
            let mut raw = legacy(4);
            match v {
                0 => raw[OFFSET + 20] ^= 1,
                1 => put(&mut raw[OFFSET..], 4, 5),
                2 => raw[OFFSET..OFFSET + 4].copy_from_slice(b"????"),
                3 => raw[OFFSET + 28] ^= 1,
                _ => {
                    raw = include_bytes!("../tests/fixtures/mod-records.bin").to_vec();
                    mod_save::replace_record(&mut raw, NAME, 2, &[0; 36], None).unwrap();
                }
            }
            let before = raw.clone();
            assert!(write(&mut raw, 1).is_err());
            assert_eq!(raw, before);
        }
    }
    #[test]
    fn initialize_and_bounds() {
        let mut raw = vec![0; 0x2000];
        assert_eq!(read(&raw).unwrap(), None);
        write(&mut raw, 123).unwrap();
        assert_eq!(read(&raw).unwrap(), Some(123));
        assert_eq!(get(&raw[OFFSET..], 4), 4);
        let before = raw.clone();
        assert!(write(&mut raw, 100000).is_err());
        assert_eq!(raw, before);
    }
}

#[cfg(test)]
mod native_tests {
    #[test]
    fn native_junk_record_and_editor_output_agree() {
        let mut raw = include_bytes!("../tests/fixtures/mod-records-junk.bin").to_vec();
        assert_eq!(super::read(&raw).unwrap(), Some(4321));
        super::write(&mut raw, 12345).unwrap();
        let Some(exe) = std::env::var_os("PE_MOD_SAVE_ORACLE") else {
            return;
        };
        let p = std::env::temp_dir().join(format!("pe-junk-oracle-{}.bin", std::process::id()));
        std::fs::write(&p, raw).unwrap();
        let status = std::process::Command::new(exe)
            .arg("read-junk")
            .arg(&p)
            .status()
            .unwrap();
        std::fs::remove_file(p).unwrap();
        assert!(status.success());
    }
}

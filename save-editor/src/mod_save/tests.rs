
use super::*;
fn native() -> Vec<u8> {
    include_bytes!("../../tests/fixtures/mod-records.bin").to_vec()
}
fn refresh(raw: &mut [u8]) {
    let tail = &mut raw[OFFSET..];
    let mut lin = gather(tail);
    let n = u16_at(&lin, 6) as usize;
    put32(&mut lin, 8, 0);
    let c = crc32(&lin[..n]);
    put32(&mut lin, 8, c);
    scatter(&lin, tail);
}
#[test]
fn native_writer_fixture_crosses_window() {
    let raw = native();
    let d = ModData::inspect(&raw).unwrap();
    assert!(d.editable && d.warnings.is_empty());
    assert_eq!(d.records.len(), 3);
    assert_eq!(d.keys().unwrap(), Some(vec![0x38, 0xc8]));
    assert_eq!(d.sort().unwrap(), Some(SortSettings::default()));
    let r = d
        .records
        .iter()
        .find(|r| r.id == id("other_author.future"))
        .unwrap();
    assert_eq!(r.version, 9);
    assert_eq!(
        r.data,
        (0..512).map(|i| (i * 7 + 3) as u8).collect::<Vec<_>>()
    );
    assert!(u16_at(&raw[OFFSET..], 6) as usize > WIN);
}
#[test]
fn edits_preserve_unknown_and_compatibility() {
    let mut raw = native();
    let d = ModData::inspect(&raw).unwrap();
    let unknown = d
        .records
        .iter()
        .find(|r| r.id == id("other_author.future"))
        .unwrap()
        .clone();
    let marker = raw[OFFSET + 16..OFFSET + 20].to_vec();
    set_keys(&mut raw, &[0x39, 0xcc]).unwrap();
    let s = SortSettings {
        auto: 1,
        ..Default::default()
    };
    set_sort(&mut raw, &s).unwrap();
    let edited = ModData::inspect(&raw).unwrap();
    assert!(edited.editable && edited.warnings.is_empty());
    assert_eq!(edited.keys().unwrap(), Some(vec![0x39, 0xcc]));
    assert_eq!(edited.sort().unwrap(), Some(s));
    assert!(edited.records.contains(&unknown));
    assert_eq!(&raw[OFFSET + 16..OFFSET + 20], &marker);
    // Remove container to force the old build's parser path: both edits must still be there.
    let mut old = raw.clone();
    old[OFFSET..OFFSET + 4].fill(0);
    let old = ModData::inspect(&old).unwrap();
    assert_eq!(old.keys().unwrap(), edited.keys().unwrap());
    assert_eq!(old.sort().unwrap(), edited.sort().unwrap());
    assert_eq!(
        crate::crc::calc_save_crc16(&raw) as u32,
        crate::crc::read_stored_crc(&raw)
    );
}
#[test]
fn native_host_consumes_editor_output() {
    let Some(exe) = std::env::var_os("PE_MOD_SAVE_ORACLE") else {
        return;
    };
    let mut raw = native();
    set_keys(&mut raw, &[0x39, 0xcc]).unwrap();
    set_sort(
        &mut raw,
        &SortSettings {
            auto: 1,
            ..Default::default()
        },
    )
    .unwrap();
    let path = std::env::temp_dir().join(format!("pe-mod-oracle-{}.bin", std::process::id()));
    std::fs::write(&path, raw).unwrap();
    let status = std::process::Command::new(exe)
        .arg("read")
        .arg(&path)
        .status()
        .unwrap();
    std::fs::remove_file(path).unwrap();
    assert!(status.success());
}
#[test]
fn legacy_edits_do_not_migrate_or_destroy_economy() {
    let mut raw = vec![0u8; 0x2000];
    raw[OFFSET..OFFSET + 32].fill(0x5a);
    raw[OFFSET..OFFSET + 4].copy_from_slice(b"PELT");
    let original = raw[..OFFSET + 32].to_vec();
    set_keys(&mut raw, &[0xc8]).unwrap();
    set_sort(&mut raw, &SortSettings::default()).unwrap();
    assert_eq!(&raw[OFFSET..OFFSET + 32], &original[OFFSET..]);
    assert_ne!(&raw[OFFSET..OFFSET + 4], b"PEMC");
    assert_eq!(
        ModData::inspect(&raw).unwrap().keys().unwrap(),
        Some(vec![0xc8])
    );
}
#[test]
fn changed_legacy_window_wins_and_survives_other_edit() {
    let mut raw = native();
    let b = legacy_block(
        &{
            let mut b = vec![0; 68];
            put16(&mut b, 0, 1);
            put16(&mut b, 4, 0xdc);
            b
        },
        b"PKEY",
        0x6b657973,
    );
    raw[OFFSET + WIN..OFFSET + WIN + b.len()].copy_from_slice(&b);
    let d = ModData::inspect(&raw).unwrap();
    assert_eq!(d.keys().unwrap(), Some(vec![0xdc]));
    assert!(!d.warnings.is_empty());
    set_sort(
        &mut raw,
        &SortSettings {
            auto: 1,
            ..Default::default()
        },
    )
    .unwrap();
    assert_eq!(
        ModData::inspect(&raw).unwrap().keys().unwrap(),
        Some(vec![0xdc])
    );
}
#[test]
fn unsupported_container_and_record_are_not_overwritten() {
    for change in 0..3 {
        let mut raw = native();
        match change {
            0 => raw[OFFSET + 4] = 2,
            1 => raw[OFFSET + 5] = 1,
            _ => {
                let mut lin = gather(&raw[OFFSET..]);
                let mut at = HEADER;
                while u32_at(&lin, at) != id("pe_qol.keys") {
                    at += 12 + u16_at(&lin, at + 6) as usize;
                }
                put16(&mut lin, at + 4, 2);
                let n = u16_at(&lin, at + 6) as usize;
                let c = record_crc(&lin[at..at + 8], &lin[at + 12..at + 12 + n]);
                put32(&mut lin, at + 8, c);
                scatter(&lin, &mut raw[OFFSET..]);
                refresh(&mut raw);
            }
        }
        let before = raw.clone();
        assert!(set_keys(&mut raw, &[0x38]).is_err());
        assert_eq!(raw, before);
    }
}
#[test]
fn corrupted_container_or_record_is_inspect_only() {
    for off in [8, HEADER + 8, HEADER + 12] {
        let mut raw = native();
        raw[OFFSET + off] ^= 1;
        let before = raw.clone();
        let d = ModData::inspect(&raw).unwrap();
        assert!(!d.editable);
        assert!(!d.warnings.is_empty());
        assert!(set_sort(&mut raw, &SortSettings::default()).is_err());
        assert_eq!(raw, before);
    }
}
#[test]
fn malformed_lengths_and_inputs_never_panic() {
    for n in [0, 1, 19, 745, 65535] {
        let mut raw = native();
        put16(&mut raw[OFFSET..], 6, n);
        assert!(!ModData::inspect(&raw).unwrap().editable);
    }
    for n in [0, 7, OFFSET, 0x1fff, 0x2001] {
        assert!(ModData::inspect(&vec![0; n]).is_err());
    }
    let mut raw = native();
    let before = raw.clone();
    assert!(set_keys(&mut raw, &[0x38, 0x38]).is_err());
    assert_eq!(raw, before);
    assert!(set_keys(&mut raw, &[0x01]).is_err());
    assert_eq!(raw, before);
}
#[test]
fn ordinary_save_and_json_roundtrip_preserve_entire_tail() {
    use crate::{memcard::MemoryCardContainer, save::PeSaveSlot};
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("samples/Parasite_Eve_PS1_Sample_Card.srm");
    for variant in 0..4 {
        let mut card = MemoryCardContainer::open(&path).unwrap();
        let save = card.slots[0].as_mut().unwrap();
        let mut raw = native();
        match variant {
            1 => raw[OFFSET + 4] = 2,
            2 => raw[OFFSET + 40] ^= 1,
            3 => raw[OFFSET..].fill(0xa5),
            _ => {}
        }
        save.raw_bytes[OFFSET..].copy_from_slice(&raw[OFFSET..]);
        save.bonus_points += 1;
        let json = serde_json::to_string(save).unwrap();
        let mut save: PeSaveSlot = serde_json::from_str(&json).unwrap();
        let out = save.to_bytes();
        assert_eq!(&out[OFFSET..], &raw[OFFSET..]);
        assert_eq!(
            crate::crc::calc_save_crc16(&out) as u32,
            crate::crc::read_stored_crc(&out)
        );
    }
}


use super::*;
#[test]
fn full_container_rejects_new_record_without_mutation() {
    let mut raw = vec![0u8; 0x2000];
    let mut lin = vec![0; ROOM];
    lin[..4].copy_from_slice(b"PEMC");
    lin[4] = 1;
    let payload = vec![0x55; 700];
    let total = HEADER + 12 + payload.len();
    put16(&mut lin, 6, total as u16);
    put32(&mut lin, 12, crc32(&raw[OFFSET + WIN..OFFSET + END]));
    put32(&mut lin, HEADER, id("unrelated.full"));
    put16(&mut lin, HEADER + 4, 1);
    put16(&mut lin, HEADER + 6, payload.len() as u16);
    lin[HEADER + 12..total].copy_from_slice(&payload);
    let c = record_crc(&lin[HEADER..HEADER + 8], &payload);
    put32(&mut lin, HEADER + 8, c);
    let c = crc32(&lin[..total]);
    put32(&mut lin, 8, c);
    scatter(&lin, &mut raw[OFFSET..]);
    assert!(ModData::inspect(&raw).unwrap().editable);
    let before = raw.clone();
    assert!(set_keys(&mut raw, &[0x38]).is_err());
    assert_eq!(raw, before);
}
#[test]
fn legacy_bad_checksum_and_unsupported_schema_are_retained() {
    let mut raw = vec![0; 0x2000];
    set_keys(&mut raw, &[0x38]).unwrap();
    raw[OFFSET + WIN + 12] ^= 1;
    let before = raw.clone();
    assert!(ModData::inspect(&raw).unwrap().keys().is_err());
    assert!(set_keys(&mut raw, &[0x39]).is_err());
    assert_eq!(raw, before);
}

#[test]
fn opaque_legacy_slots_are_not_overwritten() {
    for keys in [true, false] {
        let mut raw = vec![0u8; 0x2000];
        let at = OFFSET + if keys { WIN } else { 0x260 };
        raw[at + 17] = 0x55;
        let before = raw.clone();
        let result = if keys {
            set_keys(&mut raw, &[0x38])
        } else {
            set_sort(&mut raw, &SortSettings::default())
        };
        assert!(result.is_err(), "Unknown legacy slot must remain opaque");
        assert_eq!(raw, before);
    }
}

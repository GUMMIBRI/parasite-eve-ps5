//! Native PEMC records and the PKEY/SORT compatibility window.
//! Ordinary edits never repack the tail. Explicit mod edits are transactional and
//! require a completely valid, supported container; unknown records stay opaque.
use anyhow::{ensure, Result};

pub const OFFSET: usize = 0x1c90;
pub const SIZE: usize = 0x370;
const WIN: usize = 0x200;
const END: usize = 0x288;
const ROOM: usize = SIZE - (END - WIN);
const HEADER: usize = 20;
const LEGACY: [(&str, usize, usize, &[u8; 4]); 5] = [
    ("pe_economy.loot", 0, 32, b"PELT"),
    ("pe_economy.catalog", 0x40, 328, b"PECT"),
    ("pe_economy.shop", 0x1c0, 20, b"PESH"),
    ("pe_qol.keys", 0x200, 84, b"PKEY"),
    ("pe_qol.sort", 0x260, 40, b"SORT"),
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Record {
    pub id: u32,
    pub version: u16,
    pub data: Vec<u8>,
}
impl Record {
    pub fn name(&self) -> String {
        LEGACY
            .iter()
            .find(|r| id(r.0) == self.id)
            .map(|r| r.0.to_string())
            .unwrap_or_else(|| format!("Unknown record {:08X}", self.id))
    }
}
#[derive(Debug, Clone)]
pub struct ModData {
    pub format: String,
    pub records: Vec<Record>,
    pub warnings: Vec<String>,
    pub editable: bool,
    container: bool,
}
fn u16_at(b: &[u8], p: usize) -> u16 {
    u16::from_le_bytes(b[p..p + 2].try_into().unwrap())
}
fn u32_at(b: &[u8], p: usize) -> u32 {
    u32::from_le_bytes(b[p..p + 4].try_into().unwrap())
}
fn put16(b: &mut [u8], p: usize, v: u16) {
    b[p..p + 2].copy_from_slice(&v.to_le_bytes());
}
fn put32(b: &mut [u8], p: usize, v: u32) {
    b[p..p + 4].copy_from_slice(&v.to_le_bytes());
}
pub fn id(name: &str) -> u32 {
    name.bytes().fold(0x811c9dc5u32, |h, c| {
        (h ^ c as u32).wrapping_mul(0x01000193)
    })
}
pub fn crc32(b: &[u8]) -> u32 {
    let mut c = !0u32;
    for x in b {
        c ^= *x as u32;
        for _ in 0..8 {
            c = (c >> 1) ^ if c & 1 != 0 { 0xedb88320 } else { 0 };
        }
    }
    !c
}
fn gather(tail: &[u8]) -> Vec<u8> {
    [&tail[..WIN], &tail[END..]].concat()
}
fn scatter(lin: &[u8], tail: &mut [u8]) {
    tail[..WIN].copy_from_slice(&lin[..WIN]);
    tail[END..].copy_from_slice(&lin[WIN..]);
}
fn record_crc(header: &[u8], payload: &[u8]) -> u32 {
    crc32(&[&header[..8], payload].concat())
}

impl ModData {
    pub fn inspect(raw: &[u8]) -> Result<Self> {
        ensure!(raw.len() == 0x2000, "Expected an 8 KiB save block");
        let tail = &raw[OFFSET..];
        let mut result = Self {
            format: "Legacy / opaque tail".into(),
            records: vec![],
            warnings: vec![],
            editable: true,
            container: false,
        };
        if &tail[..4] != b"PEMC" {
            for (name, at, size, magic) in LEGACY {
                if &tail[at..at + 4] == magic {
                    result.records.push(Record {
                        id: id(name),
                        version: 0,
                        data: tail[at..at + size].to_vec(),
                    });
                }
            }
            if tail.iter().all(|b| *b == 0) {
                result.format = "No saved mod data".into();
            }
            return Ok(result);
        }
        result.container = true;
        result.format = format!(
            "PEMC {} · mod API {} · writer {}",
            tail[4],
            u16_at(tail, 16),
            u16_at(tail, 18)
        );
        if tail[4] != 1 || tail[5] != 0 {
            result.editable = false;
            result.warnings.push("Unsupported container version or flags. Preserved unchanged; mod editing is disabled.".into());
            return Ok(result);
        }
        let total = u16_at(tail, 6) as usize;
        if !(HEADER..=ROOM).contains(&total) {
            result.editable = false;
            result.warnings.push(
                "Invalid container length. Preserved unchanged; mod editing is disabled.".into(),
            );
            return Ok(result);
        }
        let mut lin = gather(tail);
        let stored = u32_at(&lin, 8);
        put32(&mut lin, 8, 0);
        let whole = crc32(&lin[..total]) == stored;
        if !whole {
            result.editable = false;
            result.warnings.push("Container checksum mismatch. Valid individual records are shown for inspection only; original bytes are preserved.".into());
        }
        let mut at = HEADER;
        while at < total {
            if total - at < 12 {
                result.editable = false;
                result.warnings.push("Truncated record header.".into());
                break;
            }
            let size = u16_at(&lin, at + 6) as usize;
            if size > total - at - 12 {
                result.editable = false;
                result.warnings.push("Truncated record payload.".into());
                break;
            }
            let rec_id = u32_at(&lin, at);
            let data = &lin[at + 12..at + 12 + size];
            if result.records.len() >= 64
                || result.records.iter().any(|r| r.id == rec_id)
                || record_crc(&lin[at..at + 8], data) != u32_at(&lin, at + 8)
            {
                result.editable = false;
                result.warnings.push(format!(
                    "Invalid checksum, duplicate or excess record {:08X}; not editable.",
                    rec_id
                ));
            } else {
                result.records.push(Record {
                    id: rec_id,
                    version: u16_at(&lin, at + 4),
                    data: data.to_vec(),
                });
            }
            at += 12 + size;
        }
        // Match the host's precedence: a window changed by an older build wins.
        if crc32(&tail[WIN..END]) != u32_at(&lin, 12) {
            result.warnings.push("Compatibility window changed since this container was written. PKEY/SORT window blocks take precedence, as in the game.".into());
            for (name, at, size, magic) in LEGACY.into_iter().filter(|r| r.1 >= WIN) {
                if &tail[at..at + 4] == magic {
                    let record = Record {
                        id: id(name),
                        version: 0,
                        data: tail[at..at + size].to_vec(),
                    };
                    if let Some(r) = result.records.iter_mut().find(|r| r.id == record.id) {
                        *r = record;
                    } else {
                        result.records.push(record);
                    }
                }
            }
        }
        Ok(result)
    }
    fn payload(
        &self,
        name: &str,
        size: usize,
        magic: &[u8; 4],
        seed: u32,
    ) -> Result<Option<Vec<u8>>> {
        let Some(r) = self.records.iter().find(|r| r.id == id(name)) else {
            return Ok(None);
        };
        if r.version == 1 {
            ensure!(
                r.data.len() == size,
                "Unsupported size for {name}; preserved unchanged"
            );
            return Ok(Some(r.data.clone()));
        }
        ensure!(
            r.version == 0 && r.data.len() == size + 16,
            "Unsupported schema for {name}; preserved unchanged"
        );
        let b = &r.data;
        ensure!(
            &b[..4] == magic
                && u32_at(b, 4) == 1
                && u32_at(b, 8) == size as u32
                && u32_at(b, 12) == legacy_crc(&b[16..], seed),
            "Invalid legacy block for {name}; preserved unchanged"
        );
        Ok(Some(b[16..].to_vec()))
    }
    pub fn keys(&self) -> Result<Option<Vec<u16>>> {
        let Some(b) = self.payload("pe_qol.keys", 68, b"PKEY", 0x6b657973)? else {
            return Ok(None);
        };
        let n = u16_at(&b, 0) as usize;
        ensure!(n <= 32, "Invalid key count");
        let keys: Vec<u16> = (0..n).map(|i| u16_at(&b, 4 + i * 2)).collect();
        ensure!(
            keys.iter()
                .enumerate()
                .all(|(i, k)| is_key(*k) && !keys[..i].contains(k)),
            "Invalid or duplicate pocketed key"
        );
        Ok(Some(keys))
    }
    pub fn sort(&self) -> Result<Option<SortSettings>> {
        let Some(b) = self.payload("pe_qol.sort", 24, b"SORT", 0x736f7274)? else {
            return Ok(None);
        };
        let s = SortSettings {
            auto: u32_at(&b, 0),
            rank: u32_at(&b, 4),
            category: u32_at(&b, 8),
            weapon: u32_at(&b, 12),
            armor: u32_at(&b, 16),
        };
        s.validate()?;
        Ok(Some(s))
    }
}
pub fn is_key(k: u16) -> bool {
    (0x38..=0x3e).contains(&k) || (0xc8..=0xdc).contains(&k)
}
fn legacy_crc(b: &[u8], seed: u32) -> u32 {
    b.iter()
        .fold(seed, |h, b| h.wrapping_mul(33).wrapping_add(*b as u32))
}
fn legacy_block(data: &[u8], magic: &[u8; 4], seed: u32) -> Vec<u8> {
    let mut b = vec![0; 16 + data.len()];
    b[..4].copy_from_slice(magic);
    put32(&mut b, 4, 1);
    put32(&mut b, 8, data.len() as u32);
    put32(&mut b, 12, legacy_crc(data, seed));
    b[16..].copy_from_slice(data);
    b
}
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SortSettings {
    pub auto: u32,
    pub rank: u32,
    pub category: u32,
    pub weapon: u32,
    pub armor: u32,
}
impl Default for SortSettings {
    fn default() -> Self {
        Self {
            auto: 0,
            rank: 0x800923f8,
            category: 0x80092458,
            weapon: 0,
            armor: 0,
        }
    }
}
impl SortSettings {
    fn validate(&self) -> Result<()> {
        ensure!(
            self.auto <= 1
                && [0x800923f8, 0x80092410, 0x80092428, 0x80092440].contains(&self.rank)
                && [0x80092458, 0x80092468].contains(&self.category)
                && self.weapon <= 2
                && self.armor <= 2,
            "Unsupported sort settings"
        );
        Ok(())
    }
}
pub fn set_keys(raw: &mut [u8], keys: &[u16]) -> Result<()> {
    ensure!(
        keys.len() <= 32
            && keys
                .iter()
                .enumerate()
                .all(|(i, k)| is_key(*k) && !keys[..i].contains(k)),
        "Invalid or duplicate key"
    );
    let mut b = vec![0; 68];
    put16(&mut b, 0, keys.len() as u16);
    for (i, k) in keys.iter().enumerate() {
        put16(&mut b, 4 + i * 2, *k);
    }
    update(raw, "pe_qol.keys", &b, WIN, b"PKEY", 0x6b657973)
}
pub fn set_sort(raw: &mut [u8], s: &SortSettings) -> Result<()> {
    s.validate()?;
    let mut b = vec![0; 24];
    for (i, v) in [s.auto, s.rank, s.category, s.weapon, s.armor, 0]
        .into_iter()
        .enumerate()
    {
        put32(&mut b, i * 4, v);
    }
    update(raw, "pe_qol.sort", &b, 0x260, b"SORT", 0x736f7274)
}
fn update(
    raw: &mut [u8],
    name: &str,
    data: &[u8],
    at: usize,
    magic: &[u8; 4],
    seed: u32,
) -> Result<()> {
    let parsed = ModData::inspect(raw)?;
    ensure!(
        parsed.editable,
        "Damaged or unsupported mod container: editing is disabled"
    );
    // Never overwrite a known identity with an unsupported/newer schema.
    if name == "pe_qol.keys" {
        parsed.keys()?;
    } else {
        parsed.sort()?;
    }
    let legacy = legacy_block(data, magic, seed);
    if !parsed.container && !parsed.records.iter().any(|r| r.id == id(name)) {
        ensure!(
            raw[OFFSET + at..OFFSET + at + legacy.len()].iter().all(|b| *b == 0),
            "Unrecognized data occupies the legacy block; preserved unchanged"
        );
    }
    replace_record(raw, name, 1, data, Some((at, &legacy)))
}

// Shared transaction for schema modules. The schema's owner validates its own
// version/payload before calling; packing, preservation and checksums live here.
pub(crate) fn replace_record(
    raw: &mut [u8],
    name: &str,
    version: u16,
    data: &[u8],
    window: Option<(usize, &[u8])>,
) -> Result<()> {
    let mut parsed = ModData::inspect(raw)?;
    ensure!(
        parsed.editable,
        "Damaged or unsupported mod container: editing is disabled"
    );
    let mut next = raw.to_vec();
    let tail = &mut next[OFFSET..];
    if let Some((at, bytes)) = window {
        ensure!(
            at <= SIZE && bytes.len() <= SIZE - at,
            "Invalid compatibility range"
        );
        ensure!(
            !parsed.container || (at >= WIN && at + bytes.len() <= END),
            "Compatibility write overlaps the container"
        );
        tail[at..at + bytes.len()].copy_from_slice(bytes);
    }
    if parsed.container {
        let rec = Record {
            id: id(name),
            version,
            data: data.to_vec(),
        };
        if let Some(r) = parsed.records.iter_mut().find(|r| r.id == rec.id) {
            *r = rec;
        } else {
            parsed.records.push(rec);
        }
        parsed.records.sort_by_key(|r| r.id);
        let total = HEADER
            + parsed
                .records
                .iter()
                .map(|r| 12 + r.data.len())
                .sum::<usize>();
        ensure!(
            total <= ROOM && parsed.records.len() <= 64,
            "Mod records do not fit; nothing was changed"
        );
        let mut lin = vec![0; ROOM];
        // Preserve the actual writer marker, not the editor's or a guessed API version.
        lin[..HEADER].copy_from_slice(&tail[..HEADER]);
        put16(&mut lin, 6, total as u16);
        put32(&mut lin, 8, 0);
        put32(&mut lin, 12, crc32(&tail[WIN..END]));
        let mut pos = HEADER;
        for r in parsed.records {
            put32(&mut lin, pos, r.id);
            put16(&mut lin, pos + 4, r.version);
            put16(&mut lin, pos + 6, r.data.len() as u16);
            lin[pos + 12..pos + 12 + r.data.len()].copy_from_slice(&r.data);
            let crc = record_crc(&lin[pos..pos + 8], &r.data);
            put32(&mut lin, pos + 8, crc);
            pos += 12 + r.data.len();
        }
        let crc = crc32(&lin[..total]);
        put32(&mut lin, 8, crc);
        scatter(&lin, tail);
    } else {
        ensure!(
            window.is_some(),
            "A legacy save requires an explicit legacy block"
        );
    }
    crate::crc::update_save_crc16(&mut next);
    raw.copy_from_slice(&next);
    Ok(())
}
#[cfg(test)]
#[path = "mod_save/tests.rs"]
mod tests;

#[cfg(test)]
#[path = "mod_save/boundary_tests.rs"]
mod boundary_tests;

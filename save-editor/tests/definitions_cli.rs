//! Definitions are tested in separate editor processes, so no installed game
//! mods, global registry changes or real player saves are involved.
use serde_json::json;
use std::{fs, process::Command};
#[test]
fn standalone_pack_drives_item_effect_listing_and_equipment_creation() {
    let dir =
        std::env::temp_dir().join(format!("pe-editor-definitions-test-{}", std::process::id()));
    fs::create_dir(&dir).unwrap();
    let pack = dir.join("definitions.json");
    let save = dir.join("test-save.bin");
    let export = dir.join("out.json");
    let mut template = vec![0u8; 32];
    template[0] = 3;
    template[1] = 2;
    template[4] = 27;
    template[6] = 4;
    template[7] = 42;
    template[8] = 77;
    template[9] = 9;
    template[10] = 9;
    template[20] = 2;
    template[21] = 0x25;
    template[22] = 0xff;
    let defs = json!({"format_version":1,"items":[{"id":27,"name":"Future custom rifle","description":"An editor-only synthetic definition","category":"Rifle","equipment_template":template}],"effects":[{"id":31,"name":"Future custom effect","description":"Defined without installing a game mod","armor":false,"group":0}]});
    fs::write(&pack, serde_json::to_vec(&defs).unwrap()).unwrap();
    let sample = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("samples/Parasite_Eve_PS1_Sample_Card.srm");
    let mut card = pe_save_editor::memcard::MemoryCardContainer::open(&sample).unwrap();
    fs::write(&save, card.slots[0].as_mut().unwrap().to_bytes()).unwrap();
    let exe = env!("CARGO_BIN_EXE_pe-save-editor");
    let run = |args: &[&str]| {
        let out = Command::new(exe)
            .arg("--definitions")
            .arg(&pack)
            .args(args)
            .output()
            .unwrap();
        assert!(
            out.status.success(),
            "{}",
            String::from_utf8_lossy(&out.stderr)
        );
        String::from_utf8(out.stdout).unwrap()
    };
    let output = run(&["list-items", "--filter", "Future custom"]);
    assert!(output.contains("Future custom rifle") && output.contains("Future custom effect"));
    run(&["edit", save.to_str().unwrap(), "--add-item", "0x1B"]);
    run(&[
        "export-json",
        save.to_str().unwrap(),
        export.to_str().unwrap(),
    ]);
    let value: serde_json::Value = serde_json::from_slice(&fs::read(&export).unwrap()).unwrap();
    let rec = value["equipment"]
        .as_array()
        .unwrap()
        .iter()
        .find(|r| r["item_id"] == 27)
        .unwrap();
    assert_eq!(rec["base_stats"], json!([42, 77, 9]));
    assert_eq!(rec["kind"], 4);
    assert_eq!(rec["tail_data"][0], 0x25);
    assert_eq!(rec["tail_data"][1], 0xff);
    for f in fs::read_dir(&dir).unwrap() {
        fs::remove_file(f.unwrap().path()).unwrap();
    }
    fs::remove_dir(&dir).unwrap();
}

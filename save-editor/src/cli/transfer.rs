//! Transfer commands.
use super::*;

pub(super) fn cmd_import_psyz(source: &PathBuf, target_dir: &PathBuf) -> Result<()> {
    let mut container = MemoryCardContainer::open(source)?;
    let written = container.export_to_psyz_dir(target_dir)?;
    println!(
        "Imported {} Parasite Eve save slot(s) from {} into PsyZ directory {}:",
        written.len(),
        source.display(),
        target_dir.display()
    );
    for p in written {
        println!("  -> {}", p.display());
    }
    Ok(())
}

pub(super) fn cmd_export_card(source: &PathBuf, target_file: &PathBuf) -> Result<()> {
    let mut container = MemoryCardContainer::open(source)?;
    let count = container.active_slot_indices().len();
    container.export_to_ps1_card_image(target_file)?;
    println!(
        "Exported {} Parasite Eve save slot(s) from {} to PS1 memory card image {}.",
        count,
        source.display(),
        target_file.display()
    );
    Ok(())
}

pub(super) fn cmd_export_json(
    source: &PathBuf,
    output: &PathBuf,
    slot: Option<usize>,
) -> Result<()> {
    let container = MemoryCardContainer::open(source)?;
    let idx = resolve_slot_index(&container, slot)?;
    let save = container.slots[idx].as_ref().unwrap();
    let json = serde_json::to_string_pretty(save)?;
    fs::write(output, json)
        .with_context(|| format!("Failed to write JSON to {}", output.display()))?;
    println!(
        "Exported Slot {} ({}) to {}",
        idx + 1,
        save.filename,
        output.display()
    );
    Ok(())
}

pub(super) fn cmd_import_json(
    input: &PathBuf,
    target: &PathBuf,
    slot: Option<usize>,
) -> Result<()> {
    let json_str = fs::read_to_string(input)
        .with_context(|| format!("Failed to read JSON file {}", input.display()))?;
    let mut imported: PeSaveSlot = serde_json::from_str(&json_str)
        .with_context(|| format!("Failed to parse PeSaveSlot JSON from {}", input.display()))?;

    let mut container = MemoryCardContainer::open(target)?;
    let target_idx = if let Some(s) = slot {
        if !(1..=15).contains(&s) {
            bail!("Slot must be 1..=15");
        }
        s - 1
    } else {
        imported.slot_index.min(14)
    };
    imported.slot_index = target_idx;
    let _ = imported.to_bytes();
    container.slots[target_idx] = Some(imported);
    container.save()?;
    println!(
        "Imported JSON save into Slot {} of {}",
        target_idx + 1,
        target.display()
    );
    Ok(())
}

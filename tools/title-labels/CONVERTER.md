# Offline owner-art conversion

`convert-label.py` uses an **existing local Pillow installation** to turn one explicitly owner-authored/generated static PNG into the corresponding uncompressed `.pelabel` container. It does not download dependencies, scan artwork directories, extract game images, generate artwork, resize/crop, patch discs or enable assets in the game. No approved artwork is currently supplied, so no actual PNG conversion was run for this delivery.

Use an existing Python runtime with Pillow installed. This source tool never installs or downloads dependencies:

```powershell
python convert-label.py --help
```

For future **approved owner-authored/generated art only**, create a deliberate new output directory, then use:

```powershell
python convert-label.py --label new-game --input C:/path/to/approved/new-game.png --output-dir C:/path/to/new-title-labels --source-note "Describe the actual author/generation and owner approval here"
```

The example paths are illustrative, not existing or approved input. `--source-note` must truthfully record provenance. Do not feed the private original title contact sheet, extracted game art, copyrighted third-party font graphics without rights, rejected AI versions or unapproved pack images into this tool. It cannot independently verify legal rights or aesthetic approval.

Inputs must be one static PNG, at the exact dimensions in `FORMAT.md`, and at most4MiB. The tool reads bounded source bytes once, and the recorded PNG SHA refers to those same bytes. Pillow converts decoded color channels to straight RGBA8; no extra ICC color-management, retouching or image metadata is embedded. Runtime sees only the32-byte fixed header and raw pixel payload.

The tool writes the canonical filename plus a `.provenance.json` sidecar recording the source note, Pillow version, approved-input hash and output hash. It does not include absolute file paths in that record or runtime header. Existing outputs are never overwritten. Provenance hashes are for those deliberately approved new tool assets, not private original-image fingerprints. Review the actual art and provenance before any later public inclusion or console transfer.

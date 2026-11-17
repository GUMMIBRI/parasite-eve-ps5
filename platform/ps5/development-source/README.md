# Development source patches

Reconstruct the public Build 21 tree with `../release-build21/BUILD21-README.md`. The required 216-byte PointSprite owner input remains external; do not commit that input, its generated initializer, local configuration, original game data, or build products.

From the reconstructed root, `git apply --check <path>/selected-source.patch` and then `git apply <path>/selected-source.patch` applies the source-only battle, dialogue, and title-label changes. `source-pins.json` records the exact local comparison inputs; line endings may differ. This is a partial development source update, not a complete Build 22/23 reconstruction or executable identity. The selected reliability baseline had additional branding and machine-specific build configuration changes that are intentionally not exported. Other source outside the Build 21 selected-file inventory has not been exhaustively reconciled.

`optional-movie-audio-diagnostic.patch` is a separate diagnostic candidate. Check and apply it from the same reconstructed root only if wanted. Its six baseline and candidate hashes are in `optional-movie-audio-pins.json`. Five affected translation units were previously compiled against local PS5 flags; it is not an audio behavior fix, full link, console test, or listening result. Startup/open failure coverage and the SPU output ring remain absent.

No current source patch establishes gameplay or fixes the reported audio or graphics issues. Historical releases and their tested identities remain unchanged.

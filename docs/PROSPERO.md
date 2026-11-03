# Prospero catalog submission notes

The public beta requires user-supplied game discs and optional texture packs. It contains no private save seed or packs. Listing and installation are not additional gameplay qualification.

The icon at `images/parasite-eve-icon.webp` is a lossless WebP encoding of `packaging/icon/parasite-eve.png` (original generation provenance is beside that PNG). It retains all1254x1254 decodedRGBA pixels exactly; no visual redesign, crop or resize. WebP SHA256: `b2a1924d5ba076448b960e71c49c43c45cb8d07fe8c7f56ab491852da5d249a3`,1731126bytes. Encoded using Pillow13.0.0 with lossless and exact enabled. Original image and beta ZIP remain unchanged.

## Save continuity

Beta1 saves, experimental checkpoints and settings live under the installed app's `user` folder. Back up that folder before using store updates. Current ProsperoStore replaces an app folder and retains the old folder under `prosperostore/previous/<TITLEID>` on its working drive; it does not automatically make our old `user` directory active in the new installation. Restore the backed-up `user` folder into the new app folder while the game is closed. Retain an independent backup because another store update may replace the previous-folder backup. Never replace newer saves with older copies without comparing them.

This behavior was inspected statically in ProsperoStore revision22d5a44a053b4dad4f3a5867fe39d262a80a1b8c, `src/install/archive.cpp` and `src/install/transaction.cpp`; store installation/update has not been tested on our console. The installer selects the app by `sce_sys/param.json` and ignores archive entries outside its app prefix. The separate `parasite-eve` folder must still be placed by the user in the documented asset location.

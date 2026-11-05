# Parasite Eve v1.1.5 (Build15)

Parasite Eve 1&2 Port community: https://discord.gg/Yg88tHYXK

Based heavily on khasinski/parasite-eve-pc.

## Changes
- Store-compatible PPSA77022 application folder and matching native title identity. Corrects the package layout that could leave RecompStore's executable one folder too deep and repeat Install instead of Open.
- Save States menu, shortcuts and background checkpoint work removed. Normal memory-card saves remain supported.
- Disc-copy/install wizard and its menu actions removed. Place your own compatible discs in the data folder; the app uses existing files directly.
- Coherent IBM Plex title choices and save/load labels, digits and separators. Select Plex for the menu font; existing Original/Retro preferences are respected.
- Directional surface lighting using original model normals and bones; PM4/PM5 resolved-shot direction and impact attenuation, P109 fire/intensity, and the exact M005 JointBeacon endpoint. Original mode remains available. This is named coverage, not every electrical attack or launch trajectory.
- Generic compatible choice-pointer texture support and bounded shot-light normalization. No third-party texture pack is bundled.

## Installation and data
The store ZIP contains one top-level PPSA77022 folder. Install that folder under homebrew. The included parasite-eve-data folder may stay inside PPSA77022, or be moved as a whole to the root of a supported USB drive or /data.

The discs directory is EMPTY. Supply your own compatible North American disc images as discs/disc1.bin and discs/disc2.bin. No discs, original background reference images or private HD packs are included. PC HD background bundling remains pending verified redistribution permission; compatible owner-supplied packs remain optional.

Keep one complete data folder, or explicitly configure paths when retaining multiple copies. Optional art and discs stay together under the selected data root. The port does not copy or move your discs automatically.

## Existing installations and saves
IMPORTANT: PPSA77022 is a new console identity, replacing this port's previous PEVG00022/PS5GameTest layout. This is not an automatic in-place save migration. Back up the ENTIRE old app/user folder and any personal files inside the app before installing or updating. Keep your old installation and backup until the new one is verified. Restore memory cards/settings deliberately into the new app/user folder; never blindly overwrite newer saves. Store updates may replace app-local files, so keep backups, particularly if you store discs/data inside the app. External USB-root or /data storage keeps those files separate from the app folder.

## Validation and limits
The owner reported the new internal Build15 working and authorized publication. SDK compilation, conversion, imports, SELF integrity and file-transfer hashes passed. These checks do not establish every scene, firmware or performance. The separately reported sig12 error remains unconfirmed; the packaging correction does not establish its cause. Day2–6 chapter cards and complete DuckStation menu-pack coverage remain unfinished.

## Credits and source
Upstream: https://github.com/khasinski/parasite-eve-pc
PS5 source: https://github.com/GUMMIBRI/parasite-eve-ps5

Credit to the original game's creators/rightsholders and upstream contributors; PsyZ, SDL, public PS5 payload SDK/runtime contributors, SynoPiia/PSXS5, mihawk-99's Vulkan/Mesa platform work, Mesa/RADV, LLVM/compiler-rt, Khronos glslang/Vulkan, AMD FidelityFX FSR1, WebP, xxHash, stb, IBM Plex, Chakra Petch and psy-q-decomp contributors. Complete retained component notices and public reconstruction instructions accompany the app. Caller-supplied original build inputs remain outside the public source recipe. No proprietary Sony modules are included. No blanket license grant or Sony/Square Enix endorsement is asserted.

# Native PS5 game test build

The existing native Parasite Eve modules now compile and link with exported PsyZ sources and the public SDL3 PS5 backends. This is an opt-in game target, not a replacement emulator or diagnostic payload. Desktop defaults remain unchanged. No console boot, gameplay, rendering, audio, input or save correctness is established by linking.

Build the pinned public SDK and SDL3 first, then run platform/ps5/build-game.ps1 with the installed SDL3 prefix. PE_PS5_GAME configures the game; generated PsyZ patches select the existing CPU PS1 rasterizer and actual SDL3 software window presentation. GPU-only options fail explicitly. Source/input/library hashes and ELF metadata are recorded. Generated sources and tool installations preserve the pinned submodules. Windows linking uses the SDK linker script with LLVM response files to avoid the wrapper command-length limit.

Console startup defaults to /data/parasite-eve, with PE_DATA_HOME and PE_CONFIG_HOME overrides. Configure both user-owned USA disc images before boot; no desktop image dialog runs on console. Existing configuration/card logic is retained. Touchpad maps to PS1 Select; L3+R3 opens settings in the same software-rendered window. The player must validate these controls and runtime permissions.

The local packager requires a successful matching game ELF manifest. Optional local BIN copying requires a read-only complete disc-archive preflight and source/copy hashes. It creates a new ignored folder and never copies saves or deploys to a console. Retail data and generated packages must never be uploaded to this repository. Original textures load from the user's discs. External DuckStation HD PNG replacement packs are currently unsupported.

Reviewed-library build evidence: public SDL3 library SHA256 c043029a134fe47d11a0c159a37419b335faa296f1409d9c6b45a63ca697fa9c; game ELF4094360bytes SHA256 e21e5ea5bc17cfcc6a7a6d3f560f54e982cc90968a8fb56ff5cb526c8154f5de. Compilation/linking completed; no agent console run occurred. Original game baseline and PsyZ pin remain deliberately preserved while upstream0.7 integration is assessed in a separate staged candidate.

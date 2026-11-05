# Selection and launch background recipe

Public reference: https://github.com/darkxex/ProsperoStore/blob/main/docs/PRESENTATION_ASSETS.md . Its supported profile uses icon0.png512x512, selection pic0.dds and launch pic1.dds, each3840x2160 BC7_UNORM DX10, single2Dsurface, oneMip. A PNG renamed DDS is invalid. ShadowMountPlus source copies supported PNG/DDS metadata, and Prospero refreshes registered sce_sys copies; neither converts arbitrary PNG files into this profile. No caching cause or console visibility is inferred.

The original project-authored prepare-launcher-art.ps1 accepts a caller-supplied composition-approved16:9 PNG, pinned texconv.exe and a fresh output directory. It performs only mechanical scaling/BC7 encoding; no artificial loading delay or image invention. Output pic0.dds and pic1.dds use the same caller image; callers may separately prepare different authorized selection/launch art. Deploy only the two fixed DDS files plus the existing approved icon and stable param.json, never conversion-work or source/editable PNGs. Keep title/save paths unchanged. No owner/private art, game textures or compiled tool is included here.

Tool: Microsoft DirectXTex may2026, MIT. Official asset https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe ; SHA256dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06. See DirectXTex-PROVENANCE.json and DirectXTex-MIT.txt. The script downloads nothing and fails if the caller tool digest differs. Source PNG is bounded64MiB/8192pixels each side and must be16:9. Tool-free syntax validation passed; no conversion or full game build was repeated for this export.

Example: pwsh -File prepare-launcher-art.ps1 -Texconv <caller-texconv.exe> -BackgroundPng <authorized-16x9.png> -OutputDirectory <fresh-local-output>

Human checks remain required for selected-app/launch surfaces and startup flash, with title/FMVs and original timing intact. Format validation is not gameplay qualification.

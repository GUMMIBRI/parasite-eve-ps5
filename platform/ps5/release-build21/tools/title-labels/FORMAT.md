# PETLRGBA v1 — bounded source-bound static label art

This is an **uncompressed** fixed-dimension RGBA container. Runtime does not parse PNG, DEFLATE, chunks, embedded paths, metadata or variable-length tables. Five exact filenames are bound to actual title-label meanings by the existing source-owned title producer.

| Label ID | Native title node | Filename | Width×height | Payload bytes | Exact file bytes |
|---|---|---|---|---|---|
|0|1 Press Start Button|`press-start.pelabel`|576×96|221,184|221,216|
|1|3 New Game|`new-game.pelabel`|336×96|129,024|129,056|
|2|4 Continue|`continue.pelabel`|336×96|129,024|129,056|
|3|5 Tutorial|`tutorial.pelabel`|336×96|129,024|129,056|
|4|6 EX Game|`ex-game.pelabel`|336×96|129,024|129,056|

All header integers are unsigned32-bit **little endian**. Header size is exactly32bytes; no optional extension fields.

| Offset | Size | Required value |
|---|---|---|
|0|8|ASCII `PETLRGBA` (no terminator)|
|8|4|Version1|
|12|4|Header bytes32|
|16|4|Exact width from the source-bound label|
|20|4|Height96|
|24|4|Exact width×height×4 payload bytes|
|28|4|Matching label ID0–4|
|32|payload|Top-down tightly packed straight/unassociated RGBA8 rows; no padding|

The runtime computes expected dimensions and byte count from its fixed label ID, **never from unchecked file arithmetic**. It checks exact file size and all header fields before allocating exactly that known payload. The same stream must deliver the payload exactly, remain the expected size and end immediately afterward; truncation, extra data, wrong versions, swapped-label files or read/close errors fail closed. No allocation size comes from the input header. There is no compression or runtime inflate scratch.

Total successful five-label payload cache is737,280bytes. The explicit pixel-cache ceiling is768KiB; fixed root/filename and small metadata buffers keep the complete owned label cache under1MiB. Only one file/IO object is active at a time. This is not a bound on every inherited renderer/system allocation.

## Rendering and source ownership

Images are optional static label artwork, not whole menu screens. Preserve transparent background and exact bounds; do not bake selection, frames, save names, dynamic numbers, controls or game data into them. No resizing is done by runtime or converter. The compositor premultiplies straight RGB by alpha, applies original intensity/256 and takes per-channel maximum against its source-owned background. It preserves the original title operation rather than adding a conventional alpha-over material renderer; dark pixels cannot darken a bright background.

Only the existing style1 Plex/native graphical-title route requests these images, **after its exact whole native-frame witness succeeds**. Atkinson style3 remains the chosen Atkinson font. Original0/Retro2 keep authored title art. Missing art retains the actual native4x font companion. No approved art is supplied or enabled by this source candidate.

## Explicit root and lifetime

`PE_HD_TITLE_LABELS` may deliberately identify one absolute owner-configured directory. It is not auto-set, does not scan USB/data directories and never discovers or enables a private texture pack. Empty/unset root disables external art. Control characters, backslashes, colons, relative roots, dot/dot-dot segments and excessive length are rejected.

Each canonical file is attempted once per root/title/device lifetime. Failure retains font fallback without per-frame file I/O, allocation, rasterization or retry. A real root change or the existing title/device retirement clears/frees cached payloads and rearms admission. An unchanged root never polls for files newly added while the app is running. GPU transfer uses backend-owned composed output, not borrowed asset pointers; the getter's return is read only during synchronous presentation callbacks on the existing render thread.

PNG files are **never attempted by runtime**. The earlier rejected PNG decoder path is not restored. REJECTED AI artwork is not accepted, converted or activated by this implementation; all actual artwork still needs owner approval and provenance.

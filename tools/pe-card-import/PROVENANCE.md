# Provenance and current native contract

Copyright 2026 PE-PORT contributors. New CLI/build/documentation/verification
files in this directory are licensed under Apache-2.0 (LICENSE).

Copied **byte-for-byte, unmodified**, from the owner's local native project:

| File | Exact source | SHA256 |
|---|---|---|
| memcard.c | `C:\.pe\port-work\native\src\memcard.c` | `22A9AA96E51875A0E9BCB20C991C71750666274E347809449209C764C35886BD` |
| memcard.h | `C:\.pe\port-work\native\src\memcard.h` | `4DA4B9B7E901B53A2A95EFCFE1C6BFE0F0EA4C4218F05DA73ACA4CFE83F4514A` |
| LICENSE | `C:\.pe\port-work\native\LICENSE` | `CFC7749B96F63BD31C3C42B5C471BF756814053E847C10F3EB003417BC523D30` |
| LICENSING.md | `C:\.pe\port-work\native\LICENSING.md` | `695FA2E9595280DB6EA952C113DCCB68BF6926F5CEE91E97622B570B1ED2C181` |

Source repository HEAD at inspection: `ce5d7f4acc75e63ceb2620b5641ae8c0290ded0f`.
All four copied files are **untracked in that repository**; HEAD is context,
not a claim that the files are contained in that revision. The SHA256 values
above identify the actual local source snapshot used. The owner's explicit
reuse authorization and native LICENSING.md cover original project-authored
src code as Apache-2.0, copyright 2026 PE-PORT contributors. No root NOTICE,
COPYING or COPYRIGHT file was present in that source directory. Existing
comments/notices and full LICENSE are retained. LICENSING.md is preserved as
source evidence; its relative paths describe the old native project, not a
claim that its other components ship with this tool.

The adapter describes itself as independently written format logic and cites
Sony FileFormat47.pdf and psx-spx memory-card documentation in memcard.h. Those
citations are preserved verbatim; no SDK header/implementation or original
game code is copied or compiled here. The CLI adds independently written
selection, safety, staging and validation policy around that adapter. Original
game/decompilation code was read only to verify interface requirements.

## Exact diagnostic candidate contract (read-only inspection)

The F494 diagnostic candidate's INPUTS.json identifies upstream merge side
`b1c225aec3065b932b1a35229502c7666c38685c` and SourceRoot:

`outputs/ps5/upstream-20261029T123827Z-a795f5/src`

Its recorded and inspected `src/main/boot/main.c` SHA256 is
`87B0A54337853BE6F60DB9C09FAFFFFB52012A1D7284BFC446CCD943F8AF1366`.
That snapshot's save_flow.c SHA256 is
`B024D6C93FCC08DEBE2063A3C6B8451B7CE75FE1A94A774FD4536D15D74E1ACC`.
The working-tree main.c is different and was not used as proof of the delivered
candidate. No candidate, startup or crash file was modified by this task.

In that exact snapshot:

- Host_CardPath accepts bu port 0/1, device 0..3, mapping individual files under
  the configuration file's parent `/memory-cards/buXY/`.
- PS5 defaults data root to `/data/parasite-eve`, allows PE_DATA_HOME override,
  and sets PE_CONFIG_HOME to that root if not already supplied.
- Host_ImportCard accepts filename length 20, an **11-byte** comparison against
  `BASLUS-00662`, and regular files exactly 8192 bytes from legacy bu00/bu10
  directories. It does not parse full cards. Its completed-import marker means
  copying new legacy directories later is not a reliable import mechanism.
- The save menu compares the **12-byte** product prefix and byte 19 in A..O;
  the filename writer uses `bu%d0:BASLUS-00662000000%c%c`. This tool requires
  the full 12-byte USA product prefix, safe names and nonconflicting slots.
- The writer produces an SC header with one block and icon display 0x11.
  This tool accepts valid SC display values 0x11..0x13, without interpreting
  or modifying title, icons, PE payload or gameplay state.

The then-delivered `C:\.pe\PS5 Current\eboot.bin` was read-only hashed as
`F4944A0E20634F3B38D5EA3B61E07724CA0D55E9327A4BACBDAC1D3DF9E90F87`.
Its startup/crash investigation remains independently owned and unresolved.

## Individual MCS/raw extension, 2026-10-29

Independently authored Apache-2.0 CLI extension and `verify-mcs.ps1`, reviewed
from `C:\.pe\ps5-native\mcs-import`. The adapter files and original licensing
notices above remain byte-for-byte unchanged. No Rust code was copied.

Format evidence: [psx-spx MCS layout](https://psx-spx.consoledev.net/controllersandmemorycards/#mcs-files-single-save-format)
describes the directory frame plus data blocks and embedded filename/size;
[directory frames](https://psx-spx.consoledev.net/controllersandmemorycards/#directory-frames-block-0-frame-115)
documents the offsets and XOR. The inspected native upstream
`save-editor/src/memcard.rs` at
`b1c225aec3065b932b1a35229502c7666c38685c` distinguishes 0x2080-byte MCS and
0x2000-byte raw slots. It was read as format evidence only.

The extension accepts only one-block USA PE: strict 8320-byte MCS with valid
directory XOR and embedded safe name, or 8192-byte raw with explicit safe name.
Off-card directory state/chain fields are ignored. SC/icon/count checks and
the original no-overwrite staging policy remain. This is not all-format MCS,
multiblock, repair, payload checksum, authenticity or gameplay validation.

Reviewed main.c SHA256:
`94A934319B1C0BB9740E453E363FECF56BE263C7E547B0185BD7BB7DC92579EE`.
The isolated handoff reports Zig0.15.2 strict compilation and 36 new plus 96
existing synthetic assertions passed. Those are prior candidate results,
not a new integration test run. Publication rebuilds the same reviewed source.
Baseline source and prior published tool are preserved in
`C:\.pe\ps5-native\mcs-import\integration-20261029`; publication hashes and
build invocation are recorded there. No fixtures or owner saves are published.

The historical F494 contract above remains diagnostic context. The later
startup absolute-path adaptation is documented separately in
`C:\.pe\ps5-native\startup-paths-20261029\REPORT.md`; this tool does not change
that application or its paths. No PS5 Current files are changed here.


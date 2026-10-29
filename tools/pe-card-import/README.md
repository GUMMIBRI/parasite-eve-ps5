# PE card import (standalone host tool)

Lists an explicitly selected PS1 memory card read-only, or stages compatible
USA Parasite Eve saves for manual transfer. It never searches for cards,
contacts a console, changes a source card, repairs a save, or changes game code.

```powershell
.\pe-card-import.exe --help
.\pe-card-import.exe --card 'D:\backups\my-card.mcr'
.\pe-card-import.exe --card 'D:\backups\my-card.gme' --extract --port 1 --output 'D:\transfer\new-card-export'
```

`--list` is the default. `--port 0` is the default target (bu00); port 1 means
bu10. The output root must not exist, including an empty directory, and its
parent must already exist. All selected saves are read and validated into
memory before any output is created. Files are opened exclusively. Failed
writes can leave a partial new root; retain it for diagnosis or remove it
yourself, then choose another fresh root. Only transfer an extraction with
`COMPLETE.txt`, published after every save has been flushed and closed.
An interrupted extraction may instead have `INCOMPLETE.txt`.

The transfer root is the directory named by `--output`. It contains:

```text
new-card-export/
  COMPLETE.txt
  memory-cards/
    bu00/ (or bu10/)
      BASLUS-006620000000A (example: exact 8192-byte save)
```

Back up console saves first. Manually copy missing individual files from this
tree into `/data/parasite-eve/memory-cards/bu00` or `bu10`, or the corresponding
`PE_CONFIG_HOME/memory-cards` directory. `PE_DATA_HOME` can change the default
data/configuration root. Skip existing names and newer saves; never replace
the whole console directory. Do not copy `COMPLETE.txt` as a save. Keep original
card images and backup copies. This tool performs no console writes.

Accepted containers are exactly 131072-byte raw MCR/SRM or 134976-byte DexDrive
GME with the adapter's 12-byte `123-456-STD\0` marker and 3904-byte wrapper.
MC header and all directory frame XOR checksums/states must pass. Every active
chain is checked for valid links, states, size, overlap, cycles and orphans,
including other games' active chains. Deleted entries and free/unavailable
blocks are explicitly skipped; deleted chains are not recovered or followed.
Other games/regions are skipped after their active chains validate.

Selected USA saves require a terminated 20-byte safe ASCII filename, the
12-byte `BASLUS-00662` product prefix, slot A through O at byte 19, exactly one
8192-byte block, SC magic, icon flag 0x11 through 0x13 and header block count 1.
ASCII letters, digits, `_` and `-` are the only filename characters accepted.
Names and bytes are retained exactly. Duplicate names or saves targeting the
same native slot reject the entire extraction. Malformed PE candidates and
multiblock PE saves also reject the entire extraction. Other games may have
valid multiblock chains, but are never extracted.

This is structural compatibility validation, not PE payload/CRC validation,
gameplay qualification, bad-sector remapping/recovery, or authenticity checking.
Unsupported formats include individual native saves, VMP/VGS/PSV and other
wrapped/encrypted cards. Only the Windows host build has been executed; the C
source also provides POSIX directory and exclusive-completion publication.
Do not run an extraction in a directory concurrently controlled by another
writer. Completion flushes C streams; it does not guarantee power-loss durability.

Build independently of the game (no SDK or game assets needed):

```powershell
$cmake = 'C:\.pe\tools\cmake\cmake-4.4.4-windows-x86_64\bin\cmake.exe'
& $cmake -S tools/pe-card-import -B C:/.pe/ps5-native/tools/pe-card-import -G Ninja `
  -DCMAKE_MAKE_PROGRAM=C:/.pe/tools/ninja/ninja.exe `
  -DCMAKE_C_COMPILER=C:/.pe/tools/zig/zig-x86_64-windows-0.15.2/zig.exe `
  -DCMAKE_C_COMPILER_ARG1=cc -DCMAKE_BUILD_TYPE=Release
& $cmake --build C:/.pe/ps5-native/tools/pe-card-import
```

On POSIX: `cmake -S tools/pe-card-import -B /tmp/pe-card-build` then
`cmake --build /tmp/pe-card-build` with an existing C11 compiler.

Optional necessary verification uses only generated synthetic fixtures and an
explicit fresh temporary directory, never real cards or game assets:

```powershell
.\tools\pe-card-import\verify.ps1 `
  -Executable C:\.pe\ps5-native\tools\pe-card-import\pe-card-import.exe `
  -WorkDirectory "$env:TEMP\pe-card-synthetic-$([guid]::NewGuid().ToString('N'))"
```

See PROVENANCE.md and LICENSE for reuse and exact source identity. Integration
and Git operations belong to the root manager; this tool is not added to the
game's top-level CMake build.


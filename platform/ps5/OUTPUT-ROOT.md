# Isolated source and native build outputs

`prepare-upstream-source.ps1` and `build-game.ps1` accept `-OutputRoot`.
Selection order is the nonempty parameter, `PE_PS5_OUTPUT_ROOT`, then the portable
repository default `outputs/ps5`. Relative paths resolve against the caller's
current PowerShell location. A missing root is created; a file path is rejected.
Use a local filesystem directory writable by the invoking user.

Each fresh invocation creates its own timestamp plus full GUID child (`upstream-`
or `game-`). Existing candidates, tools, source checkouts and delivered folders
are not moved. This option selects output storage only; it does not relocate
SDK, LLVM, SDL, application runtime inputs or game data.

```powershell
$env:PE_PS5_OUTPUT_ROOT = 'C:\.pe\ps5-native\builds'
./platform/ps5/prepare-upstream-source.ps1 -Revision <public-commit> -PsyZRevision <validated-psyz-commit>
# Read CANDIDATE.json; use only a ready candidate with no unresolved conflicts.
./platform/ps5/build-game.ps1 -SourceRoot '<candidate>\src' -PsyZRevision <validated-psyz-commit> -OutputRoot 'C:\.pe\ps5-native\builds'
```

Application builds still require their existing explicit `-ApplicationTooling`
and diagnostics switches. Source provenance, SDK/runtime checks and candidate
revision matching are unchanged. This is compilation, not package delivery or
console validation.

`-Resume` always selects its existing tree and ignores the environment variable.
Combining explicit `-OutputRoot` with `-Resume` is rejected to avoid ambiguous
output selection. All existing resume ABI/diagnostic/source guards still apply.

The optional HD renderer diagnostic patch scripts retain their repository
`outputs` confinement guard. They cannot instrument a stage under an external
output root until a separate allow-root review; this change does not relax that
guard. Ordinary native game/application compilation does not invoke them.


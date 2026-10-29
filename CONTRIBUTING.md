# Contributing

This is a native C11 project. Preserve the intended game behavior and data
formats while improving the code for modern hosts. There is no byte-match
requirement, stock PS1 compiler requirement, PS1 checksum gate, source
manifest requirement or decompilation progress/debt ratchet.

## Development workflow

1. Use subsystem headers for shared declarations and ordinary C types and functions.
2. Remove or simplify unnecessary PS1 APIs; use native C or our PsyZ fork where needed, preserving gameplay behavior.
3. Build with `cmake -S . -B build -DBUILD_TESTING=ON` and `cmake --build build --parallel`.
4. Run `ctest --test-dir build --output-on-failure` (set `PE_TEST_DISC` and `PE_TEST_DISC2` to the USA Disc 1 and Disc 2 `.cue` files to include asset and gameplay regression tests).
5. Run `./build/parasite-eve-pc --disc <Disc 1.cue> --headless --frame-limit 8000 --dump-vram /tmp/pe.vram` for reproducible headless boot checks.
6. Validate changed game behavior with focused native tests or reproducible gameplay scenarios.

CI builds the native target and runs the standalone test suite across macOS,
Linux and Windows without retail assets. Use a fresh build directory when
changing build structure or dependencies.

Do not preserve register assignments, instruction scheduling, padding,
linker order or code addresses for compiler matching. Assembly that performs
real work must be translated or replaced with portable C; do not replace
functionality with no-op stubs.

Serialized assets and save data retain their defined widths, offsets and
endianness. Host pointers must use pointer types, and host structures can be
designed independently of the original RAM layout. Validate layout assertions
against serialized formats rather than forcing host pointers into 32 bits.

Upstream is a source of optional fixes and knowledge. Imports must fit the
native architecture and behavior; they need no PS1 binary validation.

See [architecture](docs/ARCHITECTURE.md), [conventions](docs/CONVENTIONS.md) and
[code organization](docs/CODE_ORGANIZATION.md).

Feature completion requires behavior assertions and passing results for the
affected content and platforms; a skipped asset test or a source-only build
cannot establish full-game readiness.

PS1 hardware emulation is not an acceptance requirement. Statically linked room
modules and ordinary host buffers replace overlays and scratchpad memory.
The asset layer mounts both user-supplied CUE/BIN images as one logical set;
no physical CD drive or manual disc swapping is required. Preserve logical
asset identity, state/reset semantics and gameplay-visible event ordering.
High-resolution rendering (`1x`–`8x`) is provided through PsyZ.

The PsyZ submodule points to
[khasinski/parasite-eve-psyz](https://github.com/khasinski/parasite-eve-psyz).
Generic missing runtime/render/audio features can be implemented there with
library tests and game regression tests. Pin an explicitly verified commit in
the parent project; do not consume a moving branch implicitly. Fetch upstream
from `Xeeynamo/psyz`, integrate deliberately, and send reusable fixes upstream
as patches or through a fork-network PR branch when appropriate.

## PS5 fork contributions and review

This repository targets an experimental retail/homebrew PS5 port. Preserve the
upstream desktop build and keep platform changes isolated. Fetch
khasinski/parasite-eve-pc before integration batches and record the upstream
revision; do not consume a moving PsyZ revision implicitly. No console success
may be inferred from desktop compilation.

Use a feature branch and PR. Explain behavior, source provenance, build evidence,
console limitations and any new tools/downloads/network access. Review the latest
revision with Codex, resolve findings and obtain the required maintainer approval
before merging. Treat inability to obtain a review as pending, never approval.
See SECURITY.md for the settings that must be enforced independently in GitHub.

Do not include retail game data, HD packs, saves or proprietary SDKs. Local legacy
Windows work may supply useful fixes or assets with provenance retained; preserve
recoverable originals and compare asset quality/compatibility before selection.

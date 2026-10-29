# Read-only finite pack provider

`pe_hd_pack.c,h` supplies `pe_hd_index_read` for an explicit caller root. It is
default OFF (`PE_PS5_HD_PACK`), also available from opt-in `pe-hd-staging`.
No game/root CMake or renderer hooks, scans, hashing, preload, writes or PNG
validation are introduced. Only synthetic tests create/remove their own fixtures.

## Authority and limitation

Creation requires explicit `PE_HD_PACK_TRUSTED_IMMUTABLE_ROOT`; zero/unknown policy
fails. The caller guarantees root, ancestors and all descendants are trusted and
immutable, with ordinary files/directories and no symlinks, reparse points or mount
escapes. This is a **caller precondition**, not a verified security property.
Do not use this backend for an untrusted or concurrently changed pack. Relative
path ASCII/component checks reject traversal, absolute paths, backslashes, colons,
empty/dot/dot-dot components and trailing dots. These checks do not enforce root
confinement against filesystem indirection or mutation. No realpath-prefix claim.

The public SDK v0.45 installed declarations expose `open`, `O_NONBLOCK`,
`fstat`, `S_ISREG`, `read`, `close`; these are the PS5 backend's actual operations.
`openat` and `O_NOFOLLOW` also appear in its headers, but this provider does not
assume header presence establishes runtime support, nor promise that component
walking prevents every rename/mount escape. A separate hardened backend would
need a reviewed opened-root/descendant authority model and actual runtime/import
evidence. Windows synthetic host support uses `_open` with binary mode,
`_fstat64`, `_read`, `_close`; it makes the same trusted-root requirement.

Public API provenance: [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk),
installed v0.45 `target/include/sys/fcntl.h` (flags/open),
`target/include/sys/stat.h` (regular-file test/fstat),
`target/include/unistd.h` (read/close). These are reference declarations; no
filesystem implementation was copied. Original provider/test code follows
repository LICENSE.md, with no new blanket open-source license.

## Bounds and lifetime

Provider owns fixed metadata reported by `pe_hd_pack_metadata_bytes()` (root
storage plus 32 slots). Creation copies an absolute root shorter than 1024 bytes;
Windows roots use forward-slash drive paths. Metadata/allocator overhead is
separate from the explicit live encoded allocation budget (at most 256 MiB).
Each callback allocates exactly file size and reports that size as retained bytes;
index and cache count that same allocation, not duplicate memory. Each read is
limited by callback allowance, provider remaining budget and 32 MiB encoded cap.
No persistent descriptors or per-file preload. Full joined path is fixed stack
storage; no unbounded string traversal or growth loop.

Caller serializes reads and close. In a pipeline, reads occur on its read worker;
the staging owner creates the pack and transfers callback/user to pipeline create.
Pin release can occur concurrently on the decode worker and publishes slot reuse
only after freeing bytes/accounting. Every successful callback transfers exactly
one pin; every failure zeroes output and retains nothing. Never copy owning pins
or release twice. A pin's bytes are immutable until its release returns.

Reads open read-only/nonblocking on POSIX, reject nonregular/empty/oversize files,
use chunks up to 64 KiB, reject errors/short EOF, then probe EOF and recheck size.
Observed growth/truncation falls back; same-size mutation cannot be detected and
violates the immutable-root precondition. EINTR is a fallback, not an unbounded
retry. Ordinary disk/storage calls have no hard wall-clock timeout guarantee.
Close errors fall back; close is not retried because descriptor state may be
ambiguous. No logging or filesystem writes from the provider.

Shutdown order: stop requests/polls; close/drain pipeline, release held cache
leases and retry close until CLOSED; destroy any other indexes referring to pack;
join any release calls; then close pack. Pack BUSY preserves it while pins remain.
An idle index retains the callback user even with no pins, so pack cannot detect
that reference: caller must honor shutdown order. A decoded lease can survive
encoded pin release; pipeline remains responsible for its own close/lease safety.

## Synthetic evidence and remaining scope

`tests/check_pack.c` exercises path/size rejection, nonregular directory rejection,
zero policy/budget, 32 live pins, immutable bytes, cross-thread release, exhausted
budget/reuse and BUSY close. It also uses a real pipeline to read/decode the
synthetic PNG, invalidate its generation and retain a lease through close/retry.
Host validation runs fixtures in a fresh test-created directory under build cwd.
SDK builds compile/link the same check without executing it. No symlink-race,
allocation-fault or I/O-fault injection proof is claimed. No console execution.

Remaining: bounded manifest ingestion; renderer key/hash identity and immutable
snapshot ownership; UV/mask/target-resolution policy; optional hardened backend
for untrusted roots. This provider authorizes neither matching nor drawing.


# Bounded manifest ingestion (default OFF)

`pe_hd_manifest_create` consumes caller-owned immutable bytes and creates the
existing immutable index. This independent format is **not** YAML or a claim of
compatibility with any replacement-pack configuration. No filesystem scan,
provider read, image decode, renderer binding or hash computation occurs.

Exact ASCII TSV schema, LF only, mandatory final LF:

```text
PE-HD-MANIFEST<TAB>1<LF>
file<TAB>relative/path/texpage-P4-0000000000000001-0000000000000002-4x4-0-0-4x4-P0-15.png<LF>
```

The notation above represents actual tab/newline bytes. Header alone is a valid
empty index. No comments, blank lines, BOM, CRLF, escaping, extra fields or NULs.
Version is exactly `1` (no signs, whitespace or leading zeros). Record order has
no precedence. Basenames use the existing strict filename parser, including
checked decimal uint16 fields and exactly 16 hexadecimal digits per opaque hash.
Index canonical relative-path rules apply, including no traversal, absolute or
backslash paths. Equal parsed keys, including case/decimal aliases in different
folders, reject the entire manifest as DUPLICATE. No hash algorithm inferred.

Caller supplies nonzero max_bytes (hard cap 16MiB), max_line (hard cap 516 bytes,
excluding LF), scratch_bytes (hard cap 256MiB), and all index budgets. At most
65536 items; paths at most 511 bytes. Two bounded passes precede index creation;
scratch is only count*sizeof(pe_hd_index_record), checked by division before
multiplication. Input, allocator overhead, provider/runtime storage and index
storage are separate; index storage/path/pin budgets remain enforced. Peak owned
requested storage is bounded by scratch_bytes + index.storage_bytes, excluding
allocator overhead. Immutable bytes must remain stable for the whole call.

On any failure output is unchanged; temporary storage is freed. INVALID covers
malformed/truncated text/path/filename, LIMIT covers configured/hard budgets,
MEMORY covers allocation failure, DUPLICATE is whole-manifest alias rejection.
Input bytes can be released after success; index copies paths and keys. Callback
and user/root must outlive index and any transferred pins. Choose the explicit
trusted immutable-root policy in PACK.md; parsing grants no filesystem authority
or PNG validity. Blocking provider reads belong on pipeline worker, never here.

Include manifest.cmake with PE_PS5_HD_MANIFEST=ON or the default-OFF staging
aggregator. pe-hd-manifest publicly links pe-hd-index. No root CMake changes.
Synthetic check_manifest.c exercises truncations, numeric overflow, path and NUL
rejection, duplicate aliases, byte/line/item/path/scratch/storage budgets, empty
index and real index pin/BUSY/release. No private fixtures or console execution.
Original implementation under repository LICENSE.md; no external parser copied.


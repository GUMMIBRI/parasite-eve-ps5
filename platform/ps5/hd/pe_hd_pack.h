#ifndef PE_HD_PACK_H
#define PE_HD_PACK_H
#include "pe_hd_index.h"

#define PE_HD_PACK_ROOT_BYTES 1024u
#define PE_HD_PACK_PINS 32u
typedef struct pe_hd_pack pe_hd_pack;
typedef enum pe_hd_pack_result {
    PE_HD_PACK_OK, PE_HD_PACK_INVALID, PE_HD_PACK_LIMIT,
    PE_HD_PACK_MEMORY, PE_HD_PACK_BUSY
} pe_hd_pack_result;
typedef enum pe_hd_pack_policy {
    PE_HD_PACK_POLICY_UNSET = 0,
    PE_HD_PACK_TRUSTED_IMMUTABLE_ROOT = 1
} pe_hd_pack_policy;
typedef struct pe_hd_pack_config {
    const char *root; /* explicit absolute root, copied; no trailing slash except / */
    size_t pinned_bytes; /* live encoded allocations, <= PE_HD_MAX_TOTAL_BYTES */
    pe_hd_pack_policy policy;
} pe_hd_pack_config;
/* This backend is NOT a sandbox. Caller must guarantee root AND ancestors AND
 * all manifest descendants are trusted, immutable regular files/directories,
 * with no symlinks/reparse points/mount escapes, for the entire provider life.
 * It cannot atomically enforce that policy. UNSET/unknown policy fails closed.
 * Do not use with untrusted or concurrently edited packs. POSIX open is
 * nonblocking, but ordinary storage I/O has no wall-clock deadline guarantee.
 * Fixed metadata is sizeof provider (root + 32 slots); no scan/preload/writes.
 * Caller serializes create/read/close; release may run on another worker.
 * All blocking reads must occur on pipeline worker, never renderer/frame path.
 */
pe_hd_pack_result pe_hd_pack_create(const pe_hd_pack_config *, pe_hd_pack **out);
size_t pe_hd_pack_metadata_bytes(void);
/* pe_hd_index_read callback. Rejects unsafe relative ASCII paths independently
 * of index. false zeros out, retains nothing. true transfers one immutable
 * allocation; caller must invoke release exactly once, never copy ownership.
 * Bound by limit, encoded ceiling, provider byte budget and fixed pin slots.
 * No retry on EINTR/short EOF: read failure falls back. No image validation.
 */
bool pe_hd_pack_read(void *pack, const char *path, size_t limit, pe_hd_index_bytes *out);
/* First close/drain pipeline and destroy every index referring to this pack.
 * BUSY preserves object while pins exist; retry after their release completes.
 * Must stop new reads before close. OK frees object, no calls afterward.
 */
pe_hd_pack_result pe_hd_pack_close(pe_hd_pack *);
#endif


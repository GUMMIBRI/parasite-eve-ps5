#ifndef PE_HD_INDEX_H
#define PE_HD_INDEX_H
#include "pe_hd_filename.h"
#include "pe_hd_cache.h"

#define PE_HD_INDEX_MAX_ITEMS 65536u
#define PE_HD_INDEX_MAX_PATH 512u
#define PE_HD_INDEX_PINS 32u
typedef struct pe_hd_index pe_hd_index;
typedef enum pe_hd_index_result {
    PE_HD_INDEX_OK, PE_HD_INDEX_INVALID, PE_HD_INDEX_LIMIT,
    PE_HD_INDEX_DUPLICATE, PE_HD_INDEX_MEMORY, PE_HD_INDEX_MISSING,
    PE_HD_INDEX_BUSY, PE_HD_INDEX_READ
} pe_hd_index_result;
typedef struct pe_hd_index_record {
    const char *path;
    size_t length; /* excludes NUL; basename must parse with PeHd_ParseFilename */
} pe_hd_index_record;
typedef struct pe_hd_index_config {
    size_t max_items, path_bytes, storage_bytes;
    size_t pinned_bytes; /* all live provider backing allocations, <=256MiB */
} pe_hd_index_config;
typedef struct pe_hd_index_bytes {
    const void *data;
    size_t length, retained_bytes;
    void (*release)(void *user);
    void *user;
} pe_hd_index_bytes;
/* Staging-only synchronous callback. On true, transfers one immutable pin to
 * index; retained_bytes includes ALL retained backing storage and <=limit.
 * On false, retains nothing and leaves out zero. Must be finite/non-reentrant.
 * release runs once, possibly on cache worker; must be bounded/thread-safe and
 * must not access index/cache. Provider owns safe root resolution (including symlinks) and
 * must not treat the supplied relative path as authority outside its root.
 */
typedef bool (*pe_hd_index_read)(void *user, const char *path, size_t limit,
                                 pe_hd_index_bytes *out);

/* Copies records/parsed keys into immutable storage. Accepts only canonical
 * relative ASCII paths: A-Z a-z 0-9 _ - . and / separators; no empty, dot or
 * dot-dot components, absolute paths, backslashes, colons or trailing dots.
 * Case is preserved. Equal parsed keys (including hex-case/decimal-zero aliases)
 * reject the WHOLE manifest, independent of order; no first/last-wins ambiguity.
 * No scanning, hashing, filesystem access, image preload or PNG validation.
 * storage_bytes bounds index + sorted entries + copied paths (allocator overhead
 * and caller/provider storage excluded). path_bytes is aggregate including NULs.
 * All limits explicit/nonzero, storage <=256MiB. out is unchanged on failure.
 */
pe_hd_index_result pe_hd_index_create(const pe_hd_index_config *,
    const pe_hd_index_record *, size_t count, pe_hd_index_read, void *user,
    pe_hd_index **out);
/* Fieldwise key comparison; ignores padding. No hash algorithm inferred. */
bool pe_hd_index_key_equal(const PeHdFilename *, const PeHdFilename *);
/* Serialize create/pin/destroy on one staging owner; never render path.
 * Success supplies an owning source; do not copy it or invoke done twice.
 * Cache submit(copy=0) PENDING transfers pin; ALL submit failures leave caller
 * owning pin. For copy=1, caller always invokes done after submit returns.
 * Without transfer caller invokes source.done(source.user) exactly once.
 * Source pointer immutable until done; retained_bytes counts against BOTH
 * provider and cache budgets (not additional allocations). done may run on
 * cache worker concurrently with pin. out unchanged on failure. LIMIT means
 * exhausted byte budget or encoded size ceiling; BUSY means all pin slots
 * occupied; READ means callback returned false.
 * Keys must come from reviewed producer; parsing does not prove renderer match.
 */
pe_hd_index_result pe_hd_index_pin(pe_hd_index *, const PeHdFilename *, pe_hd_source *out);
/* BUSY preserves index while pins/callbacks exist. Owner must stop new pin calls
 * before destroy; retry after cache drains. OK frees index. No calls afterward.
 * No renderer generation tracking: caller must coordinate cache generation and
 * invalidate/release tickets on scene/reload boundaries before integration.
 */
pe_hd_index_result pe_hd_index_destroy(pe_hd_index *);
#endif


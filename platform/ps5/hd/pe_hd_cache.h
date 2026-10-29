#ifndef PE_HD_CACHE_H
#define PE_HD_CACHE_H
#include "pe_hd_image.h"

#define PE_HD_CACHE_SLOTS 16u
#define PE_HD_CACHE_LEASES 32u
typedef struct pe_hd_cache pe_hd_cache;
typedef struct pe_hd_ticket { uint64_t serial; unsigned slot; } pe_hd_ticket;
typedef struct pe_hd_handle {
    const unsigned char *rgba;
    uint32_t width, height;
    size_t bytes;
    uint64_t serial;
    unsigned lease;
    const pe_hd_cache *owner;
} pe_hd_handle;
typedef enum pe_hd_cache_result {
    PE_HD_CACHE_READY, PE_HD_CACHE_PENDING, PE_HD_CACHE_INVALID,
    PE_HD_CACHE_FULL, PE_HD_CACHE_BUSY, PE_HD_CACHE_CLOSED
} pe_hd_cache_result;
typedef struct pe_hd_cache_config {
    size_t total_bytes, decode_bytes, owned_encoded_bytes, borrowed_encoded_bytes;
} pe_hd_cache_config;
/* Only a reviewed immutable index/provider may supply these bytes. No paths are
 * interpreted. Borrow requires an immutable pin until done(user) on the worker;
 * callback is non-reentrant, bounded and must not access this cache. Copy mode
 * retains no pin. Missing/invalid provider entries should never be submitted. */
typedef struct pe_hd_source {
    const void *png;
    size_t length;
    size_t retained_bytes; /* borrow: full pinned backing allocation >=length */
    void (*done)(void *user);
    void *user;
} pe_hd_source;

/* Metadata sizeof(cache), explicit encoded partitions and decode heap (scratch,
 * headers, retained pixels) must fit total <=256MiB. SDL synchronization/thread
 * runtime storage, libc allocator implementation overhead and caller/provider
 * index storage are explicitly EXCLUDED (this is not a process RSS cap);
 * caller must reserve those separately. Borrowed retained_bytes count against their
 * partition despite not being cache allocations. No hidden encoded allocation. */
size_t pe_hd_cache_metadata_bytes(void);
pe_hd_cache *pe_hd_cache_create(const pe_hd_cache_config *config);
/* Staging only: preflight + optional bounded malloc/copy; never frame path.
 * Success PENDING transfers borrow pin; any failure leaves ownership with caller.
 * generation must equal current nonzero generation (initially 1).
 * Tickets are per-request identities, not filename/hash keys or deduplication. */
pe_hd_cache_result pe_hd_cache_submit(pe_hd_cache *, uint64_t generation,
    const pe_hd_source *, int copy, pe_hd_ticket *out);
/* Frame-side try-lock only; never decode/wait/allocate. READY acquires one lease.
 * Every READY must have exactly one successful release. Other results mean use
 * original texture. Do not copy/alter owning handles; pixels immutable while held.
 * Poll may acquire additional independent leases for the same ready ticket. */
pe_hd_cache_result pe_hd_cache_acquire(pe_hd_cache *, pe_hd_ticket, pe_hd_handle *out);
/* BUSY leaves handle owned: retry later. Success zeros it; pointer then invalid.
 * Fixed release queue has one reserved position per lease, so cannot overflow. */
pe_hd_cache_result pe_hd_cache_release(pe_hd_cache *, pe_hd_handle *);
/* Administrative staging operation, not render path. Strictly increasing;
 * invalidates tickets/results, never held pixels. No generation wrap allowed. */
pe_hd_cache_result pe_hd_cache_generation(pe_hd_cache *, uint64_t next);
/* Owner thread, with no concurrent admin calls. Stops acceptance immediately.
 * BUSY means consumer leases still exist: keep cache alive, release and retry.
 * Final call wakes/joins worker (may wait for finite decode/callback) and frees
 * context/cache only after worker frees all pixels. No calls after CLOSED.
 * Do not call on render path, worker/callback, or concurrently with submit/acquire.
 * Release may be retried after BUSY. Before retrying close, join/serialize all
 * release calls with the owner: a zeroed handle does not prove release returned.
 * Final freeing close must not race release. No force-destroy or dangling handles. */
pe_hd_cache_result pe_hd_cache_close(pe_hd_cache *);
#endif


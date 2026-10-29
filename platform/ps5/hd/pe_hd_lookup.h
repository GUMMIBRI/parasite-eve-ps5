#ifndef PE_HD_LOOKUP_H
#define PE_HD_LOOKUP_H
#include "pe_hd_pipeline.h"
#include "pe_hd_generation.h"

#define PE_HD_LOOKUP_SLOTS 16u
#define PE_HD_LOOKUP_DEPENDENCIES 4u
typedef struct pe_hd_lookup pe_hd_lookup;
typedef struct pe_hd_lookup_token { uint64_t serial; unsigned slot; } pe_hd_lookup_token;
typedef enum pe_hd_lookup_result {
    PE_HD_LOOKUP_READY, PE_HD_LOOKUP_PENDING, PE_HD_LOOKUP_FALLBACK,
    PE_HD_LOOKUP_BUSY, PE_HD_LOOKUP_FULL, PE_HD_LOOKUP_INVALID
} pe_hd_lookup_result;
typedef struct pe_hd_lookup_proof {
    const pe_hd_generation *tracker;
    uint64_t checkpoint, state; /* snapshot epoch and nonzero sampling-state ID */
    unsigned count; /* Complete packed-word AND CLUT coverage, 1..4. */
    pe_hd_generation_rect dependencies[PE_HD_LOOKUP_DEPENDENCIES];
} pe_hd_lookup_proof;
typedef struct pe_hd_lookup_observation {
    /* Independently observed identity fields; hashes in this struct are ignored.
     * Units must be established by the caller's reviewed key producer. */
    PeHdFilename texture;
    uint32_t output_width, output_height;
    unsigned unsupported; /* MUST flag any filter, window, STP/mask/blend or
                           * partial/subrect usage requiring unapproved semantics. */
} pe_hd_lookup_observation;

/* Staging allocation only. Borrows an exclusively administered pipeline at its
 * initial generation 1. No other request/forget/generation/close calls until
 * destroy. Provider and pipeline lifetimes remain caller-owned. out unchanged
 * on failure; fixed metadata allocation reported here excludes pipeline memory.
 * ALL coordinator calls are serialized on ONE owner, including frame acquire,
 * release and VRAM writes. No concurrent staging/frame calls or internal locks.
 * Never copy coordinator/tokens across coordinator lifetimes. */
size_t pe_hd_lookup_metadata_bytes(void);
pe_hd_lookup_result pe_hd_lookup_create(pe_hd_pipeline *, pe_hd_lookup **out);
/* Staging only: bounded linear coalescing by fieldwise key, metadata and exact
 * proof. May take pipeline mutex, never provider I/O/decode. Requires externally
 * computed opaque key; no hash/matching implementation or renderer hook here.
 * Conservative subset: full-region P4/P8, full CLUT, nearest/no special flags,
 * uniform positive integer output scale. Other observations FALLBACK unchanged.
 * Caller certifies complete dependencies and state identity; cannot be inferred.
 * Terminal failures remain until explicit retire/generation (no retry storm).
 * Tokens are borrowed identities; coalesced callers must coordinate retirement.
 */
pe_hd_lookup_result pe_hd_lookup_prepare(pe_hd_lookup *, const PeHdFilename *,
    const pe_hd_lookup_observation *, const pe_hd_lookup_proof *, pe_hd_lookup_token *out);
/* Frame: no allocation/read/decode/blocking lock. current_state must reflect ALL
 * E2/page/palette/filter changes, nonzero and never reused in this lifetime.
 * Checks live tracker dependencies before every acquire. READY transfers one
 * independent lease into zeroed out; every other result leaves out unchanged.
 * No draw is authorized: alpha/mask/UV/render semantics still need integration.
 * Caller must keep proof/state valid through the draw and fall back for any
 * read-after-write/self-texturing feedback. An acquired lease pins pixels, not
 * the truth of renderer metadata after subsequent writes.
 * Tracker must outlive token retirement; reinit requires generation barrier and
 * discard of all old snapshots/keys/proofs first. Epochs alone cannot detect ABA.
 */
pe_hd_lookup_result pe_hd_lookup_acquire(pe_hd_lookup *, pe_hd_lookup_token,
    uint64_t current_state, pe_hd_handle *out);
/* Release READY zeros owning handle; BUSY retains it for retry, even after
 * token retirement/generation. After destroy use pipeline_release directly.
 * Never copy a lease. */
pe_hd_lookup_result pe_hd_lookup_release(pe_hd_lookup *, pe_hd_handle *);
/* Staging only. Retire invalidates shared token; outstanding external leases
 * survive. Internal rejected-size leases are drained before retirement. BUSY
 * preserves token for retry. Generation strictly increases, never wraps; first
 * drains internal rejected leases then cancels pipeline and clears all tokens.
 */
pe_hd_lookup_result pe_hd_lookup_retire(pe_hd_lookup *, pe_hd_lookup_token);
pe_hd_lookup_result pe_hd_lookup_generation(pe_hd_lookup *, uint64_t next);
/* Staging: retire all tokens and free coordinator; BUSY preserves for retry.
 * Does not close pipeline or free external leases. After success, release those
 * directly through the pipeline, then close it under its documented contract.
 * All token/proof users must stop before destroy. */
pe_hd_lookup_result pe_hd_lookup_destroy(pe_hd_lookup *);
#endif


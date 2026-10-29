#ifndef PE_HD_PIPELINE_H
#define PE_HD_PIPELINE_H
#include "pe_hd_index.h"

#define PE_HD_PIPELINE_REQUESTS 16u
typedef struct pe_hd_pipeline pe_hd_pipeline;
typedef struct pe_hd_request { uint64_t serial; unsigned slot; } pe_hd_request;
typedef enum pe_hd_pipeline_result {
    PE_HD_PIPELINE_READY, PE_HD_PIPELINE_PENDING, PE_HD_PIPELINE_FALLBACK,
    PE_HD_PIPELINE_BUSY, PE_HD_PIPELINE_FULL, PE_HD_PIPELINE_INVALID,
    PE_HD_PIPELINE_CLOSED
} pe_hd_pipeline_result;
typedef struct pe_hd_pipeline_config {
    pe_hd_index_config index;
    pe_hd_cache_config cache;
} pe_hd_pipeline_config;
/* Owns a newly copied immutable index and cache. Provider user must outlive
 * successful close. All reads and releases run on internal workers; callback
 * contracts in index.h apply. No filesystem access or renderer identity inferred.
 * Independent fixed pipeline metadata is reported below; add index storage and
 * cache total plus SDL/runtime/allocator overhead for overall memory planning.
 * Pins count against BOTH index and cache limits, not duplicate allocation.
 * One read at a time, no read buffer allocation beyond provider pin budget.
 * out unchanged on failure. SDL threading must already be available.
 */
size_t pe_hd_pipeline_metadata_bytes(void);
pe_hd_pipeline_result pe_hd_pipeline_create(const pe_hd_pipeline_config *,
    const pe_hd_index_record *, size_t, pe_hd_index_read, void *, pe_hd_pipeline **out);
/* Single staging owner serializes request/forget/generation/close. Initial
 * generation=1, strictly increasing, no wrap. request copies the opaque key,
 * never reads/decodes; no deduplication. FULL requires forget or generation.
 * A request persists until forget/generation; fallback is terminal for it.
 */
pe_hd_pipeline_result pe_hd_pipeline_request(pe_hd_pipeline *, uint64_t,
    const PeHdFilename *, pe_hd_request *out);
pe_hd_pipeline_result pe_hd_pipeline_forget(pe_hd_pipeline *, pe_hd_request);
pe_hd_pipeline_result pe_hd_pipeline_generation(pe_hd_pipeline *, uint64_t);
/* Frame side: try-lock only. READY owns one cache lease; use zeroed handle.
 * All other results mean retain original texture. Multiple polls may each
 * acquire a lease; release every one. Never copy owning handles. Release BUSY
 * leaves ownership intact; retry even after generation or close returned BUSY.
 * Stop concurrent poll calls before close; release may continue after BUSY.
 * Before retrying close, join/serialize all release calls with the staging
 * owner: zeroing a handle does not prove its release call has returned.
 */
pe_hd_pipeline_result pe_hd_pipeline_poll(pe_hd_pipeline *, pe_hd_request, pe_hd_handle *);
pe_hd_pipeline_result pe_hd_pipeline_release(pe_hd_pipeline *, pe_hd_handle *);
/* Staging only: stops/joins read worker (finite callback may block), then drains
 * cache. BUSY preserves pipeline for lease releases and close retry. CLOSED
 * frees pipeline/index/cache; no calls afterward. Final close must not race
 * any release call, including one that has already zeroed its handle.
 * Never call from callbacks.
 */
pe_hd_pipeline_result pe_hd_pipeline_close(pe_hd_pipeline *);
#endif


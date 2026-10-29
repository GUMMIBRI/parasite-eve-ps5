#ifndef PE_HD_GENERATION_H
#define PE_HD_GENERATION_H
#include <stdbool.h>
#include <stdint.h>

enum { PE_HD_GENERATION_COLUMNS = 64, PE_HD_GENERATION_ROWS = 32,
       PE_HD_GENERATION_TILES = 2048 };
typedef struct pe_hd_generation {
    /* Private state; caller-owned fixed storage. Never copy a live tracker. */
    uint64_t epoch;
    uint64_t tiles[PE_HD_GENERATION_TILES];
} pe_hd_generation;
typedef enum pe_hd_generation_mode {
    PE_HD_GENERATION_CLIP, PE_HD_GENERATION_WRAP
} pe_hd_generation_mode;
typedef struct pe_hd_generation_rect {
    int32_t x, y; /* VRAM WORD coordinates, not indexed texture texels. */
    uint32_t width, height; /* 1..1024 and 1..512 respectively */
    pe_hd_generation_mode mode;
} pe_hd_generation_rect;
typedef enum pe_hd_generation_result {
    PE_HD_GENERATION_OK, PE_HD_GENERATION_ARGUMENT,
    PE_HD_GENERATION_ROLLOVER
} pe_hd_generation_result;

/* No allocation, locks, VRAM reads, hash or renderer hook. Single owner must
 * serialize ALL calls with VRAM writes/snapshot acquisition. Initialize before
 * use; rectangle and output storage must not overlap the tracker. 16x16-word tiles
 * conservatively invalidate neighbors in the same tile (including CLUT data).
 * Init starts epoch 1. Reset/reinit REQUIRES caller to discard every outstanding
 * checkpoint, snapshot, request and derived key first: epochs are tracker-local,
 * not scene IDs or pipeline generations. Never compare across lifetimes.
 * At UINT64_MAX a nonempty touch returns ROLLOVER unchanged; no silent wrap.
 * Caller must invalidate dependent state before init, then retry that write.
 * Valid CLIP rectangles intersect [0,1024)x[0,512); empty intersections are
 * no-ops. WRAP normalizes signed origins modulo each dimension and splits at
 * edges. Bounds are half-open; no writes wrap implicitly. Invalid calls leave
 * tracker/output unchanged. A touch advances once even if pixels are unchanged.
 * checkpoint is tracker.epoch sampled under the same owner synchronization.
 * changed checks any intersecting tile newer than since (1..current epoch).
 * Full dependency coverage requires querying BOTH packed words and CLUT words;
 * E2/window/page changes and scene events need separate owner invalidation. */
void pe_hd_generation_init(pe_hd_generation *tracker);
pe_hd_generation_result pe_hd_generation_touch(pe_hd_generation *tracker,
    const pe_hd_generation_rect *rect);
pe_hd_generation_result pe_hd_generation_changed(const pe_hd_generation *tracker,
    const pe_hd_generation_rect *rect, uint64_t since, bool *changed);
#endif


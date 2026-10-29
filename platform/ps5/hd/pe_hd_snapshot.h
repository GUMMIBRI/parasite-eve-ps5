#ifndef PE_HD_SNAPSHOT_H
#define PE_HD_SNAPSHOT_H
#include "pe_hd_identity.h"
#include "pe_hd_generation.h"
#include "pe_hd_vram.h"

typedef struct pe_hd_snapshot {
    PeHdFilename key;
    uint64_t checkpoint;
    pe_hd_generation_rect dependencies[2]; /* packed page, full CLUT */
} pe_hd_snapshot;

/* Caller serializes snapshot acquisition, this call, tracker touches and later
 * validation with VRAM writers. vram must describe tracker->epoch EXACTLY;
 * neither an old copy plus a new epoch nor unsynchronized live VRAM is valid.
 * Tracker is initialized and never reset while derived proofs remain live.
 * Full 256x256 ordinary P4/P8 only, no window, UV crop or wrapping footprint.
 * tpage accepts bits0..8 (blend bits5..6 do not affect content); bit9+ rejected.
 * STP here refers to filename mode, not palette bit15, which IS hashed intact.
 * Does not certify draw/filter/mask/blend compatibility or provide a state ID.
 * No allocation, extraction buffer, retained pointer or renderer hook. Caller
 * owns disjoint inputs/output. budget counts hashed bytes, max66048. Failure
 * leaves out unchanged. Output is owned by value; tracker lifetime still governs
 * use of checkpoint. Check BOTH dependencies before use; no epoch ABA allowed.
 */
pe_hd_identity_result pe_hd_snapshot_page(const uint16_t *vram, size_t words,
    size_t stride, const pe_hd_vram_request *request,
    const pe_hd_generation *tracker, size_t budget, pe_hd_snapshot *out);
#endif


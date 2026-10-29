#ifndef PE_HD_IDENTITY_H
#define PE_HD_IDENTITY_H
#include "pe_hd_filename.h"

typedef enum pe_hd_identity_result {
    PE_HD_IDENTITY_OK, PE_HD_IDENTITY_ARGUMENT,
    PE_HD_IDENTITY_UNSUPPORTED, PE_HD_IDENTITY_BUDGET
} pe_hd_identity_result;

/* Compute a texpage key from caller-selected filename geometry and an immutable
 * synchronized native-word VRAM snapshot (1024x512, stride >=1024 words).
 * page_x/page_y are native word coordinates; CLUT origin is in native words.
 * All storage is caller-owned, live and disjoint for the duration of this call.
 * No allocations, I/O, source pointers retained, renderer hook or drawing.
 * Supports nonwrapping P4/P8/STP4/STP8 pages, word-aligned subrectangles,
 * and FULL palettes only. Unsupported reduced/edge-truncated CLUTs fail closed.
 * Geometry's old hashes are ignored; successful output copies its other fields.
 * budget bounds input bytes hashed (not snapshot size); fixed scratch <=512
 * bytes plus xxHash state. On any error *out is unchanged.
 * A result is content identity only, not draw compatibility or generation proof.
 */
pe_hd_identity_result pe_hd_identity_page(const uint16_t *vram, size_t words,
    size_t stride, uint16_t page_x, uint16_t page_y,
    uint16_t clut_x, uint16_t clut_y, const PeHdFilename *geometry,
    size_t budget, PeHdFilename *out);
#endif


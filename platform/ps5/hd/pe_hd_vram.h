#ifndef PE_HD_VRAM_H
#define PE_HD_VRAM_H
#include <stddef.h>
#include <stdint.h>

enum { PE_HD_VRAM_ZERO = 1, PE_HD_VRAM_STP = 2 };
typedef struct pe_hd_vram_texel {
    uint16_t color; /* Unmodified PS1 R5 G5 B5 STP word, not RGBA alpha. */
    uint8_t index;  /* Palette index; zero for direct color. */
    uint8_t flags;  /* ZERO iff color == 0; STP iff bit 15 set. */
} pe_hd_vram_texel;
typedef struct pe_hd_vram_request {
    uint16_t tpage, clut;
    uint16_t u, v, width, height; /* origins 0..255, extents 1..256 */
    uint32_t window; /* GP0(E2) low 20 bits; zero disables texture window. */
} pe_hd_vram_request;
typedef struct pe_hd_vram_metadata {
    uint16_t palette[256]; /* Snapshot, unused entries zero. */
    uint16_t palette_count; /* 16, 256, or 0 for direct color */
    uint8_t depth; /* tpage bits 7..8: 0=4bpp, 1=8bpp, 2=direct */
    size_t texel_count, zero_count, stp_count;
} pe_hd_vram_metadata;
typedef enum pe_hd_vram_result {
    PE_HD_VRAM_OK, PE_HD_VRAM_ARGUMENT, PE_HD_VRAM_LIMIT,
    PE_HD_VRAM_RESERVED_DEPTH
} pe_hd_vram_result;

/* Pure synchronous extraction; no allocations, I/O, hashes or renderer hooks.
 * Input is immutable host-native uint16_t VRAM words, not a byte stream.
 * Exactly 1024 columns x 512 rows; stride is in words (>=1024), words bounds
 * the readable span including row padding. Caller synchronizes VRAM writers.
 * Output is caller-owned, row-major, tightly packed; capacity counts texels.
 * budget bounds bytes written to texels PLUS sizeof(*metadata); no heap cost.
 * All arguments/storage must be disjoint and remain live throughout the call.
 * Failure leaves outputs unchanged. Success overwrites metadata and width*height
 * texels. Output never borrows VRAM, so caller may mutate VRAM after return.
 * UV/window/VRAM-X wrap follows PsyZ SampleTexture; CLUT wraps within its row.
 * Reserved depth 3 is rejected even though PsyZ currently treats it as direct.
 * No modulation/filtering/blend or mask-write policy is applied. STP alone does
 * not imply blending: the primitive's semi-transparency flag also matters. */
pe_hd_vram_result pe_hd_vram_extract(const uint16_t *vram, size_t words,
    size_t stride, const pe_hd_vram_request *request,
    pe_hd_vram_texel *texels, size_t capacity, size_t budget,
    pe_hd_vram_metadata *metadata);
#endif


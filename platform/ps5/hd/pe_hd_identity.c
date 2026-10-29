/* Independent implementation of the public byte-level format contract.
 * See IDENTITY.md for pinned documentation, exclusions and provenance.
 * xxHash is the unmodified BSD-2-Clause upstream primitive, not emulator code.
 */
#include "pe_hd_identity.h"
#define XXH_INLINE_ALL
#include "vendor/xxhash-v0.8.3.h"

/* Explicit serialization avoids host endianness and VRAM stride dependence. */
static uint64_t hash_rect(const uint16_t *vram, size_t stride,
    unsigned x, unsigned y, unsigned width, unsigned height)
{
    XXH3_state_t state;
    unsigned char row[512]; /* max P8 row:128words, full CLUT:256words */
    XXH3_INITSTATE(&state);
    (void)XXH3_64bits_reset(&state);
    for (unsigned j = 0; j < height; ++j) {
        const uint16_t *src = vram + (y + j) * stride + x;
        for (unsigned i = 0; i < width; ++i) {
            row[i * 2] = (unsigned char)src[i];
            row[i * 2 + 1] = (unsigned char)(src[i] >> 8);
        }
        (void)XXH3_64bits_update(&state, row, width * 2u);
    }
    return XXH3_64bits_digest(&state);
}

pe_hd_identity_result pe_hd_identity_page(const uint16_t *vram, size_t words,
    size_t stride, uint16_t page_x, uint16_t page_y,
    uint16_t clut_x, uint16_t clut_y, const PeHdFilename *geometry,
    size_t budget, PeHdFilename *out)
{
    PeHdFilename key;
    unsigned expansion, palette_count, width_words;
    size_t bytes;
    if (!vram || !geometry || !out || stride < 1024 ||
        stride > (SIZE_MAX / sizeof(*vram) - 1024u) / 511u ||
        words < stride * 511u + 1024u)
        return PE_HD_IDENTITY_ARGUMENT;
    switch (geometry->mode) {
    case PE_HD_NAME_P4: case PE_HD_NAME_STP4:
        expansion = 4; palette_count = 16; break;
    case PE_HD_NAME_P8: case PE_HD_NAME_STP8:
        expansion = 2; palette_count = 256; break;
    default: return PE_HD_IDENTITY_UNSUPPORTED;
    }
    if (geometry->source_width != 256u / expansion ||
        geometry->source_height != 256 || !geometry->width ||
        !geometry->height || geometry->x % expansion ||
        geometry->width % expansion ||
        (unsigned)geometry->x + geometry->width > 256u ||
        (unsigned)geometry->y + geometry->height > 256u ||
        geometry->palette_first != 0 ||
        geometry->palette_last != palette_count - 1u ||
        page_x % 64 || page_y % 256 ||
        (unsigned)page_x + geometry->source_width > 1024u ||
        (unsigned)page_y + 256u > 512u || clut_x % 16 ||
        (unsigned)clut_x + palette_count > 1024u || clut_y >= 512)
        return PE_HD_IDENTITY_UNSUPPORTED;
    width_words = geometry->width / expansion;
    bytes = ((size_t)width_words * geometry->height + palette_count) * 2u;
    if (bytes > budget) return PE_HD_IDENTITY_BUDGET;
    key = *geometry;
    key.texture_hash = hash_rect(vram, stride, page_x + geometry->x / expansion,
        page_y + geometry->y, width_words, geometry->height);
    key.palette_hash = hash_rect(vram, stride, clut_x, clut_y, palette_count, 1);
    *out = key;
    return PE_HD_IDENTITY_OK;
}


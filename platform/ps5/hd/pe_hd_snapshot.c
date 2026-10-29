/* Independent adapter over existing public-contract identity; see SNAPSHOT.md. */
#include "pe_hd_snapshot.h"

pe_hd_identity_result pe_hd_snapshot_page(const uint16_t *vram, size_t words,
    size_t stride, const pe_hd_vram_request *r,
    const pe_hd_generation *tracker, size_t budget, pe_hd_snapshot *out)
{
    pe_hd_snapshot result = {0};
    PeHdFilename geometry = {0};
    pe_hd_identity_result status;
    unsigned depth, px, py, cx, cy, width, colors;
    if (!r || !tracker || !tracker->epoch || !out)
        return PE_HD_IDENTITY_ARGUMENT;
    depth = (r->tpage >> 7) & 3u;
    if (depth > 1 || (r->tpage & ~0x1ffu) || (r->clut & 0x8000u) ||
        r->window || r->u || r->v || r->width != 256 || r->height != 256)
        return PE_HD_IDENTITY_UNSUPPORTED;
    width = depth ? 128 : 64;
    colors = depth ? 256 : 16;
    px = (r->tpage & 15u) * 64u;
    py = ((r->tpage >> 4) & 1u) * 256u;
    cx = (r->clut & 63u) * 16u;
    cy = (r->clut >> 6) & 511u;
    geometry.mode = depth ? PE_HD_NAME_P8 : PE_HD_NAME_P4;
    geometry.source_width = (uint16_t)width;
    geometry.source_height = geometry.width = geometry.height = 256;
    geometry.palette_last = (uint16_t)(colors - 1);
    status = pe_hd_identity_page(vram, words, stride, (uint16_t)px,
        (uint16_t)py, (uint16_t)cx, (uint16_t)cy, &geometry, budget, &result.key);
    if (status != PE_HD_IDENTITY_OK) return status;
    result.checkpoint = tracker->epoch;
    result.dependencies[0] = (pe_hd_generation_rect){(int32_t)px, (int32_t)py,
        width, 256, PE_HD_GENERATION_CLIP};
    result.dependencies[1] = (pe_hd_generation_rect){(int32_t)cx, (int32_t)cy,
        colors, 1, PE_HD_GENERATION_CLIP};
    *out = result;
    return PE_HD_IDENTITY_OK;
}


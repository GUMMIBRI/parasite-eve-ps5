/* Independent implementation from documented PS1 formats; no emulator copy.
 * https://psx-spx.consoledev.net/graphicsprocessingunitgpu/
 * PsyZ reference: psyz/src/platform/sdl3_software.c SampleTexture and textured
 * RasterTriangle branch (zero discard; semi && STP). See header for limits. */
#include "pe_hd_vram.h"

pe_hd_vram_result pe_hd_vram_extract(const uint16_t *vram, size_t words,
    size_t stride, const pe_hd_vram_request *r, pe_hd_vram_texel *out,
    size_t capacity, size_t budget, pe_hd_vram_metadata *metadata) {
    unsigned depth, cx, cy, tx, ty, mx, my, ox, oy;
    size_t count, required, i;
    pe_hd_vram_metadata m = {0};
    if (!vram || !r || !out || !metadata || stride < 1024 ||
        r->u > 255 || r->v > 255 || !r->width || !r->height ||
        r->width > 256 || r->height > 256 || (r->window & ~0xfffffu))
        return PE_HD_VRAM_ARGUMENT;
    if (stride > (SIZE_MAX - 1024) / 511 || words < stride * 511 + 1024)
        return PE_HD_VRAM_LIMIT;
    depth = (r->tpage >> 7) & 3;
    if (depth == 3) return PE_HD_VRAM_RESERVED_DEPTH;
    count = (size_t)r->width * r->height;
    required = count * sizeof(*out) + sizeof(m); /* <= 65536 texels */
    if (capacity < count || budget < required) return PE_HD_VRAM_LIMIT;
    m.depth = (uint8_t)depth;
    m.texel_count = count;
    m.palette_count = depth == 0 ? 16 : depth == 1 ? 256 : 0;
    cx = (r->clut & 63) * 16;
    cy = (r->clut >> 6) & 511;
    for (i = 0; i < m.palette_count; ++i)
        m.palette[i] = vram[(size_t)cy * stride + ((cx + i) & 1023)];
    tx = (r->tpage & 15) * 64;
    ty = ((r->tpage >> 4) & 1) * 256;
    mx = (r->window & 31) * 8;
    my = ((r->window >> 5) & 31) * 8;
    ox = ((r->window >> 10) & 31) * 8;
    oy = ((r->window >> 15) & 31) * 8;
    for (i = 0; i < count; ++i) {
        unsigned u = ((r->u + i % r->width) & 255);
        unsigned v = ((r->v + i / r->width) & 255);
        unsigned shift = depth == 0 ? 2 : depth == 1 ? 1 : 0;
        uint16_t packed, color;
        uint8_t index = 0, flags;
        u = (u & ~mx) | (ox & mx);
        v = (v & ~my) | (oy & my);
        packed = vram[(size_t)(ty + v) * stride + ((tx + (u >> shift)) & 1023)];
        if (depth == 0) index = (uint8_t)((packed >> ((u & 3) * 4)) & 15);
        if (depth == 1) index = (uint8_t)((packed >> ((u & 1) * 8)) & 255);
        color = depth == 2 ? packed : m.palette[index];
        flags = (uint8_t)((color == 0 ? PE_HD_VRAM_ZERO : 0) |
            (color & 0x8000 ? PE_HD_VRAM_STP : 0));
        out[i].color = color;
        out[i].index = index;
        out[i].flags = flags;
        m.zero_count += color == 0;
        m.stp_count += (color >> 15);
    }
    *metadata = m;
    return PE_HD_VRAM_OK;
}


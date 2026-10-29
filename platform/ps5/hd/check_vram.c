#include "pe_hd_vram.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Synthetic words only. No game or private texture inputs. */
static uint16_t vram[1031 * 512];
static pe_hd_vram_texel pixels[256 * 256];
static pe_hd_vram_metadata metadata;
static pe_hd_vram_request r;
static size_t stride = 1024;
static void extract(void) {
    assert(pe_hd_vram_extract(vram, sizeof(vram)/sizeof(*vram), stride,
        &r, pixels, 256 * 256, sizeof(pixels) + sizeof(metadata),
        &metadata) == PE_HD_VRAM_OK);
}
static void reset(void) {
    memset(vram, 0, sizeof(vram));
    memset(&r, 0, sizeof(r));
    r.width = 1; r.height = 1; stride = 1024;
}
static void failure(pe_hd_vram_result expected, size_t words, size_t s,
                    size_t capacity, size_t budget) {
    pe_hd_vram_texel before;
    pe_hd_vram_metadata m;
    memset(&metadata, 0xa5, sizeof(metadata));
    memset(pixels, 0x5a, sizeof(pixels));
    memcpy(&before, pixels, sizeof(before));
    memcpy(&m, &metadata, sizeof(m));
    assert(pe_hd_vram_extract(vram, words, s, &r, pixels, capacity,
        budget, &metadata) == expected);
    assert(!memcmp(&before, pixels, sizeof(before)));
    assert(!memcmp(&m, &metadata, sizeof(m)));
}
int main(void) {
    unsigned i;
    reset(); r.clut = 64; r.width = 4;
    vram[0] = 0x3210;
    vram[1024] = 0; vram[1025] = 0x8000;
    vram[1026] = 0x001f; vram[1027] = 0xffff;
    extract();
    assert(metadata.palette_count == 16 && metadata.depth == 0);
    assert(metadata.zero_count == 1 && metadata.stp_count == 2);
    assert(pixels[0].flags == PE_HD_VRAM_ZERO);
    assert(pixels[1].flags == PE_HD_VRAM_STP && pixels[1].color == 0x8000);
    assert(pixels[2].flags == 0 && pixels[2].color == 31);
    for (i = 0; i < 4; ++i) assert(pixels[i].index == i);
    assert(metadata.palette[16] == 0);

    /* Unaligned nibble start crosses packed words; last CLUT row. */
    reset(); r.clut = 511u << 6; r.u = 3; r.width = 2;
    vram[0] = 0x4000; vram[1] = 5;
    vram[511 * 1024 + 4] = 0x4321;
    vram[511 * 1024 + 5] = 0xabcd;
    extract();
    assert(pixels[0].index == 4 && pixels[0].color == 0x4321);
    assert(pixels[1].index == 5 && pixels[1].color == 0xabcd);

    /* 8bpp byte order, palette index 0 nonzero, CLUT crossing X=1023. */
    reset(); r.tpage = 128; r.clut = 64 + 63; r.width = 2;
    vram[0] = 0xff00;
    vram[1024 + 1008] = 0x1234;
    vram[1024 + 239] = 0x8000;
    extract();
    assert(metadata.palette_count == 256 && metadata.depth == 1);
    assert(pixels[0].index == 0 && pixels[0].color == 0x1234);
    assert(pixels[0].flags == 0);
    assert(pixels[1].index == 255 && pixels[1].color == 0x8000);

    /* Direct page at bottom right; VRAM X wraps and UV Y wraps to page base. */
    reset(); stride = 1031; r.tpage = 256 + 16 + 15;
    r.u = 63; r.v = 255; r.width = 2; r.height = 2;
    vram[511 * 1031 + 1023] = 0x001f;
    vram[511 * 1031] = 0x03e0;
    vram[256 * 1031 + 1023] = 0x7c00;
    vram[256 * 1031] = 0xffff;
    extract();
    assert(metadata.palette_count == 0 && metadata.depth == 2);
    assert(pixels[0].color == 0x001f && pixels[1].color == 0x03e0);
    assert(pixels[2].color == 0x7c00 && pixels[3].color == 0xffff);
    assert(metadata.palette[0] == 0 && pixels[3].index == 0);

    reset(); r.tpage = 256; r.u = 255; r.v = 255;
    r.width = 2; r.height = 2;
    vram[255 * 1024 + 255] = 1; vram[255 * 1024] = 2;
    vram[255] = 3; vram[0] = 4;
    extract();
    for (i = 0; i < 4; ++i) assert(pixels[i].color == i + 1);

    /* Window masks X bit3/Y bit4 and replaces them using offset bits. */
    reset(); r.tpage = 256; r.u = 0; r.v = 16;
    r.window = 1 | (2u << 5) | (1u << 10);
    vram[8] = 0x4567;
    extract(); assert(pixels[0].color == 0x4567);

    /* Full bounded output and exact budget boundary. */
    reset(); r.tpage = 256; r.width = r.height = 256;
    extract(); assert(metadata.texel_count == 65536 && metadata.zero_count == 65536);
    failure(PE_HD_VRAM_LIMIT, 524288, 1024, 65536,
        sizeof(pixels) + sizeof(metadata) - 1);
    failure(PE_HD_VRAM_LIMIT, 524288, 1024, 65535, SIZE_MAX);
    failure(PE_HD_VRAM_LIMIT, 524287, 1024, 65536, SIZE_MAX);
    failure(PE_HD_VRAM_LIMIT, SIZE_MAX, SIZE_MAX, 65536, SIZE_MAX);
    failure(PE_HD_VRAM_ARGUMENT, 524288, 1023, 65536, SIZE_MAX);
    r.tpage = 384;
    failure(PE_HD_VRAM_RESERVED_DEPTH, 524288, 1024, 65536, SIZE_MAX);
    r.tpage = 0; r.window = 1u << 20;
    failure(PE_HD_VRAM_ARGUMENT, 524288, 1024, 65536, SIZE_MAX);
    r.window = 0; r.width = 257;
    failure(PE_HD_VRAM_ARGUMENT, 524288, 1024, 65536, SIZE_MAX);
    r.width = 0;
    failure(PE_HD_VRAM_ARGUMENT, 524288, 1024, 65536, SIZE_MAX);
    r.width = 1; r.u = 256;
    failure(PE_HD_VRAM_ARGUMENT, 524288, 1024, 65536, SIZE_MAX);
    assert(pe_hd_vram_extract(NULL, 524288, 1024, &r, pixels, 65536,
        SIZE_MAX, &metadata) == PE_HD_VRAM_ARGUMENT);
    puts("PASS: 4bpp/8bpp/direct, palette/STP/zero, CLUT/X/UV wrap, window, stride, budgets and failure atomicity");
    return 0;
}


#include "pe_hd_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t vram[512 * 1031];
static uint16_t pattern(unsigned i) { return (uint16_t)((i * 7919u) ^ 0xa51cu); }
static PeHdFilename base(void)
{
    PeHdFilename k = {0};
    k.mode = PE_HD_NAME_P4; k.source_width = 64; k.source_height = 256;
    k.width = 64; k.height = 1; k.palette_last = 15;
    return k;
}
static void fill(unsigned width_words, unsigned height)
{
    memset(vram, 0xdd, sizeof(vram));
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width_words; ++x)
            vram[(256u + y) * 1031u + 64u + x] = pattern(y * width_words + x);
    for (unsigned i = 0; i < 256; ++i) vram[400u * 1031u + 128u + i] = pattern(i);
}
static pe_hd_identity_result compute(const PeHdFilename *k, size_t budget, PeHdFilename *out)
{
    return pe_hd_identity_page(vram, sizeof(vram)/sizeof(vram[0]), 1031,
        64, 256, 128, 400, k, budget, out);
}
int main(void)
{
    /* Literal vectors from pinned public documentation, independently generated
     * there with Python xxhash3.6.0. We do not derive expected hashes with SUT. */
    const unsigned counts[] = {4, 8, 16, 256, 8192};
    const uint64_t hashes[] = {UINT64_C(0x3631D2A04EC85311),
        UINT64_C(0x000CA76CB9B329DC), UINT64_C(0x9CBCDEA9305D7528),
        UINT64_C(0x0005D0B83A005F4E), UINT64_C(0xC68A00DCCA652920)};
    PeHdFilename k = base(), out, other, sentinel;
    for (unsigned i = 0; i < 5; ++i) {
        unsigned row = counts[i] > 64 ? 64 : counts[i];
        unsigned height = counts[i] / row;
        fill(row, height); k.width = (uint16_t)(row * 4); k.height = (uint16_t)height;
        assert(compute(&k, SIZE_MAX, &out) == PE_HD_IDENTITY_OK);
        assert(out.texture_hash == hashes[i]);
        assert(out.palette_hash == UINT64_C(0x9CBCDEA9305D7528));
    }
    /* Subrectangle in expanded texel units; outside words/stride padding ignored. */
    k = base(); k.x = 8; k.y = 3; k.width = 16;
    fill(1, 1);
    for (unsigned i = 0; i < 4; ++i) vram[259u * 1031u + 66u + i] = pattern(i);
    assert(compute(&k, 40, &out) == PE_HD_IDENTITY_OK);
    assert(out.texture_hash == hashes[0]);
    vram[259u * 1031u + 65u] ^= 1; vram[259u * 1031u + 1028u] ^= 1;
    assert(compute(&k, 40, &other) == PE_HD_IDENTITY_OK);
    assert(other.texture_hash == out.texture_hash);
    vram[259u * 1031u + 66u] ^= 1;
    assert(compute(&k, 40, &other) == PE_HD_IDENTITY_OK);
    assert(other.texture_hash != out.texture_hash);
    vram[400u * 1031u + 128u] ^= 0x8000;
    assert(compute(&k, 40, &other) == PE_HD_IDENTITY_OK);
    assert(other.palette_hash != out.palette_hash); /* STP preserved in CLUT */
    out = other; k.mode = PE_HD_NAME_STP4;
    assert(compute(&k, 40, &other) == PE_HD_IDENTITY_OK);
    assert(other.texture_hash == out.texture_hash && other.palette_hash == out.palette_hash);
    fill(8, 1); k = base(); k.mode = PE_HD_NAME_P8;
    k.source_width = 128; k.palette_last = 255; k.width = 16;
    assert(compute(&k, 528, &out) == PE_HD_IDENTITY_OK);
    assert(out.texture_hash == hashes[1] && out.palette_hash == hashes[3]);
    memset(&sentinel, 0xa5, sizeof(sentinel)); out = sentinel;
    assert(compute(&k, 527, &out) == PE_HD_IDENTITY_BUDGET);
    assert(memcmp(&out, &sentinel, sizeof(out)) == 0);
    k.palette_first = 1;
    assert(compute(&k, SIZE_MAX, &out) == PE_HD_IDENTITY_UNSUPPORTED);
    k = base(); k.x = 1;
    assert(compute(&k, SIZE_MAX, &out) == PE_HD_IDENTITY_UNSUPPORTED);
    k = base(); k.width = 65535;
    assert(compute(&k, SIZE_MAX, &out) == PE_HD_IDENTITY_UNSUPPORTED);
    k = base();
    assert(pe_hd_identity_page(vram, SIZE_MAX, SIZE_MAX, 64, 256, 128, 400,
        &k, SIZE_MAX, &out) == PE_HD_IDENTITY_ARGUMENT);
    assert(pe_hd_identity_page(vram, SIZE_MAX,
        (SIZE_MAX / sizeof(*vram) - 1024u) / 511u + 1u,
        64, 256, 128, 400, &k, SIZE_MAX, &out) == PE_HD_IDENTITY_ARGUMENT);
    assert(pe_hd_identity_page(vram, 511*1031+1023, 1031, 64, 256, 128, 400,
        &k, SIZE_MAX, &out) == PE_HD_IDENTITY_ARGUMENT);
    assert(pe_hd_identity_page(vram, 512*1031, 1031, 64, 256, 1024, 400,
        &k, SIZE_MAX, &out) == PE_HD_IDENTITY_UNSUPPORTED);
    k.mode = PE_HD_NAME_P8; k.source_width = 128; k.palette_last = 255;
    assert(pe_hd_identity_page(vram, 512*1031, 1031, 960, 256, 128, 400,
        &k, SIZE_MAX, &out) == PE_HD_IDENTITY_UNSUPPORTED);
    assert(pe_hd_identity_page(vram, 512*1031, 1031, 64, 256, 1008, 400,
        &k, SIZE_MAX, &out) == PE_HD_IDENTITY_UNSUPPORTED);
    assert(memcmp(&out, &sentinel, sizeof(out)) == 0);
    puts("identity: fixed XXH3 vectors, stride/crop, STP, mode, budget and rejection PASS");
    return 0;
}


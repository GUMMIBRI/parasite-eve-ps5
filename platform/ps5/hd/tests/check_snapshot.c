#include "pe_hd_snapshot.h"
#include <assert.h>
#include <string.h>

static uint16_t vram[512 * 1030]; /* padded rows */
static pe_hd_vram_texel texels[256 * 256];
static pe_hd_generation tracker;
static pe_hd_snapshot snapshot, before;

static void rejected(pe_hd_vram_request r, pe_hd_identity_result expected,
                     size_t budget)
{
    memset(&snapshot, 0x5a, sizeof(snapshot));
    memcpy(&before, &snapshot, sizeof(snapshot));
    assert(pe_hd_snapshot_page(vram, 512 * 1030, 1030, &r, &tracker,
        budget, &snapshot) == expected);
    assert(memcmp(&before, &snapshot, sizeof(snapshot)) == 0);
}

static void supported(unsigned depth)
{
    pe_hd_vram_request r = {(uint16_t)((depth << 7) | 0x11),
        (uint16_t)((100 << 6) | 32), 0, 0, 256, 256, 0};
    pe_hd_vram_metadata metadata;
    pe_hd_snapshot changed;
    pe_hd_generation_rect mutation = {64, 256, 1, 1, PE_HD_GENERATION_CLIP};
    bool dirty = true;
    unsigned colors = depth ? 256 : 16;
    memset(vram, 0, sizeof(vram));
    pe_hd_generation_init(&tracker);
    vram[256 * 1030 + 64] = depth ? 0x0201 : 0x3210;
    for (unsigned i = 0; i < colors; ++i) vram[100 * 1030 + 512 + i] = (uint16_t)i;
    vram[100 * 1030 + 514] = 0x8002;
    assert(pe_hd_snapshot_page(vram, 512 * 1030, 1030, &r, &tracker,
        66048, &snapshot) == PE_HD_IDENTITY_OK);
    assert(snapshot.key.mode == (depth ? PE_HD_NAME_P8 : PE_HD_NAME_P4));
    assert(snapshot.key.source_width == (depth ? 128 : 64));
    assert(snapshot.dependencies[0].x == 64 && snapshot.dependencies[0].y == 256);
    assert(snapshot.dependencies[0].width == (depth ? 128u : 64u));
    assert(snapshot.dependencies[1].x == 512 && snapshot.dependencies[1].y == 100);
    assert(snapshot.dependencies[1].width == colors && snapshot.dependencies[1].height == 1);
    assert(pe_hd_vram_extract(vram, 512 * 1030, 1030, &r, texels, 65536,
        sizeof(texels) + sizeof(metadata), &metadata) == PE_HD_VRAM_OK);
    assert(texels[0].index == (depth ? 1 : 0));
    assert(texels[1].index == (depth ? 2 : 1));
    assert(texels[depth ? 1 : 2].flags & PE_HD_VRAM_STP);
    /* Unrelated row/padding is neither content nor a dependency. */
    mutation.x = 900; mutation.y = 10;
    vram[10 * 1030 + 900] ^= 1;
    assert(pe_hd_generation_touch(&tracker, &mutation) == PE_HD_GENERATION_OK);
    for (unsigned i = 0; i < 2; ++i) {
        assert(pe_hd_generation_changed(&tracker, &snapshot.dependencies[i],
            snapshot.checkpoint, &dirty) == PE_HD_GENERATION_OK && !dirty);
    }
    assert(pe_hd_snapshot_page(vram, 512 * 1030, 1030, &r, &tracker,
        66048, &changed) == PE_HD_IDENTITY_OK);
    assert(changed.key.texture_hash == snapshot.key.texture_hash);
    assert(changed.key.palette_hash == snapshot.key.palette_hash);
    /* Palette bit15 participates in identity AND generation proof. */
    mutation.x = 514; mutation.y = 100;
    vram[100 * 1030 + 514] ^= 0x8000;
    assert(pe_hd_generation_touch(&tracker, &mutation) == PE_HD_GENERATION_OK);
    assert(pe_hd_generation_changed(&tracker, &snapshot.dependencies[1],
        snapshot.checkpoint, &dirty) == PE_HD_GENERATION_OK && dirty);
    assert(pe_hd_snapshot_page(vram, 512 * 1030, 1030, &r, &tracker,
        66048, &changed) == PE_HD_IDENTITY_OK);
    assert(changed.key.texture_hash == snapshot.key.texture_hash);
    assert(changed.key.palette_hash != snapshot.key.palette_hash);
    mutation.x = 64; mutation.y = 256;
    vram[256 * 1030 + 64] ^= 1;
    assert(pe_hd_generation_touch(&tracker, &mutation) == PE_HD_GENERATION_OK);
    assert(pe_hd_generation_changed(&tracker, &snapshot.dependencies[0],
        snapshot.checkpoint, &dirty) == PE_HD_GENERATION_OK && dirty);
    assert(pe_hd_snapshot_page(vram, 512 * 1030, 1030, &r, &tracker,
        66048, &changed) == PE_HD_IDENTITY_OK);
    assert(changed.key.texture_hash != snapshot.key.texture_hash);
    rejected(r, PE_HD_IDENTITY_BUDGET, (depth ? 66048 : 32800) - 1);
}

int main(void)
{
    pe_hd_vram_request r = {0, 0, 0, 0, 256, 256, 0}, bad;
    supported(0); supported(1);
    bad = r; bad.window = 1; rejected(bad, PE_HD_IDENTITY_UNSUPPORTED, 66048);
    bad = r; bad.u = 1; rejected(bad, PE_HD_IDENTITY_UNSUPPORTED, 66048);
    bad = r; bad.width = 255; rejected(bad, PE_HD_IDENTITY_UNSUPPORTED, 66048);
    bad = r; bad.tpage = 256; rejected(bad, PE_HD_IDENTITY_UNSUPPORTED, 66048);
    bad = r; bad.tpage = 512; rejected(bad, PE_HD_IDENTITY_UNSUPPORTED, 66048);
    bad = r; bad.clut = 0x8000; rejected(bad, PE_HD_IDENTITY_UNSUPPORTED, 66048);
    bad = r; bad.tpage = 128 | 15; rejected(bad, PE_HD_IDENTITY_UNSUPPORTED, 66048);
    bad = r; bad.tpage = 128; bad.clut = 63;
    rejected(bad, PE_HD_IDENTITY_UNSUPPORTED, 66048);
    memset(&snapshot, 0x5a, sizeof(snapshot)); memcpy(&before, &snapshot, sizeof(snapshot));
    assert(pe_hd_snapshot_page(vram, 1024, 1030, &r, &tracker,
        66048, &snapshot) == PE_HD_IDENTITY_ARGUMENT);
    assert(memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    tracker.epoch = 0; rejected(r, PE_HD_IDENTITY_ARGUMENT, 66048);
    pe_hd_generation_init(&tracker);
    assert(pe_hd_snapshot_page(vram, SIZE_MAX, SIZE_MAX, &r, &tracker,
        66048, &snapshot) == PE_HD_IDENTITY_ARGUMENT);
    return 0;
}


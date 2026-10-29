#include "pe_hd_generation.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static bool query(const pe_hd_generation *t, pe_hd_generation_rect r,
                  uint64_t since) {
    bool changed = false;
    assert(pe_hd_generation_changed(t, &r, since, &changed) == PE_HD_GENERATION_OK);
    return changed;
}
/* Independent word-by-word oracle for tile coverage, including signed wrap. */
static void coverage(pe_hd_generation_rect r, bool seen[2048]) {
    for (uint32_t y = 0; y < r.height; ++y)
        for (uint32_t x = 0; x < r.width; ++x) {
            int64_t px = (int64_t)r.x + x, py = (int64_t)r.y + y;
            if (r.mode == PE_HD_GENERATION_WRAP) {
                px = (px % 1024 + 1024) % 1024;
                py = (py % 512 + 512) % 512;
            }
            if (px >= 0 && px < 1024 && py >= 0 && py < 512)
                seen[(py / 16) * 64 + px / 16] = true;
        }
}
int main(void) {
    pe_hd_generation t;
    pe_hd_generation_rect one = {15, 15, 1, 1, PE_HD_GENERATION_CLIP};
    pe_hd_generation_rect adjacent = {16, 16, 1, 1, PE_HD_GENERATION_CLIP};
    bool out = true;
    pe_hd_generation_init(&t);
    assert(!query(&t, one, 1));
    assert(pe_hd_generation_touch(&t, &one) == PE_HD_GENERATION_OK);
    assert(query(&t, one, 1) && !query(&t, adjacent, 1));
    one.x = one.y = 0; /* Same tile conservatively dirty. */
    assert(query(&t, one, 1) && !query(&t, one, 2));
    one.width = 0;
    assert(pe_hd_generation_touch(&t, &one) == PE_HD_GENERATION_ARGUMENT);
    assert(t.epoch == 2);
    assert(pe_hd_generation_changed(&t, &adjacent, 3, &out) == PE_HD_GENERATION_ARGUMENT && out);
    assert(pe_hd_generation_changed(&t, &adjacent, 0, &out) == PE_HD_GENERATION_ARGUMENT);

    const pe_hd_generation_rect vectors[] = {
        {1023, 511, 2, 2, PE_HD_GENERATION_WRAP},
        {-1, -1, 2, 2, PE_HD_GENERATION_WRAP},
        {-1, -1, 2, 2, PE_HD_GENERATION_CLIP},
        {15, 15, 2, 2, PE_HD_GENERATION_CLIP},
        {1023, 511, 1024, 512, PE_HD_GENERATION_WRAP},
        {INT32_MIN, INT32_MAX, 1024, 512, PE_HD_GENERATION_WRAP},
        {INT32_MAX, INT32_MIN, 1024, 512, PE_HD_GENERATION_CLIP},
        {-1024, -512, 1024, 512, PE_HD_GENERATION_CLIP},
        {0, 0, 1024, 512, PE_HD_GENERATION_CLIP}
    };
    for (unsigned i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        bool seen[2048] = {false};
        bool any = false;
        pe_hd_generation_init(&t);
        coverage(vectors[i], seen);
        assert(pe_hd_generation_touch(&t, &vectors[i]) == PE_HD_GENERATION_OK);
        for (unsigned n = 0; n < 2048; ++n) {
            pe_hd_generation_rect tile = {(int32_t)(n % 64 * 16),
                (int32_t)(n / 64 * 16), 16, 16, PE_HD_GENERATION_CLIP};
            assert(query(&t, tile, 1) == seen[n]);
            any = any || seen[n];
        }
        assert(t.epoch == (any ? 2u : 1u));
    }
    pe_hd_generation_init(&t);
    one = (pe_hd_generation_rect){0, 0, 1, 1, PE_HD_GENERATION_CLIP};
    assert(pe_hd_generation_touch(&t, &one) == PE_HD_GENERATION_OK);
    uint64_t checkpoint = t.epoch;
    assert(pe_hd_generation_touch(&t, &adjacent) == PE_HD_GENERATION_OK);
    assert(!query(&t, one, checkpoint) && query(&t, adjacent, checkpoint));
    /* Test-only private-state fault injection simulates the last two epochs. */
    t.epoch = UINT64_MAX - 1;
    assert(pe_hd_generation_touch(&t, &one) == PE_HD_GENERATION_OK);
    assert(t.epoch == UINT64_MAX && t.tiles[0] == UINT64_MAX);
    assert(pe_hd_generation_touch(&t, &adjacent) == PE_HD_GENERATION_ROLLOVER);
    assert(t.epoch == UINT64_MAX && t.tiles[65] == 3);
    assert(query(&t, one, UINT64_MAX - 1));
    assert(!query(&t, adjacent, UINT64_MAX - 1));
    one.x = INT32_MAX;
    assert(pe_hd_generation_touch(&t, &one) == PE_HD_GENERATION_OK);
    assert(t.epoch == UINT64_MAX);
    /* Caller has now discarded all old observations before reinitialization. */
    pe_hd_generation_init(&t);
    assert(t.epoch == 1 && t.tiles[0] == 1);
    puts("generation overlap/wrap/bounds/rollover checks passed");
    return 0;
}


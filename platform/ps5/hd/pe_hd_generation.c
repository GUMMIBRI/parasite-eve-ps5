#include "pe_hd_generation.h"
#include <stddef.h>

typedef struct span { unsigned lo, hi; } span;
static unsigned axis(int32_t origin, uint32_t length, unsigned limit,
                     pe_hd_generation_mode mode, span out[2]) {
    int64_t lo = origin, hi = lo + length;
    if (mode == PE_HD_GENERATION_CLIP) {
        if (lo < 0) lo = 0;
        if (hi > limit) hi = limit;
        if (hi <= lo) return 0;
        out[0] = (span){(unsigned)lo, (unsigned)hi};
        return 1;
    }
    lo = origin % (int64_t)limit;
    if (lo < 0) lo += limit;
    hi = lo + length;
    out[0] = (span){(unsigned)lo, hi > limit ? limit : (unsigned)hi};
    if (hi <= limit) return 1;
    out[1] = (span){0, (unsigned)(hi - limit)};
    return 2;
}
static bool valid(const pe_hd_generation_rect *r) {
    return r && r->width && r->width <= 1024 && r->height &&
        r->height <= 512 && (r->mode == PE_HD_GENERATION_CLIP ||
                            r->mode == PE_HD_GENERATION_WRAP);
}
void pe_hd_generation_init(pe_hd_generation *t) {
    if (!t) return;
    t->epoch = 1;
    for (size_t i = 0; i < PE_HD_GENERATION_TILES; ++i) t->tiles[i] = 1;
}
/* Each axis has at most two nonempty spans; no coordinate addition overflows. */
pe_hd_generation_result pe_hd_generation_touch(pe_hd_generation *t,
    const pe_hd_generation_rect *r) {
    span xs[2], ys[2];
    unsigned nx, ny;
    if (!t || !t->epoch || !valid(r)) return PE_HD_GENERATION_ARGUMENT;
    nx = axis(r->x, r->width, 1024, r->mode, xs);
    ny = axis(r->y, r->height, 512, r->mode, ys);
    if (!nx || !ny) return PE_HD_GENERATION_OK;
    if (t->epoch == UINT64_MAX) return PE_HD_GENERATION_ROLLOVER;
    ++t->epoch;
    for (unsigned j = 0; j < ny; ++j)
        for (unsigned i = 0; i < nx; ++i)
            for (unsigned y = ys[j].lo / 16; y <= (ys[j].hi - 1) / 16; ++y)
                for (unsigned x = xs[i].lo / 16; x <= (xs[i].hi - 1) / 16; ++x)
                    t->tiles[y * PE_HD_GENERATION_COLUMNS + x] = t->epoch;
    return PE_HD_GENERATION_OK;
}
pe_hd_generation_result pe_hd_generation_changed(const pe_hd_generation *t,
    const pe_hd_generation_rect *r, uint64_t since, bool *changed) {
    span xs[2], ys[2];
    unsigned nx, ny;
    bool found = false;
    if (!t || !t->epoch || !valid(r) || !changed || !since || since > t->epoch)
        return PE_HD_GENERATION_ARGUMENT;
    nx = axis(r->x, r->width, 1024, r->mode, xs);
    ny = axis(r->y, r->height, 512, r->mode, ys);
    for (unsigned j = 0; j < ny; ++j)
        for (unsigned i = 0; i < nx; ++i)
            for (unsigned y = ys[j].lo / 16; y <= (ys[j].hi - 1) / 16; ++y)
                for (unsigned x = xs[i].lo / 16; x <= (xs[i].hi - 1) / 16; ++x)
                    if (t->tiles[y * PE_HD_GENERATION_COLUMNS + x] > since)
                        found = true;
    *changed = found;
    return PE_HD_GENERATION_OK;
}


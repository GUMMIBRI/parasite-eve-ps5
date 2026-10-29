/* Synthetic boundary checks; no game assets or filesystem access. */
#include "pe_hd_image.h"
#include <assert.h>
#include <string.h>
#include "synthetic_png.h" /* generated only into isolated test build */

int main(void)
{
    pe_hd_context c, tiny;
    pe_hd_image image = {0}, second = {0};
    unsigned char bad[sizeof(synthetic_png)];
    size_t bytes = 123;
    assert(pe_hd_context_init(&c, PE_HD_MAX_TOTAL_BYTES) == PE_HD_OK);
    assert(pe_hd_context_init(&tiny, 1) == PE_HD_OK);
    assert(pe_hd_rgba_size(0, 1, &bytes) == PE_HD_LIMIT && bytes == 0);
    assert(pe_hd_rgba_size(UINT32_MAX, UINT32_MAX, &bytes) == PE_HD_LIMIT);
    assert(pe_hd_rgba_size(4096, 4096, &bytes) == PE_HD_OK);
    assert(bytes == PE_HD_MAX_IMAGE_BYTES);
    assert(pe_hd_rgba_size(4096, 4097, &bytes) == PE_HD_LIMIT);
    assert(pe_hd_image_decode(&tiny, synthetic_png, sizeof(synthetic_png), &image) == PE_HD_LIMIT);
    assert(tiny.live_bytes == 0 && !image.rgba);
    assert(pe_hd_image_decode(&c, synthetic_png, sizeof(synthetic_png), &image) == PE_HD_OK);
    assert(image.width == 1 && image.height == 1 && image.bytes == 4);
    assert(image.rgba[0] == 12 && image.rgba[1] == 34 && image.rgba[2] == 56 && image.rgba[3] == 78);
    assert(c.live_bytes >= 4 && c.peak_bytes <= c.budget);
    assert(pe_hd_image_decode(&c, synthetic_png, sizeof(synthetic_png), &image) == PE_HD_ARGUMENT);
    assert(pe_hd_image_decode(&c, synthetic_png, sizeof(synthetic_png), &second) == PE_HD_OK);
    pe_hd_image_release(&second);
    pe_hd_image_release(&image);
    pe_hd_image_release(&image);
    assert(c.live_bytes == 0);
    memcpy(bad, synthetic_png, sizeof(bad));
    bad[16] = 0x7f; /* impossible width rejected before decoder allocation */
    assert(pe_hd_image_decode(&c, bad, sizeof(bad), &image) == PE_HD_LIMIT);
    assert(c.live_bytes == 0 && !image.rgba);
    /* Truncate inside IDAT: exercise decoder scratch cleanup repeatedly. */
    for (size_t n = 33; n < 45; ++n) {
        assert(pe_hd_image_decode(&c, synthetic_png, n, &image) != PE_HD_OK);
        assert(c.live_bytes == 0 && !image.rgba);
    }
    /* Fits output but not inflater scratch. */
    tiny.budget = 128;
    assert(pe_hd_image_decode(&tiny, synthetic_png, sizeof(synthetic_png), &image) != PE_HD_OK);
    assert(tiny.live_bytes == 0 && !image.rgba);
    /* Allocation-pressure boundaries include inflater growth and retained pixels. */
    for (size_t budget = 32; budget <= 32768; budget *= 2) {
        pe_hd_result result;
        assert(pe_hd_context_init(&tiny, budget) == PE_HD_OK);
        result = pe_hd_image_decode(&tiny, synthetic_png, sizeof(synthetic_png), &image);
        assert(result == PE_HD_OK || result == PE_HD_DECODE);
        assert(tiny.peak_bytes <= budget);
        pe_hd_image_release(&image);
        assert(tiny.live_bytes == 0);
    }
    return 0;
}

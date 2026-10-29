#include "pe_hd_image.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef union hd_allocation {
    max_align_t alignment;
    struct { size_t charged; pe_hd_context *owner; } meta;
} hd_allocation;
static _Thread_local pe_hd_context *active_context;

static void *hd_alloc(size_t bytes)
{
    pe_hd_context *c = active_context;
    hd_allocation *h;
    size_t charged;
    if (!c || bytes > SIZE_MAX - sizeof(*h)) return NULL;
    charged = bytes + sizeof(*h);
    if (c->live_bytes > c->budget || charged > c->budget - c->live_bytes)
        return NULL;
    h = malloc(charged);
    if (!h) return NULL;
    h->meta.charged = charged;
    h->meta.owner = c;
    c->live_bytes += charged;
    if (c->live_bytes > c->peak_bytes) c->peak_bytes = c->live_bytes;
    return h + 1;
}
static void hd_free(void *p)
{
    hd_allocation *h;
    if (!p) return;
    h = (hd_allocation *)p - 1;
    h->meta.owner->live_bytes -= h->meta.charged;
    free(h);
}
static void *hd_realloc(void *p, size_t bytes)
{
    void *next;
    size_t old;
    if (!p) return hd_alloc(bytes);
    old = ((hd_allocation *)p - 1)->meta.charged - sizeof(hd_allocation);
    /* Charge both old and new until copy completes: peak cannot exceed budget. */
    next = hd_alloc(bytes);
    if (!next) return NULL;
    memcpy(next, p, old < bytes ? old : bytes);
    hd_free(p);
    return next;
}
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_FAILURE_STRINGS
#define STBI_MAX_DIMENSIONS PE_HD_MAX_DIMENSION
#define STBI_MALLOC(n) hd_alloc(n)
#define STBI_REALLOC(p,n) hd_realloc(p,n)
#define STBI_FREE(p) hd_free(p)
#include "vendor/stb_image.h"

pe_hd_result pe_hd_context_init(pe_hd_context *c, size_t budget)
{
    if (!c || !budget || budget > PE_HD_MAX_TOTAL_BYTES) return PE_HD_ARGUMENT;
    memset(c, 0, sizeof(*c));
    c->budget = budget;
    return PE_HD_OK;
}
pe_hd_result pe_hd_rgba_size(uint32_t w, uint32_t h, size_t *bytes)
{
    size_t row;
    if (!bytes) return PE_HD_ARGUMENT;
    *bytes = 0;
    if (!w || !h || w > PE_HD_MAX_DIMENSION || h > PE_HD_MAX_DIMENSION)
        return PE_HD_LIMIT;
    if ((size_t)w > SIZE_MAX / 4) return PE_HD_LIMIT;
    row = (size_t)w * 4;
    if ((size_t)h > SIZE_MAX / row || row * h > PE_HD_MAX_IMAGE_BYTES)
        return PE_HD_LIMIT;
    *bytes = row * h;
    return PE_HD_OK;
}
static uint32_t hd_be32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}
pe_hd_result pe_hd_image_decode(pe_hd_context *c, const void *input,
                               size_t length, pe_hd_image *out)
{
    static const unsigned char signature[8] = {137,80,78,71,13,10,26,10};
    const unsigned char *p = input;
    uint32_t w, h;
    size_t bytes;
    int x, y, channels;
    unsigned char *pixels;
    pe_hd_result result;
    if (!c || !out || out->rgba || out->width || out->height || out->bytes ||
        !p || !c->budget || c->budget > PE_HD_MAX_TOTAL_BYTES || active_context)
        return PE_HD_ARGUMENT;
    if (length > PE_HD_MAX_ENCODED_BYTES || length > INT_MAX) return PE_HD_LIMIT;
    if (length < 33 || memcmp(p, signature, 8) || hd_be32(p+8) != 13 ||
        memcmp(p+12, "IHDR", 4) || p[24] != 8 || p[25] != 6 ||
        p[26] || p[27] || p[28] > 1) return PE_HD_FORMAT;
    w = hd_be32(p+16); h = hd_be32(p+20);
    result = pe_hd_rgba_size(w, h, &bytes);
    if (result != PE_HD_OK) return result;
    if (c->live_bytes > c->budget || bytes > c->budget - c->live_bytes)
        return PE_HD_LIMIT;
    active_context = c;
    pixels = stbi_load_from_memory(p, (int)length, &x, &y, &channels, 4);
    active_context = NULL;
    if (!pixels) return PE_HD_DECODE; /* corrupt data, heap or scratch budget */
    if ((uint32_t)x != w || (uint32_t)y != h || channels != 4) {
        hd_free(pixels);
        return PE_HD_FORMAT;
    }
    out->rgba = pixels; out->width = w; out->height = h; out->bytes = bytes;
    return PE_HD_OK;
}
void pe_hd_image_release(pe_hd_image *image)
{
    if (!image) return;
    hd_free((void *)image->rgba);
    memset(image, 0, sizeof(*image));
}

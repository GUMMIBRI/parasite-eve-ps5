#ifndef PE_HD_IMAGE_H
#define PE_HD_IMAGE_H
#include <stddef.h>
#include <stdint.h>

#define PE_HD_MAX_DIMENSION 8192u
#define PE_HD_MAX_IMAGE_BYTES ((size_t)64 * 1024 * 1024)
#define PE_HD_MAX_ENCODED_BYTES ((size_t)32 * 1024 * 1024)
#define PE_HD_MAX_TOTAL_BYTES ((size_t)256 * 1024 * 1024)

/* One context per decode worker. Serialize decode/release on that worker.
 * Context must outlive every image; never copy contexts or owning images.
 * Budget counts live heap allocations including scratch and allocator headers.
 * Borrowed encoded bytes and caller storage are outside this budget. */
typedef struct pe_hd_context {
    size_t budget, live_bytes, peak_bytes;
} pe_hd_context;
typedef struct pe_hd_image {
    const unsigned char *rgba; /* immutable, top-down, straight alpha, RGBA8 */
    uint32_t width, height;
    size_t bytes;
} pe_hd_image;
typedef enum pe_hd_result {
    PE_HD_OK, PE_HD_ARGUMENT, PE_HD_FORMAT, PE_HD_LIMIT, PE_HD_DECODE
} pe_hd_result;

/* Initialize fresh context only; zero budget or > hard ceiling is rejected. */
pe_hd_result pe_hd_context_init(pe_hd_context *ctx, size_t budget);
/* Pure overflow-safe policy check, no allocations. */
pe_hd_result pe_hd_rgba_size(uint32_t width, uint32_t height, size_t *bytes);
/* Worker-only synchronous decode. Input must remain immutable during call.
 * out must be zero-initialized/previously released. Failure leaves it empty.
 * No file access, game state, color transforms or filename/hash matching.
 * PNG CRCs are not validated by stb; this is not an authenticity validator. */
pe_hd_result pe_hd_image_decode(pe_hd_context *ctx, const void *png,
                               size_t length, pe_hd_image *out);
/* Release exactly once on owning worker. Zeroed image release is harmless. */
void pe_hd_image_release(pe_hd_image *image);
#endif

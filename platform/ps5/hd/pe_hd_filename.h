#ifndef PE_HD_FILENAME_H
#define PE_HD_FILENAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum PeHdNameMode {
    PE_HD_NAME_P4,
    PE_HD_NAME_P8,
    PE_HD_NAME_STP4,
    PE_HD_NAME_STP8
} PeHdNameMode;

typedef struct PeHdFilename {
    PeHdNameMode mode;
    uint64_t texture_hash, palette_hash;
    uint16_t source_width, source_height;
    uint16_t x, y, width, height;
    uint16_t palette_first, palette_last;
} PeHdFilename;

/* Parse a basename, excluding its NUL byte, in the documented texpage grammar.
 * No I/O or allocation; accepts only P4/P8/STP4/STP8 forms, max255bytes.
 * On failure out is unchanged. Parsed dimensions are raw identity fields:
 * this does not establish hash compatibility, texel/word units or renderer use.
 */
bool PeHd_ParseFilename(const char *name, size_t length, PeHdFilename *out);
#endif


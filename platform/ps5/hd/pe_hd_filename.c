/* Independently implemented from the public texture-replacement filename grammar.
 * No DuckStation implementation copied. This is a filename parser only; it does
 * not implement hashing, image decoding or replacement rendering.
 */
#include "pe_hd_filename.h"
#include <string.h>

typedef struct Cursor { const char *at, *end; } Cursor;

static bool literal(Cursor *cursor, const char *text, size_t length)
{
    if ((size_t)(cursor->end - cursor->at) < length ||
        memcmp(cursor->at, text, length) != 0) return false;
    cursor->at += length;
    return true;
}

static bool hex16(Cursor *cursor, uint64_t *out)
{
    uint64_t value = 0;
    if (cursor->end - cursor->at < 16) return false;
    for (unsigned i = 0; i < 16; ++i) {
        unsigned char c = (unsigned char)cursor->at[i];
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        value = (value << 4) | digit;
    }
    cursor->at += 16;
    *out = value;
    return true;
}

static bool decimal(Cursor *cursor, uint16_t *out)
{
    unsigned value = 0;
    const char *begin = cursor->at;
    while (cursor->at != cursor->end && *cursor->at >= '0' && *cursor->at <= '9') {
        unsigned digit = (unsigned)(*cursor->at - '0');
        if (value > (UINT16_MAX - digit) / 10u) return false;
        value = value * 10u + digit;
        ++cursor->at;
    }
    if (cursor->at == begin) return false;
    *out = (uint16_t)value;
    return true;
}

bool PeHd_ParseFilename(const char *name, size_t length, PeHdFilename *out)
{
    PeHdFilename parsed = {0};
    Cursor cursor;
    if (!name || !out || length == 0 || length > 255) return false;
    cursor.at = name;
    cursor.end = name + length;
    if (!literal(&cursor, "texpage-", 8)) return false;
    if (literal(&cursor, "STP4-", 5)) parsed.mode = PE_HD_NAME_STP4;
    else if (literal(&cursor, "STP8-", 5)) parsed.mode = PE_HD_NAME_STP8;
    else if (literal(&cursor, "P4-", 3)) parsed.mode = PE_HD_NAME_P4;
    else if (literal(&cursor, "P8-", 3)) parsed.mode = PE_HD_NAME_P8;
    else return false;
    if (!hex16(&cursor, &parsed.texture_hash) || !literal(&cursor, "-", 1) ||
        !hex16(&cursor, &parsed.palette_hash) || !literal(&cursor, "-", 1) ||
        !decimal(&cursor, &parsed.source_width) || !literal(&cursor, "x", 1) ||
        !decimal(&cursor, &parsed.source_height) || !literal(&cursor, "-", 1) ||
        !decimal(&cursor, &parsed.x) || !literal(&cursor, "-", 1) ||
        !decimal(&cursor, &parsed.y) || !literal(&cursor, "-", 1) ||
        !decimal(&cursor, &parsed.width) || !literal(&cursor, "x", 1) ||
        !decimal(&cursor, &parsed.height) || !literal(&cursor, "-P", 2) ||
        !decimal(&cursor, &parsed.palette_first) || !literal(&cursor, "-", 1) ||
        !decimal(&cursor, &parsed.palette_last) || !literal(&cursor, ".png", 4) ||
        cursor.at != cursor.end) return false;
    if (!parsed.source_width || !parsed.source_height || !parsed.width || !parsed.height ||
        parsed.palette_first > parsed.palette_last) return false;
    /* Coordinate units, CLUT ranges and overlapping identities belong to the
     * compatibility/index layer, not guesses based on a filename alone. */
    *out = parsed;
    return true;
}


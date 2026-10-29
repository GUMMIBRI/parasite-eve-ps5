/* Original bounded TSV adapter; no external parser implementation copied. */
#include "pe_hd_manifest.h"
#include <stdlib.h>
#include <string.h>

static pe_hd_index_result scan(const pe_hd_manifest_config *c,
    const unsigned char *bytes, size_t length, pe_hd_index_record *records,
    size_t *count)
{
    static const char header[] = "PE-HD-MANIFEST\t1";
    size_t start = 0, lines = 0, items = 0, paths = 0;
    while (start < length) {
        size_t end = start, n;
        while (end < length && bytes[end] != '\n') {
            if (end - start >= c->max_line) return PE_HD_INDEX_LIMIT;
            ++end;
        }
        /* Require terminated records, including header; reject partial input. */
        if (end == length) return PE_HD_INDEX_INVALID;
        n = end - start;
        if (!lines) {
            if (n != sizeof(header)-1 || memcmp(bytes+start, header, n))
                return PE_HD_INDEX_INVALID;
        } else {
            if (n <= 5 || memcmp(bytes+start, "file\t", 5))
                return PE_HD_INDEX_INVALID;
            if (items >= c->index.max_items) return PE_HD_INDEX_LIMIT;
            if (n-5 >= PE_HD_INDEX_MAX_PATH) return PE_HD_INDEX_LIMIT;
            if (n-4 > c->index.path_bytes-paths) return PE_HD_INDEX_LIMIT;
            paths += n-4; /* copied path plus NUL */
            if (records) records[items] = (pe_hd_index_record){
                (const char *)bytes+start+5, n-5};
            ++items;
        }
        ++lines;
        start = end+1;
    }
    if (!lines) return PE_HD_INDEX_INVALID;
    *count = items;
    return PE_HD_INDEX_OK;
}

pe_hd_index_result pe_hd_manifest_create(const pe_hd_manifest_config *c,
    const void *bytes, size_t length, pe_hd_index_read read, void *user,
    pe_hd_index **out)
{
    size_t count = 0;
    pe_hd_index_record *records = NULL;
    pe_hd_index_result result;
    if (!c || !bytes || !out || !read) return PE_HD_INDEX_INVALID;
    if (!c->max_bytes || c->max_bytes > PE_HD_MANIFEST_MAX_BYTES ||
        length > c->max_bytes || !c->max_line ||
        c->max_line > PE_HD_MANIFEST_MAX_LINE || !c->scratch_bytes ||
        c->scratch_bytes > PE_HD_MAX_TOTAL_BYTES || !c->index.max_items ||
        c->index.max_items > PE_HD_INDEX_MAX_ITEMS || !c->index.path_bytes)
        return PE_HD_INDEX_LIMIT;
    result = scan(c, bytes, length, NULL, &count);
    if (result != PE_HD_INDEX_OK) return result;
    /* Division before multiplication; no allocation proportional to raw input. */
    if (count > c->scratch_bytes / sizeof(*records)) return PE_HD_INDEX_LIMIT;
    if (count) {
        records = malloc(count * sizeof(*records));
        if (!records) return PE_HD_INDEX_MEMORY;
        result = scan(c, bytes, length, records, &count);
        if (result != PE_HD_INDEX_OK) { free(records); return result; }
    }
    result = pe_hd_index_create(&c->index, records, count, read, user, out);
    free(records);
    return result;
}


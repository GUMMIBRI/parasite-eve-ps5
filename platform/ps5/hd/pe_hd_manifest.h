#ifndef PE_HD_MANIFEST_H
#define PE_HD_MANIFEST_H
#include "pe_hd_index.h"

#define PE_HD_MANIFEST_MAX_BYTES (16u * 1024u * 1024u)
#define PE_HD_MANIFEST_MAX_LINE (PE_HD_INDEX_MAX_PATH + 4u)
typedef struct pe_hd_manifest_config {
    size_t max_bytes, max_line, scratch_bytes;
    pe_hd_index_config index;
} pe_hd_manifest_config;

/* Immutable caller bytes, valid throughout call. Versioned finite TSV; see
 * MANIFEST.md. No filesystem calls, decoding or provider reads. Copies accepted
 * records into index; input may be freed after return. out unchanged on ANY
 * failure. INVALID includes malformed/truncated text; LIMIT includes input,
 * line, item, path, scratch and index budgets; DUPLICATE is parsed key alias.
 * Scratch bounds record array only; index storage is separately bounded.
 * read/user lifetime and trusted immutable root policy remain caller-owned;
 * successful creation does not validate PNGs, filesystem authority or hashes.
 */
pe_hd_index_result pe_hd_manifest_create(const pe_hd_manifest_config *,
    const void *bytes, size_t length, pe_hd_index_read read, void *user,
    pe_hd_index **out);
#endif


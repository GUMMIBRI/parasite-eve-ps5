/* Independent read-only manifest/provider foundation; no pack implementation. */
#include "pe_hd_index.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct entry { PeHdFilename key; size_t path; } entry;
typedef struct pin {
    atomic_bool used;
    struct pe_hd_index *owner;
    pe_hd_index_bytes bytes;
} pin;
struct pe_hd_index {
    size_t count, budget;
    atomic_size_t retained;
    pe_hd_index_read read;
    void *user;
    entry *entries;
    char *paths;
    pin pins[PE_HD_INDEX_PINS];
};

static int key_compare(const PeHdFilename *a, const PeHdFilename *b)
{
#define CMP(f) if (a->f != b->f) return a->f < b->f ? -1 : 1
    CMP(mode); CMP(texture_hash); CMP(palette_hash);
    CMP(source_width); CMP(source_height); CMP(x); CMP(y);
    CMP(width); CMP(height); CMP(palette_first); CMP(palette_last);
#undef CMP
    return 0;
}
bool pe_hd_index_key_equal(const PeHdFilename *a, const PeHdFilename *b)
{ return a && b && key_compare(a, b) == 0; }
static int entry_compare(const void *a, const void *b)
{ return key_compare(&((const entry *)a)->key, &((const entry *)b)->key); }

static bool path_key(const pe_hd_index_record *r, PeHdFilename *key)
{
    size_t start = 0;
    if (!r->path || !r->length || r->length >= PE_HD_INDEX_MAX_PATH) return false;
    for (size_t i = 0; i <= r->length; ++i) {
        unsigned char c = i == r->length ? '/' : (unsigned char)r->path[i];
        if (c == '/') {
            size_t n = i - start;
            if (!n || (n == 1 && r->path[start] == '.') ||
                (n == 2 && r->path[start] == '.' && r->path[start+1] == '.') ||
                r->path[i-1] == '.') return false;
            if (i == r->length)
                return PeHd_ParseFilename(r->path + start, n, key);
            start = i + 1;
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
    }
    return false;
}

pe_hd_index_result pe_hd_index_create(const pe_hd_index_config *c,
    const pe_hd_index_record *records, size_t count, pe_hd_index_read read,
    void *user, pe_hd_index **out)
{
    size_t paths = 0, bytes;
    pe_hd_index *idx;
    if (!c || !out || !read || (count && !records)) return PE_HD_INDEX_INVALID;
    if (!c->max_items || c->max_items > PE_HD_INDEX_MAX_ITEMS || count > c->max_items ||
        !c->path_bytes || !c->storage_bytes || c->storage_bytes > PE_HD_MAX_TOTAL_BYTES ||
        !c->pinned_bytes || c->pinned_bytes > PE_HD_MAX_TOTAL_BYTES)
        return PE_HD_INDEX_LIMIT;
    /* count is capped before multiplication. Validate before any allocation. */
    for (size_t i = 0; i < count; ++i) {
        PeHdFilename key;
        if (!path_key(&records[i], &key)) return PE_HD_INDEX_INVALID;
        if (records[i].length + 1 > c->path_bytes - paths) return PE_HD_INDEX_LIMIT;
        paths += records[i].length + 1;
    }
    bytes = sizeof(*idx) + count * sizeof(entry);
    if (bytes > c->storage_bytes || paths > c->storage_bytes - bytes)
        return PE_HD_INDEX_LIMIT;
    /* Separate allocations avoid alignment assumptions for the entry array. */
    idx = calloc(1, sizeof(*idx));
    if (!idx) return PE_HD_INDEX_MEMORY;
    idx->entries = count ? malloc(count * sizeof(entry)) : NULL;
    idx->paths = paths ? malloc(paths) : NULL;
    if (count && (!idx->entries || !idx->paths)) {
        free(idx->entries); free(idx->paths); free(idx); return PE_HD_INDEX_MEMORY;
    }
    idx->count = count; idx->budget = c->pinned_bytes; idx->read = read; idx->user = user;
    atomic_init(&idx->retained, 0);
    for (size_t i = 0; i < PE_HD_INDEX_PINS; ++i) {
        atomic_init(&idx->pins[i].used, false); idx->pins[i].owner = idx;
    }
    paths = 0;
    for (size_t i = 0; i < count; ++i) {
        (void)path_key(&records[i], &idx->entries[i].key);
        idx->entries[i].path = paths;
        memcpy(idx->paths + paths, records[i].path, records[i].length);
        paths += records[i].length; idx->paths[paths++] = 0;
    }
    if (count) qsort(idx->entries, count, sizeof(entry), entry_compare);
    for (size_t i = 1; i < count; ++i) {
        if (pe_hd_index_key_equal(&idx->entries[i-1].key, &idx->entries[i].key)) {
            free(idx->entries); free(idx->paths); free(idx); return PE_HD_INDEX_DUPLICATE;
        }
    }
    *out = idx; return PE_HD_INDEX_OK;
}

static void unpin(void *user)
{
    pin *p = user;
    pe_hd_index *idx = p->owner;
    size_t bytes = p->bytes.retained_bytes;
    p->bytes.release(p->bytes.user);
    atomic_fetch_sub_explicit(&idx->retained, bytes, memory_order_relaxed);
    /* Last access: destroy or next pin can proceed only after this release. */
    atomic_store_explicit(&p->used, false, memory_order_release);
}

pe_hd_index_result pe_hd_index_pin(pe_hd_index *idx, const PeHdFilename *key,
                                  pe_hd_source *out)
{
    size_t lo = 0, hi, available;
    pin *p = NULL;
    pe_hd_index_bytes bytes = {0};
    if (!idx || !key || !out) return PE_HD_INDEX_INVALID;
    hi = idx->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int order = key_compare(key, &idx->entries[mid].key);
        if (order > 0) lo = mid + 1; else hi = mid;
    }
    if (lo == idx->count || key_compare(key, &idx->entries[lo].key))
        return PE_HD_INDEX_MISSING;
    for (size_t i = 0; i < PE_HD_INDEX_PINS; ++i)
        if (!atomic_load_explicit(&idx->pins[i].used, memory_order_acquire)) {
            p = &idx->pins[i]; break;
        }
    if (!p) return PE_HD_INDEX_BUSY;
    available = idx->budget - atomic_load_explicit(&idx->retained, memory_order_relaxed);
    if (!available) return PE_HD_INDEX_LIMIT;
    if (!idx->read(idx->user, idx->paths + idx->entries[lo].path, available, &bytes))
        return PE_HD_INDEX_READ;
    if (!bytes.data || !bytes.length || !bytes.release || bytes.retained_bytes < bytes.length) {
        if (bytes.release) bytes.release(bytes.user);
        return PE_HD_INDEX_INVALID;
    }
    if (bytes.retained_bytes > available || bytes.length > PE_HD_MAX_ENCODED_BYTES) {
        bytes.release(bytes.user);
        return PE_HD_INDEX_LIMIT;
    }
    p->bytes = bytes;
    atomic_fetch_add_explicit(&idx->retained, bytes.retained_bytes, memory_order_relaxed);
    atomic_store_explicit(&p->used, true, memory_order_release);
    *out = (pe_hd_source){bytes.data, bytes.length, bytes.retained_bytes, unpin, p};
    return PE_HD_INDEX_OK;
}

pe_hd_index_result pe_hd_index_destroy(pe_hd_index *idx)
{
    if (!idx) return PE_HD_INDEX_INVALID;
    for (size_t i = 0; i < PE_HD_INDEX_PINS; ++i)
        if (atomic_load_explicit(&idx->pins[i].used, memory_order_acquire))
            return PE_HD_INDEX_BUSY;
    free(idx->entries); free(idx->paths); free(idx); return PE_HD_INDEX_OK;
}


/* Original bounded provider; trusted immutable root policy, no sandbox claim. */
#include "pe_hd_pack.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#define pack_open _open
#define pack_read _read
#define pack_close _close
#define pack_stat _stat64
#define pack_fstat _fstat64
#define PACK_FLAGS (_O_RDONLY | _O_BINARY)
#define PACK_REGULAR(m) (((m) & _S_IFMT) == _S_IFREG)
#else
#include <unistd.h>
#define pack_open open
#define pack_read read
#define pack_close close
#define pack_stat stat
#define pack_fstat fstat
#define PACK_FLAGS (O_RDONLY | O_NONBLOCK)
#define PACK_REGULAR(m) S_ISREG(m)
#endif

typedef struct pack_pin {
    atomic_bool used;
    struct pe_hd_pack *owner;
    void *data;
    size_t size;
} pack_pin;
struct pe_hd_pack {
    char root[PE_HD_PACK_ROOT_BYTES];
    size_t root_length, budget;
    atomic_size_t retained;
    pack_pin pins[PE_HD_PACK_PINS];
};
size_t pe_hd_pack_metadata_bytes(void) { return sizeof(pe_hd_pack); }

static size_t bounded_length(const char *s, size_t max)
{
    size_t n = 0;
    if (!s) return max;
    while (n < max && s[n]) ++n;
    return n;
}
static bool relative_path(const char *s, size_t n)
{
    size_t start = 0;
    if (!n || n >= PE_HD_INDEX_MAX_PATH) return false;
    for (size_t i = 0; i <= n; ++i) {
        unsigned char c = i == n ? '/' : (unsigned char)s[i];
        if (c == '/') {
            size_t len = i - start;
            if (!len || s[i-1] == '.' || (len == 1 && s[start] == '.') ||
                (len == 2 && s[start] == '.' && s[start+1] == '.')) return false;
            start = i + 1;
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
    }
    return true;
}
pe_hd_pack_result pe_hd_pack_create(const pe_hd_pack_config *c, pe_hd_pack **out)
{
    size_t n;
    pe_hd_pack *p;
    if (!c || !out || c->policy != PE_HD_PACK_TRUSTED_IMMUTABLE_ROOT)
        return PE_HD_PACK_INVALID;
    n = bounded_length(c->root, PE_HD_PACK_ROOT_BYTES);
    if (!n || n == PE_HD_PACK_ROOT_BYTES) return PE_HD_PACK_INVALID;
    /* Root is caller authority; only relative paths have the restrictive ASCII grammar. */
#ifdef _WIN32
    if (!(n >= 3 && ((c->root[0] >= 'A' && c->root[0] <= 'Z') ||
        (c->root[0] >= 'a' && c->root[0] <= 'z')) && c->root[1] == ':' &&
        c->root[2] == '/')) return PE_HD_PACK_INVALID;
    if (n > 3 && c->root[n-1] == '/') return PE_HD_PACK_INVALID;
#else
    if (c->root[0] != '/' || (n > 1 && c->root[n-1] == '/')) return PE_HD_PACK_INVALID;
#endif
    if (!c->pinned_bytes || c->pinned_bytes > PE_HD_MAX_TOTAL_BYTES)
        return PE_HD_PACK_LIMIT;
    p = calloc(1, sizeof(*p));
    if (!p) return PE_HD_PACK_MEMORY;
    memcpy(p->root, c->root, n + 1); p->root_length = n; p->budget = c->pinned_bytes;
    atomic_init(&p->retained, 0);
    for (size_t i = 0; i < PE_HD_PACK_PINS; ++i) {
        atomic_init(&p->pins[i].used, false); p->pins[i].owner = p;
    }
    *out = p; return PE_HD_PACK_OK;
}
static void release_pin(void *user)
{
    pack_pin *pin = user;
    pe_hd_pack *p = pin->owner;
    size_t size = pin->size;
    free(pin->data);
    atomic_fetch_sub_explicit(&p->retained, size, memory_order_relaxed);
    /* Last access; close or slot reuse may proceed after this store. */
    atomic_store_explicit(&pin->used, false, memory_order_release);
}
bool pe_hd_pack_read(void *user, const char *path, size_t limit, pe_hd_index_bytes *out)
{
    pe_hd_pack *p = user;
    pack_pin *pin = NULL;
    char full[PE_HD_PACK_ROOT_BYTES + PE_HD_INDEX_MAX_PATH + 1];
    struct pack_stat st, after;
    size_t n, cap, size, offset = 0;
    unsigned char *data, extra;
    int fd;
    if (!out) return false;
    *out = (pe_hd_index_bytes){0};
    if (!p || !limit) return false;
    n = bounded_length(path, PE_HD_INDEX_MAX_PATH);
    if (!relative_path(path, n)) return false;
    cap = p->budget - atomic_load_explicit(&p->retained, memory_order_relaxed);
    if (cap > limit) cap = limit;
    if (cap > PE_HD_MAX_ENCODED_BYTES) cap = PE_HD_MAX_ENCODED_BYTES;
    if (!cap) return false;
    for (size_t i = 0; i < PE_HD_PACK_PINS; ++i)
        if (!atomic_load_explicit(&p->pins[i].used, memory_order_acquire)) {
            pin = &p->pins[i]; break;
        }
    if (!pin) return false;
    memcpy(full, p->root, p->root_length);
    offset = p->root_length;
    if (full[offset-1] != '/') full[offset++] = '/';
    memcpy(full + offset, path, n + 1);
    fd = pack_open(full, PACK_FLAGS);
    if (fd < 0) return false;
    if (pack_fstat(fd, &st) || !PACK_REGULAR(st.st_mode) || st.st_size <= 0 ||
        (uintmax_t)st.st_size > (uintmax_t)cap) {
        (void)pack_close(fd); return false;
    }
    size = (size_t)st.st_size;
    data = malloc(size);
    if (!data) { (void)pack_close(fd); return false; }
    offset = 0;
    while (offset < size) {
        /* Finite number of calls and portable _read unsigned-int count. */
        unsigned chunk = (unsigned)(size - offset > 65536 ? 65536 : size - offset);
        int64_t got = pack_read(fd, data + offset, chunk);
        if (got <= 0) goto fail;
        offset += (size_t)got;
    }
    /* Reject observed growth/truncation; does NOT prove same-size immutability. */
    if (pack_read(fd, &extra, 1) != 0 || pack_fstat(fd, &after) ||
        after.st_size != st.st_size) goto fail;
    if (pack_close(fd)) { free(data); return false; }
    pin->data = data; pin->size = size;
    atomic_fetch_add_explicit(&p->retained, size, memory_order_relaxed);
    atomic_store_explicit(&pin->used, true, memory_order_release);
    *out = (pe_hd_index_bytes){data, size, size, release_pin, pin};
    return true;
fail:
    (void)pack_close(fd); free(data); return false;
}
pe_hd_pack_result pe_hd_pack_close(pe_hd_pack *p)
{
    if (!p) return PE_HD_PACK_INVALID;
    for (size_t i = 0; i < PE_HD_PACK_PINS; ++i)
        if (atomic_load_explicit(&p->pins[i].used, memory_order_acquire)) return PE_HD_PACK_BUSY;
    free(p); return PE_HD_PACK_OK;
}


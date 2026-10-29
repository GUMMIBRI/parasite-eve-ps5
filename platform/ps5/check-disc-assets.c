/* Read-only host preflight using the game's actual ISO/CUE/BIN implementation.
 * This does not execute the PS5 ELF or prove console file access. */
#include "pe1/cdrom.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: check-disc-assets IMAGE DISC_NUMBER [EXTRACTED_PE.IMG]\n");
        return 2;
    }
    unsigned identity = (unsigned)strtoul(argv[2], NULL, 10);
    if (identity != 1 && identity != 2) return 2;
    PeDisc disc;
    if (!pe_disc_open(&disc, argv[1])) {
        fprintf(stderr, "Disc open failed: %s\n", disc.error);
        return 1;
    }
    char marker[40], executable[30];
    snprintf(marker, sizeof(marker), "FMV%u/PEDISC0%u.IDF", identity, identity);
    snprintf(executable, sizeof(executable), "SLUS_006.%s", identity == 1 ? "62" : "68");
    PeDiscFile file, archive;
    int ok = pe_disc_find(&disc, marker, &file) && !file.directory &&
             pe_disc_find(&disc, executable, &file) && !file.directory &&
             pe_disc_find(&disc, "PE.IMG", &archive) && !archive.directory;
    FILE *reference = ok && argc == 4 ? fopen(argv[3], "rb") : NULL;
    if (argc == 4 && !reference) ok = 0;
    unsigned char actual[65536], expected[65536];
    uint64_t offset = 0;
    uint64_t first_mismatch = UINT64_MAX;
    int match = 1;
    while (ok && offset < archive.size) {
        size_t count = archive.size - offset;
        if (count > sizeof(actual)) count = sizeof(actual);
        ok = pe_disc_read(&disc, &archive, offset, actual, count);
        if (ok && reference) {
            size_t got = fread(expected, 1, count, reference);
            if (got != count || memcmp(actual, expected, count)) {
                if (match) {
                    size_t i = 0;
                    while (i < got && actual[i] == expected[i]) ++i;
                    first_mismatch = offset + i;
                }
                match = 0;
            }
        }
        if (ok) offset += count;
    }
    if (reference && (fgetc(reference) != EOF || ferror(reference))) match = 0;
    if (reference) fclose(reference);
    printf("disc=%u sector_size=%u archive_bytes=%u read_bytes=%llu archive_readable=%s reference_supplied=%s exact_archive_match=%s first_mismatch=%llu\n",
           identity, disc.sector_size, ok ? archive.size : 0,
           (unsigned long long)offset, ok ? "true" : "false", reference ? "true" : "false",
           reference && ok && match ? "true" : "false", (unsigned long long)first_mismatch);
    if (!ok) fprintf(stderr, "Asset preflight failed: %s\n", disc.error);
    pe_disc_close(&disc);
    return !ok ? 1 : !match ? 3 : 0;
}

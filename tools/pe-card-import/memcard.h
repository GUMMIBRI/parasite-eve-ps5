#ifndef PE_MEMCARD_H
#define PE_MEMCARD_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#define PE_MEMCARD_BYTES 131072u
#define PE_MEMCARD_PAGE_BYTES 128u
#define PE_MEMCARD_BLOCK_BYTES 8192u
typedef enum {PE_MEMCARD_RAW,PE_MEMCARD_GME} PeMemCardFormat;
typedef struct {FILE *file;unsigned offset;PeMemCardFormat format;} PeMemCard;
typedef struct {
 uint8_t state;uint32_t bytes;uint16_t next;
 unsigned char filename[21]; /* Original fixed bytes; no encoding assumptions. */
 int checksum_valid,state_valid;
} PeMemCardEntry;
/* Borrowed read-only stream. Content-based recognition, exact sizes:
 * rawMCR/SRM131072, DexDriveGME134976 with standard12byte signature.
 * Header MC/frame checksum validated; directory damage is reported separately.
 * No APIs write, normalize on disk, close stream, or interpret game state.
 */
int PeMemCard_Attach(PeMemCard *card,FILE *file);
/* Rejected ranges leave output untouched. An actual I/O failure can partially
 * write output. Normalized offsets always refer to the original128KiB card. */
int PeMemCard_Read(PeMemCard *card,size_t offset,size_t bytes,void *output,size_t capacity);
int PeMemCard_ReadPages(PeMemCard *card,unsigned page,unsigned count,void *output,size_t capacity);
int PeMemCard_ReadBlocks(PeMemCard *card,unsigned block,unsigned count,void *output,size_t capacity);
int PeMemCard_GetEntry(PeMemCard *card,unsigned block,PeMemCardEntry *entry);
/* Validate one active/deleted file chain, including XOR checksums, state
 * progression, pointer bounds, cycles and size. Returns actual blocks1..15.
 * Does not claim validity of unrelated files or deleted chains after reuse.
 * On failure output and *count are untouched. */
int PeMemCard_GetChain(PeMemCard *card,unsigned first,uint8_t blocks[15],size_t *count);
/* Sources: Sony FileFormat47.pdf memory-card directory specification;
 * psx-spx memory-card-images/data-format. Independently written adapter.
 * https://psx.arthus.net/sdk/Psy-Q/DOCS/FileFormat47.pdf
 * https://psx-spx.consoledev.net/controllersandmemorycards/
 */
#endif


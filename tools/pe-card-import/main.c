/* Copyright 2026 PE-PORT contributors. SPDX-License-Identifier: Apache-2.0
 * Independent host CLI; no game/SDK source is compiled into this tool. */
#include "memcard.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p,0700)
#endif

typedef struct { char name[21]; unsigned block; unsigned char data[8192]; } Save;
static Save saves[15];
static int fail(const char *why) { fprintf(stderr,"ERROR: %s\n",why); return 1; }
static void help(void) {
    puts("pe-card-import --card IMAGE [--list | --extract --output NEW_DIRECTORY] [--port 0|1]\n"
         "Default --list is read-only. Exactly one explicitly selected raw MCR/SRM\n"
         "(131072 bytes) or DexDrive GME (134976 bytes) is read; no scanning.\n"
         "Extracts all active compatible single-block USA PE saves after preflight.\n"
         "The entire output directory must be new; its parent must already exist.\n"
         "Creates NEW_DIRECTORY/memory-cards/bu00 or bu10 plus COMPLETE.txt last.\n"
         "On error, incomplete staging may remain; never transfer without COMPLETE.txt.\n"
         "Transfer root is NEW_DIRECTORY: manually copy its memory-cards contents\n"
         "to /data/parasite-eve/memory-cards (or your PE_CONFIG_HOME/memory-cards).\n"
         "Back up console saves first; skip existing names and newer saves. Never\n"
         "replace the whole console directory. This tool never contacts a console.\n"
         "Original cards are retained unchanged. No game payload/checksum validation,\n"
         "repair, multiblock PE, native individual saves, VMP/VGS/PSV or other regions.");
}
static int safe_name(const unsigned char *p) {
    /* Host importer: 20 bytes + USA prefix. Menu: slot at byte19 in A..O.
     * Require safe ASCII and a real terminator; never sanitize/rename entries. */
    if (p[20] || memcmp(p,"BASLUS-00662",12) || p[19]<'A' || p[19]>'O') return 0;
    for (unsigned i=0;i<20;i++)
        if (!((p[i]>='A'&&p[i]<='Z') || (p[i]>='a'&&p[i]<='z') ||
              (p[i]>='0'&&p[i]<='9') || p[i]=='-' || p[i]=='_')) return 0;
    return 1;
}
static int path(char *dst, size_t cap, const char *root, const char *tail) {
    int n=snprintf(dst,cap,"%s/%s",root,tail);
    return n>=0 && (size_t)n<cap;
}
static int write_new(const char *name,const void *data,size_t size) {
    FILE *f=fopen(name,"wbx");
    if (!f) return 0;
    int ok=fwrite(data,1,size,f)==size;
    if (fflush(f)) ok=0;
    if (fclose(f)) ok=0;
    return ok;
}
int main(int argc,char **argv) {
    const char *cardpath=NULL,*out=NULL; int extract=0,mode=0,port=0,port_set=0;
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--help")) { help(); return 0; }
        if (!strcmp(argv[i],"--card") && i+1<argc && !cardpath) cardpath=argv[++i];
        else if (!strcmp(argv[i],"--output") && i+1<argc && !out) out=argv[++i];
        else if (!strcmp(argv[i],"--port") && i+1<argc && !port_set) {
            const char *p=argv[++i]; if (strcmp(p,"0")&&strcmp(p,"1")) return fail("port must be 0 or 1");
            port=*p-'0'; port_set=1;
        } else if ((!strcmp(argv[i],"--list") || !strcmp(argv[i],"--extract")) && !mode) {
            extract=!strcmp(argv[i],"--extract"); mode=1;
        } else return fail("invalid/duplicate argument; use --help");
    }
    if (!cardpath || (extract ? !out || !*out : out!=NULL)) return fail("select --card; --extract requires --output (list forbids output)");
    FILE *f=fopen(cardpath,"rb"); PeMemCard card;
    if (!f) return fail("cannot open selected card read-only");
    if (!PeMemCard_Attach(&card,f)) { fclose(f); return fail("unsupported size/signature or damaged MC header"); }
    PeMemCardEntry entries[16]; unsigned owned=0,active=0,n=0; int bad=0;
    printf("Format: %s; target bu%d0\n",card.format==PE_MEMCARD_GME?"DexDrive GME":"raw MCR/SRM",port);
    for (unsigned b=1;b<=15;b++) {
        unsigned char directory[128];
        if (!PeMemCard_ReadPages(&card,b,1,directory,sizeof(directory)) ||
            directory[1] || directory[2] || directory[3] ||
            !PeMemCard_GetEntry(&card,b,&entries[b]) || !entries[b].checksum_valid || !entries[b].state_valid) {
            fprintf(stderr,"block %u: malformed directory\n",b); bad=1; continue;
        }
        unsigned s=entries[b].state;
        if (s>=0x51 && s<=0x53) active|=1u<<b;
        else printf("block %u: skip %s\n",b,(s>=0xa1&&s<=0xa3)?"deleted":"free/unavailable");
    }
    if (bad) { fclose(f); return fail("directory validation failed; no output created"); }
    for (unsigned b=1;b<=15;b++) if (entries[b].state==0x51) {
        uint8_t chain[15]; size_t count=0;
        if (!PeMemCard_GetChain(&card,b,chain,&count)) { bad=1; fprintf(stderr,"block %u: malformed/cyclic chain\n",b); continue; }
        for (size_t k=0;k<count;k++) {
            unsigned bit=1u<<chain[k]; if (owned&bit) bad=1; owned|=bit;
        }
        if (memcmp(entries[b].filename,"BASLUS-00662",12)) { printf("block %u: skip other game/region\n",b); continue; }
        if (!safe_name(entries[b].filename) || count!=1 || entries[b].bytes!=8192) {
            fprintf(stderr,"block %u: unsupported PE name/size/multiblock save\n",b); bad=1; continue;
        }
        Save *s=&saves[n]; memcpy(s->name,entries[b].filename,21); s->block=b;
        if (!PeMemCard_ReadBlocks(&card,b,1,s->data,sizeof(s->data)) ||
            s->data[0]!='S' || s->data[1]!='C' || s->data[2]<0x11 || s->data[2]>0x13 || s->data[3]!=1) {
            fprintf(stderr,"block %u: invalid single-block SC header\n",b); bad=1; continue;
        }
        for (unsigned j=0;j<n;j++) if (!strcmp(s->name,saves[j].name) || s->name[19]==saves[j].name[19]) {
            fprintf(stderr,"block %u: duplicate name/native slot\n",b); bad=1;
        }
        printf("block %u: compatible %s (8192 bytes)\n",b,s->name); n++;
    }
    if (owned!=active) bad=1;
    if (fclose(f)) bad=1;
    if (bad) return fail("invalid/overlapping/orphan/unsupported directory or save; no output created");
    printf("Preflight complete: %u compatible saves\n",n);
    if (!extract) return 0;
    if (!n) return fail("no compatible saves; no output created");
    char cards[4096],dest[4096],filename[4096],manifest[4096];
    char tail[32]; snprintf(tail,sizeof(tail),"memory-cards/bu%d0",port);
    if (!path(cards,sizeof(cards),out,"memory-cards") || !path(dest,sizeof(dest),out,tail) ||
        !path(manifest,sizeof(manifest),out,"COMPLETE.txt") || strlen(dest)+22>=sizeof(filename)) return fail("output path too long");
    if (MKDIR(out)) return fail("output root must be newly created (parent must exist)");
    if (MKDIR(cards) || MKDIR(dest)) return fail("cannot create staging directories; incomplete root retained");
    for (unsigned j=0;j<n;j++) {
        if (!path(filename,sizeof(filename),dest,saves[j].name) || !write_new(filename,saves[j].data,8192))
            return fail("exclusive save write failed; incomplete root retained, do not transfer");
    }
    char pending[4096];
    if (!path(pending,sizeof(pending),out,"INCOMPLETE.txt")) return fail("manifest path too long");
    FILE *m=fopen(pending,"wbx");
    if (!m) return fail("cannot create completion manifest; do not transfer");
    int ok=fprintf(m,"PE card extraction COMPLETE\nTransfer root: this directory, NOT memory-cards/bu%d0.\n"
        "Target: /data/parasite-eve/memory-cards/bu%d0 (or PE_CONFIG_HOME/memory-cards/bu%d0).\n"
        "Original card retained unchanged. Files below are exact block bytes, not repaired.\n"
        "Back up console saves; manually copy missing files only. Never overwrite newer saves.\n"
        "No payload/checksum/gameplay validation. Source format: %s.\n",port,port,port,
        card.format==PE_MEMCARD_GME?"GME":"raw")>0;
    for (unsigned j=0;j<n;j++) if (fprintf(m,"block=%u bytes=8192 path=memory-cards/bu%d0/%s\n",saves[j].block,port,saves[j].name)<0) ok=0;
    if (fflush(m)) ok=0;
    if (fclose(m)) ok=0;
    if (!ok) return fail("manifest write failed; incomplete root retained");
#ifdef _WIN32
    /* Windows rename refuses an existing target. */
    if (rename(pending,manifest)) return fail("completion publication failed; do not transfer");
#else
    /* POSIX rename can replace a target; hard link publishes without replacement. */
    if (link(pending,manifest)) return fail("completion publication failed; do not transfer");
    (void)unlink(pending);
#endif
    puts("Extraction complete; see COMPLETE.txt before manual transfer."); return 0;
}


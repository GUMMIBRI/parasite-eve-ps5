#include "memcard.h"
#include <string.h>
#ifdef _WIN32
#define SEEK64 _fseeki64
#define TELL64 _ftelli64
#else
#define SEEK64 fseeko
#define TELL64 ftello
#endif
static unsigned checksum(const unsigned char *page) {
 unsigned sum=0;for(unsigned i=0;i<128;i++)sum^=page[i];return sum;
}
int PeMemCard_Attach(PeMemCard *card,FILE *file) {
 if(!card)return 0;*card=(PeMemCard){0};
 if(!file || SEEK64(file,0,SEEK_END))return 0;
 int64_t size=TELL64(file);PeMemCard candidate={.file=file};
 if(size==PE_MEMCARD_BYTES)candidate.format=PE_MEMCARD_RAW;
 else if(size==PE_MEMCARD_BYTES+3904) {
  unsigned char marker[12];
  if(SEEK64(file,0,SEEK_SET) || fread(marker,1,12,file)!=12 || memcmp(marker,"123-456-STD\0",12))return 0;
  candidate.format=PE_MEMCARD_GME;candidate.offset=3904;
 } else return 0;
 unsigned char header[128];
 if(SEEK64(file,candidate.offset,SEEK_SET) || fread(header,1,128,file)!=128 ||
    header[0]!='M' || header[1]!='C' || checksum(header))return 0;
 *card=candidate;return 1;
}
int PeMemCard_Read(PeMemCard *card,size_t offset,size_t bytes,void *output,size_t capacity) {
 if(!card || !card->file || !output || !bytes || bytes>capacity ||
    offset>=PE_MEMCARD_BYTES || bytes>PE_MEMCARD_BYTES-offset)return 0;
 if(SEEK64(card->file,(int64_t)card->offset+(int64_t)offset,SEEK_SET))return 0;
 return fread(output,1,bytes,card->file)==bytes;
}
int PeMemCard_ReadPages(PeMemCard *card,unsigned page,unsigned count,void *output,size_t capacity) {
 if(page>=1024 || !count || count>1024-page)return 0;
 return PeMemCard_Read(card,(size_t)page*128,(size_t)count*128,output,capacity);
}
int PeMemCard_ReadBlocks(PeMemCard *card,unsigned block,unsigned count,void *output,size_t capacity) {
 if(block>=16 || !count || count>16-block)return 0;
 return PeMemCard_Read(card,(size_t)block*8192,(size_t)count*8192,output,capacity);
}
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int PeMemCard_GetEntry(PeMemCard *card,unsigned block,PeMemCardEntry *entry) {
 if(!entry || block<1 || block>15)return 0;
 unsigned char page[128];if(!PeMemCard_ReadPages(card,block,1,page,sizeof(page)))return 0;
 PeMemCardEntry e={.state=page[0],.bytes=word(page+4),.next=(uint16_t)(page[8]|((unsigned)page[9]<<8)),.checksum_valid=!checksum(page)};
 memcpy(e.filename,page+10,21);
 e.state_valid=(page[0]==0xa0 || page[0]==0x51 || page[0]==0x52 || page[0]==0x53 ||
                page[0]==0xa1 || page[0]==0xa2 || page[0]==0xa3 || page[0]==0xff);
 *entry=e;return 1;
}
int PeMemCard_GetChain(PeMemCard *card,unsigned first,uint8_t blocks[15],size_t *count) {
 if(!blocks || !count || first<1 || first>15)return 0;
 PeMemCardEntry entry;if(!PeMemCard_GetEntry(card,first,&entry) || !entry.checksum_valid ||
   (entry.state!=0x51 && entry.state!=0xa1))return 0;
 uint32_t bytes=entry.bytes;unsigned family=entry.state&0xf0,block=first,seen=0;
 uint8_t chain[15];size_t n=0;
 for(;;) {
  if(block<1 || block>15 || (seen&(1u<<block)) || n==15)return 0;
  if(!PeMemCard_GetEntry(card,block,&entry) || !entry.checksum_valid)return 0;
  if(n==0) {if(entry.state!=(family|1))return 0;}
  else if(entry.state!=(family|(entry.next==0xffff?3:2)))return 0;
  chain[n++]=(uint8_t)block;seen|=1u<<block;
  if(entry.next==0xffff)break;
  if(entry.next>=15)return 0;
  block=entry.next+1;
 }
 if(!bytes || bytes>(uint32_t)n*8192 || bytes<=(uint32_t)(n-1)*8192)return 0;
 memcpy(blocks,chain,n);*count=n;return 1;
}


/* SPDX-License-Identifier: MIT. Selected homebrew app and sibling asset storage.
 * Raw public kernel APIs only, including before CRT. No FILE/stat layout,
 * heap, TLS, cwd, symlink or build-token dependency. */
#include <stddef.h>
#include "pe-portable-root.h"
extern int sceKernelOpen(const char *,int,unsigned);
extern long sceKernelRead(int,void *,size_t);
extern int sceKernelClose(int);
static int initialized,mode,error,asset_state,app_asset_state;
static char root[200],app[160],user[180],storage[16];
static unsigned io_budget;
static unsigned char compare_a[4096],compare_b[4096];
static size_t len(const char*s){size_t n=0;while(s[n])n++;return n;}
static int append(char*d,size_t cap,const char*a,const char*b){size_t n=len(a),m=len(b);if(n+m>=cap)return 0;for(size_t i=0;i<n;i++)d[i]=a[i];for(size_t i=0;i<=m;i++)d[n+i]=b[i];return 1;}
static int missing(int r){return r==-2||(unsigned)r==0x80020002u;}
static int read_file(const char*path,char*data,size_t cap){int f=sceKernelOpen(path,0,0);if(f<0)return missing(f)?-2:-1;size_t n=0;int ok=1;while(n<cap){long got=sceKernelRead(f,data+n,cap-n);if(got<0||(size_t)got>cap-n){ok=0;break;}if(!got)break;n+=(size_t)got;}if(sceKernelClose(f)<0)ok=0;return ok?(int)n:-1;}
/* Exact identity, not a filename/title-ID guess. At most64MiB per binary and
 *256MiB total reads. Partial reads are accumulated; every descriptor closes. */
static int chunk(int f,unsigned char*out){size_t n=0;while(n<4096){if(!io_budget)return -1;size_t ask=4096-n;if(ask>io_budget)ask=io_budget;long got=sceKernelRead(f,out+n,ask);if(got<0||(size_t)got>ask)return -1;if(!got)break;io_budget-=(unsigned)got;n+=(size_t)got;}return(int)n;}
static int same_app(const char*path){
 int candidate=sceKernelOpen(path,0,0);if(candidate<0)return missing(candidate)?0:-1;
 int active=sceKernelOpen("/app0/eboot.bin",0,0);if(active<0){sceKernelClose(candidate);return -1;}
 int result=0;unsigned bytes=0;
 for(;;){int a=chunk(active,compare_a),b=chunk(candidate,compare_b);if(a<0||b<0){result=-1;break;}if(a!=b)break;if(!a){result=bytes>=4096?1:-1;break;}int equal=1;for(int i=0;i<a;i++)if(compare_a[i]!=compare_b[i]){equal=0;break;}if(!equal)break;bytes+=(unsigned)a;if(bytes>64u*1024u*1024u){result=-1;break;}}
 if(sceKernelClose(active)<0)result=-1;if(sceKernelClose(candidate)<0)result=-1;return result;
}
/* Manager supplies the selected physical title folder at compile time. The
 * legacy folder stays discoverable, but two exact matching copies fail closed.
 * This macro does not rewrite param.json or choose/reserve a title ID. */
#ifndef PE_NATIVE_APP_FOLDER
#define PE_NATIVE_APP_FOLDER "PPSA77022"
#endif
static int folder_ok(const char *s){
 if(len(s)==11){const char *legacy="PS5GameTest";unsigned i=0;for(;s[i]&&s[i]==legacy[i];i++){}return s[i]==0&&legacy[i]==0;}
 if(len(s)!=9)return 0;
 for(unsigned i=0;i<9;i++)if(i<4?(s[i]<'A'||s[i]>'Z'):(s[i]<'0'||s[i]>'9'))return 0;
 return !(s[0]=='N'&&s[1]=='P'&&s[2]=='X'&&s[3]=='S')&&!(s[0]=='C'&&s[1]=='U'&&s[2]=='S'&&s[3]=='A');
}
static int equal(const char*a,const char*b){unsigned i=0;while(a[i]&&a[i]==b[i])i++;return a[i]==b[i];}
/* A complete default is a readable, nonempty PAIR from one root, not a
 * disc-content/region validation. The normal disc loader validates that later.
 * Missing and unreadable are distinct; never silently fall back on app IO. */
static int disc_root(const char*base){
 char path[224];unsigned present=0,readable=0;int unreadable=0;
 for(unsigned i=0;i<2;i++){
  if(!append(path,sizeof path,base,i?"/discs/disc2.bin":"/discs/disc1.bin"))return PE_PORTABLE_ASSETS_UNREADABLE;
  int f=sceKernelOpen(path,0,0);
  if(f<0){if(!missing(f))unreadable=1;continue;}
  present++;unsigned char first;long got=sceKernelRead(f,&first,1);
  if(got==1)readable++;else if(got<0||got>1)unreadable=1;
  if(sceKernelClose(f)<0)unreadable=1;
 }
 if(unreadable)return PE_PORTABLE_ASSETS_UNREADABLE;
 if(readable==2)return PE_PORTABLE_ASSETS_COMPLETE;
 return present?PE_PORTABLE_ASSETS_INCOMPLETE:PE_PORTABLE_ASSETS_MISSING;
}
/* A paired new/legacy root is ambiguous, not an arbitrary first match. */
static unsigned storage_disc_roots(const char*mount,char*selected,int*incomplete,int*unreadable){
 static const char*const names[]={"/parasite-eve-data","/parasite-eve"};unsigned count=0;char base[32];
 for(unsigned n=0;n<2;n++)if(append(base,sizeof base,mount,names[n])){
  int state=disc_root(base);
  if(state==PE_PORTABLE_ASSETS_COMPLETE){count++;append(selected,32,base,"");}
  else if(state==PE_PORTABLE_ASSETS_INCOMPLETE)*incomplete=1;
  else if(state==PE_PORTABLE_ASSETS_UNREADABLE)*unreadable=1;
 }
 return count;
}
int PePortable_Mode(void){
 if(initialized)return mode;initialized=1;
#ifndef PE_NATIVE_SHARE_LAYOUT
 error=5;mode=-1;return mode;
#else
 if(!folder_ok(PE_NATIVE_APP_FOLDER)){error=4;mode=-1;return mode;}
 io_budget=256u*1024u*1024u;unsigned matches=0;char candidate[160],binary[180],home[64];
 const char*const leaves[]={PE_NATIVE_APP_FOLDER,"PS5GameTest","PEVG00022"};
 /* Same supported storage roots as RecompStore: internal, extended and USB. */
 for(unsigned volume=0;volume<11;volume++){
  char mount[16]="/data";
  if(volume==1)append(mount,sizeof mount,"/mnt/ext0","");
  else if(volume==2)append(mount,sizeof mount,"/mnt/ext1","");
  else if(volume>=3){append(mount,sizeof mount,"/mnt/usb0","");mount[8]=(char)('0'+volume-3);}
  for(unsigned leaf=0;leaf<3;leaf++){
   int duplicate=0;for(unsigned prior=0;prior<leaf;prior++)if(equal(leaves[prior],leaves[leaf]))duplicate=1;if(duplicate)continue;
   if(!append(home,sizeof home,mount,"/homebrew/")||!append(candidate,sizeof candidate,home,leaves[leaf])||!append(binary,sizeof binary,candidate,"/eboot.bin")){error=4;mode=-1;return mode;}
   int found=same_app(binary);if(found<0){error=5;mode=-1;return mode;}
   if(found){if(++matches>1){error=3;mode=-1;return mode;}append(app,sizeof app,candidate,"");append(storage,sizeof storage,mount,"");}
  }
 }
 if(matches!=1){error=2;mode=-1;return mode;}
 /* DEFAULT policy: a complete app-contained pair wins before external data
  * discovery. Saved/imported/command-line paths load later and are not changed.
  * Never combine partial pairs or optional defaults across distinct roots. */
 if(!append(root,sizeof root,app,"/parasite-eve-data")||!append(user,sizeof user,app,"/user")){error=4;mode=-1;return mode;}
 char selected[200],external[32];unsigned roots=0;
 app_asset_state=disc_root(root);
 if(app_asset_state==PE_PORTABLE_ASSETS_COMPLETE){asset_state=PE_PORTABLE_ASSETS_COMPLETE;mode=1;return mode;}
 if(app_asset_state==PE_PORTABLE_ASSETS_UNREADABLE){asset_state=PE_PORTABLE_ASSETS_UNREADABLE;mode=1;return mode;}
 int incomplete=app_asset_state==PE_PORTABLE_ASSETS_INCOMPLETE,unreadable=0;
 /* Only missing/incomplete app defaults allow unique external discovery.
  * A copy of one disc is never joined to a disc from a different root. */
 for(unsigned volume=0;volume<11;volume++){
  char mount[16]="/data";
  if(volume==1)append(mount,sizeof mount,"/mnt/ext0","");
  else if(volume==2)append(mount,sizeof mount,"/mnt/ext1","");
  else if(volume>=3){append(mount,sizeof mount,"/mnt/usb0","");mount[8]=(char)('0'+volume-3);}
  unsigned n=storage_disc_roots(mount,external,&incomplete,&unreadable);roots+=n;
  if(n==1)append(selected,sizeof selected,external,"");
 }
 if(roots>1)asset_state=PE_PORTABLE_ASSETS_AMBIGUOUS;
 else if(unreadable)asset_state=PE_PORTABLE_ASSETS_UNREADABLE;
 else if(roots==1){append(root,sizeof root,selected,"");asset_state=PE_PORTABLE_ASSETS_COMPLETE;}
 else asset_state=incomplete?PE_PORTABLE_ASSETS_INCOMPLETE:PE_PORTABLE_ASSETS_MISSING;
 mode=1;return mode;
#endif
}
int PePortable_Error(void){(void)PePortable_Mode();return error;}
const char*PePortable_Root(void){return PePortable_Mode()==1?root:NULL;}
const char*PePortable_AppRoot(void){return PePortable_Mode()==1?app:NULL;}
const char*PePortable_UserRoot(void){return PePortable_Mode()==1?user:NULL;}
const char*PePortable_StorageRoot(void){return PePortable_Mode()==1?storage:NULL;}
int PePortable_AssetsState(void){(void)PePortable_Mode();return asset_state;}
const char*PePortable_ErrorText(void){
 switch(PePortable_Error()){
 case 0:return "Application executable discovery succeeded.";
 case 2:return "The running application could not be matched to a supported app folder; no data fallback was attempted.";
 case 3:return "Multiple exact copies of this application were found; executable location is ambiguous.";
 case 4:return "The selected application folder or storage path is invalid or too long.";
 case 5:
#ifdef PE_NATIVE_SHARE_LAYOUT
  return "Application executable discovery encountered an access, read or close error; no data fallback was attempted.";
#else
  return "This build has no supported portable application storage role.";
#endif
 default:return "Application executable discovery failed for an unknown reason.";
 }
}
const char*PePortable_AssetsText(void){
 if(PePortable_Mode()!=1)return PePortable_ErrorText();
 switch(asset_state){
 case PE_PORTABLE_ASSETS_COMPLETE:
  if(app_asset_state==PE_PORTABLE_ASSETS_COMPLETE)return "Complete app-contained discs selected by default; external data discovery was skipped.";
  return app_asset_state==PE_PORTABLE_ASSETS_INCOMPLETE?
   "The app-contained disc pair is incomplete; one complete external data folder supplies the defaults.":
   "No app-contained disc pair exists; one complete external data folder supplies the defaults.";
 case PE_PORTABLE_ASSETS_AMBIGUOUS:return "Multiple complete external data folders exist; set explicit disc and texture paths instead of choosing a copy implicitly.";
 case PE_PORTABLE_ASSETS_INCOMPLETE:return "No complete disc pair was found; a data folder has only one disc or an empty file. Keep both discs together in parasite-eve-data/discs.";
 case PE_PORTABLE_ASSETS_UNREADABLE:return app_asset_state==PE_PORTABLE_ASSETS_UNREADABLE?
  "App-contained disc files could not be read or closed; external data fallback was not attempted.":
  "External data discovery encountered unreadable files; automatic default selection was stopped.";
 default:return "No disc files were found at the default data locations. Add both disc1.bin and disc2.bin to app/parasite-eve-data/discs or configure explicit paths.";
 }
}
int PePortable_ImportConfig(char *path,unsigned cap){
 if(PePortable_Mode()!=1||!path||cap<2)return 0;
 char file[200];append(file,sizeof file,app,"/use-existing-config.txt");int n=read_file(file,path,cap-1);
 if(n==-2)return 0;if(n<=0||(unsigned)n>=cap-1)return -1;
 while(n&&(path[n-1]=='\n'||path[n-1]=='\r'))n--;path[n]=0;
 if(path[0]!='/')return -1;
 for(int i=0;i<n;i++){unsigned char ch=(unsigned char)path[i];if(ch<32||ch==127||ch=='\\'||ch==':')return -1;if(ch=='/'&&path[i+1]=='.'&&(path[i+2]==0||path[i+2]=='/'||(path[i+2]=='.'&&(path[i+3]==0||path[i+3]=='/'))))return -1;}
 return 1;
}

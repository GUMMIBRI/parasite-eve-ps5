/* SPDX-License-Identifier: MIT. Native settings; normal memory cards are independent. */
#ifndef PE_NATIVE_TABS_H
#define PE_NATIVE_TABS_H
#include "native_registry.h"
/* Read-only counterpart of Host_CardPath's preference-parent routing. Never
 * call that callback for help: it creates directories as a side effect. */
static void Host_ModernCardFolder(int slot,char*out,unsigned size){
 const char *slash=strrchr(host_options.user_file,'/');
 const char *backslash=strrchr(host_options.user_file,'\\');
 if(backslash&&(!slash||backslash>slash))slash=backslash;
 if(!slash){snprintf(out,size,"Active card folder unavailable");return;}
 int n=snprintf(out,size,"%.*s/memory-cards/bu%d0",(int)(slash-host_options.user_file),host_options.user_file,slot);
 if(n<0||(unsigned)n>=size)snprintf(out,size,"Active card path exceeds display capacity");
}
static int Host_ModernRowCount(int page){if(page==HOST_PAGE_MODS||page==HOST_PAGE_CHEATS)return Host_RegistryCount(page);return page==HOST_PAGE_IMAGES?12:0;}
static const char *Host_ModernRowLabel(int page,int row){
 if(page==HOST_PAGE_MODS||page==HOST_PAGE_CHEATS)return Host_RegistryLabel(page,row);
 static const char *const system[]={"Menu appearance","Menu text","Focus pointer","Disc 1","Disc 2","Game files","Save data","Card 1 folder","Card 2 folder","Copy PC saves","Error reports","Build"};
 return page==HOST_PAGE_IMAGES&&row>=0&&row<12?system[row]:"";
}
static int Host_ModernRowKind(int page,int row){if(page==HOST_PAGE_MODS||page==HOST_PAGE_CHEATS)return Host_RegistryKind(page,row);return page==HOST_PAGE_IMAGES?(row<3?2:0):0;}
static void Host_ModernRowValue(int page,int row,char*out,unsigned size){
 if(page==HOST_PAGE_MODS||page==HOST_PAGE_CHEATS){Host_RegistryValue(page,row,out,size);return;}
 if(page==HOST_PAGE_IMAGES&&(row==7||row==8)){Host_ModernCardFolder(row-7,out,size);return;}
 const char*value="";if(page==HOST_PAGE_IMAGES){if(row==3||row==4)value=g_CdImageVolumes[row-3]?"Validated and mounted":"Missing: check disc path";else if(row==5)value="Configured asset folders";else if(row==6)value="Normal memory cards";else if(row==9)value="8192-byte BAS files only";else if(row==10)value="Separate launch logs";else if(row==11)value=PE_BUILD_ID;}snprintf(out,size,"%s",value);
}
static const char *Host_ModernRowHelp(int page,int row){
 if(page==HOST_PAGE_MODS||page==HOST_PAGE_CHEATS)return Host_RegistryHelp(page,row);
 if(page==HOST_PAGE_IMAGES){
  if(row==3||row==4)return g_CdImageVolumes[row-3]?"This USA image is validated and mounted. Existing mounted images are not replaced while playing.":"Place disc1.bin and disc2.bin in parasite-eve-data/discs, or correct saved disc paths, then restart.";
  if(row==5)return "Uses existing detected or configured disc and texture folders in place. No disc installation or copying is required.";
  if(row==6)return "Close the game before copying saves. Back up both card folders first; preserve originals. Restart after copying, then use the original Load menu.";
  if(row==7||row==8){static char path[sizeof(host_options.user_file)+80];Host_ModernCardFolder(row-7,path,sizeof path);return path;}
  if(row==9)return "On PC, copy individual native 8192-byte BAS save files to Card 1 or Card 2 above; keep filenames. Whole .mcr/.mcd images are unsupported. No import or folder browser.";
  if(row==10)return "Each launch has its own build-stamped log and toggle/error records. Restart does not overwrite an earlier report.";
  if(row==11)return "Source build identity supplied by PE_BUILD_ID. Compilation and source reviews do not certify every scene; human playtesting remains authoritative.";
 }
 return "";
}
static int Host_ModernAction(int page,int row,int direction,int activate){
 if(page==HOST_PAGE_MODS||page==HOST_PAGE_CHEATS)return Host_RegistryAction(page,row,direction,activate);
 return 0;
}
#endif

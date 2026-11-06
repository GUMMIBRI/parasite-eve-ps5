/* SPDX-License-Identifier: MIT -- selected application storage roots; no libc before CRT. */
#ifndef PE_PORTABLE_ROOT_H
#define PE_PORTABLE_ROOT_H
/* 1: unique active PS5GameTest app in local/USB homebrew; -1: missing/ambiguous/IO failure. No marker. */
int PePortable_Mode(void);
int PePortable_Error(void);
const char *PePortable_Root(void);
const char *PePortable_AppRoot(void);
const char *PePortable_UserRoot(void);
const char *PePortable_StorageRoot(void);
/* Defaults only: explicit saved/command-line asset paths remain authoritative. */
enum { PE_PORTABLE_ASSETS_MISSING=0, PE_PORTABLE_ASSETS_COMPLETE=1,
       PE_PORTABLE_ASSETS_AMBIGUOUS=2, PE_PORTABLE_ASSETS_INCOMPLETE=3,
       PE_PORTABLE_ASSETS_UNREADABLE=4 };
int PePortable_AssetsState(void);
/* Static diagnostics only: no private filenames, allocations or logging before CRT. */
const char *PePortable_ErrorText(void);
const char *PePortable_AssetsText(void);
/* Optional explicit read-only settings import. 0 absent, 1 path, -1 invalid. */
int PePortable_ImportConfig(char *path,unsigned capacity);
#endif

# PS5 filesystem paths (F494 startup failure)

Owner evidence: real `chdir(/data/parasite-eve)` returns -1/EPERM immediately
before main returns 1. Raw startup.log open/write succeeds in that directory.
`dup2(12,1/2)` also fails EPERM, so stderr alone cannot diagnose later failures.
The termination-time SIGSYS remains a separate unresolved failure.

Pre-change trace, completed before implementation:

* main's PE_PS5 chdir is the failing early return. PE_DATA_HOME selects the data
  directory; absent/empty selects /data/parasite-eve. It must now be absolute.
* Host_InitOptions loads SDL base-path defaults, a cwd parasite-eve.toml, then
  PE_CONFIG_HOME/parasite-eve.toml. PE_CONFIG_HOME formerly defaults to data_root.
  PS5 will use only explicitly resolved data/config paths; desktop keeps its
  existing defaults. A relative config override resolves against data_root.
* Host_SaveOptions writes user_file.tmp then renames to user_file. Making
  user_file absolute covers both operations and the mod PeHookInit prefs_file.
* Config [discs] and --disc/--disc2 reach main's pe_disc_open; later selection
  uses Host_InstallImage -> pe_disc_open. Resolve both at one host boundary.
  scene_assets.c cue_image already resolves relative FILE names against the CUE
  parent. Absolute CUE/BIN inputs remain absolute. Game files, overlays and
  backgrounds are read from the mounted PeDisc, not host-relative asset paths.
* Host_CardPath maps bu00:..bu13: to the user_file parent's memory-cards tree.
  Psyz_AdjustPathCB covers plat_unix open/stat/erase/enumeration/format and
  save_flow.c staged save writes/renames/removal. Host_InstallCardPaths also
  checks legacy bu00/bu10 under cwd and SDL base; PS5 cwd source must instead
  explicitly use data_root. A PS5 mapping failure must not fall back to cwd.
* PsyZ libcard.c _bu_init separately mkdir's relative bu00/bu10, bypassing that
  callback. Stage a PS5-only callback-based initialization in patch-game.ps1;
  preserve the pinned checkout and desktop behavior.
* mods/host pe_host_start derives s_prefs_dir from PeHookInit.prefs_file;
  pe_mod_path supplies pe_mods.ini, temporary settings, pe_mods.log, pe_play.log
  and mod content paths. Absolute prefs_file covers these when enabled.
  Current game.cmake forces PE_MODS OFF; plugin candidate stays checkpointed.
* --dump-vram/frame/audio feed init.c fopen. Resolve these optional output
  paths against data_root too, retaining main-lifetime storage.
* startup-log.c/application-startup-log.c already open absolute diagnostic
  files, including absolute fallback. No path relocation or exit change needed.

No assets, saves, permissions, loader identity or process cwd need changing.
SDK compilation and public package verification cannot establish console play.


// iOS platform layer — path providers for the engine filesystem (Objective 3).
// The engine's Quake-lineage FS derives everything from two roots:
//   fs_basepath  — read-only game data   → app bundle resource path on iOS
//   fs_homepath  — all writes (configs, players/, mods/, screenshots)
//                                        → app-sandbox Documents/ on iOS
// iOS apps cannot write anywhere else; every other path the Win32 code derives
// (Sys_Cwd, GetModuleFileName dir, registry paths) is meaningless in a sandbox.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// <AppBundle>.app/ resource path. Read-only at runtime (bundle is code-signed).
const char *Sys_iOS_BundlePath(void);

// <sandbox>/Documents — persistent, backed up, user-visible in the Files app.
const char *Sys_iOS_DocumentsPath(void);

// <sandbox>/Library/Caches — persistent-ish, purgeable by the OS under pressure.
const char *Sys_iOS_CachesPath(void);

#ifdef __cplusplus
}
#endif

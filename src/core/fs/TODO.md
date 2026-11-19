# Vrutti IDE — src/core/fs/ TODO

**Scope:** `vrutti_ide/src/core/fs/` only. Parent `vrutti/` never touched.

## Purpose
High-performance filesystem operations: path, URI, stream, glob, workspace. Native `std::filesystem` wrapper with caching for frequently-requested configs.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `Path.cpp/.h` | Path manipulation | Present |
| `URI.cpp/.h` | URI handling | Present |
| `Stream.cpp/.h` | Stream abstraction | Present |
| `Glob.cpp/.h` | Glob pattern matching | Present |
| `Workspace.cpp/.h` | Workspace folder topology | Present |

## Tasks (Priority Order)
- [ ] **FS1.** Add file watcher (`ReadDirectoryChangesW` on Windows, `inotify` on POSIX) for external file change detection.
- [ ] **FS2.** Add recursive directory walk with ignore rules (`.gitignore` support).
- [ ] **FS3.** Add file caching layer with TTL for frequently-read configs.
- [ ] **FS4.** Resolve all paths dynamically from executable directory — zero hardcoding.
- [ ] **FS5.** Add atomic write (write to temp, then rename) for settings and workspace files.

## Related
- `src/core/plugins/FileSystemService.cpp` — wraps fs into an IPlugin
- `src/core/plugins/WorkspaceService.cpp` — wraps workspace into an IPlugin
- `src/core/config/TODO.md` — settings file persistence

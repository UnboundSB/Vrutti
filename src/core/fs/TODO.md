# Vrutti IDE — src/core/fs/ TODO

**Scope:** `vrutti_ide/src/core/fs/` only. Parent `vrutti/` never touched.

## Purpose
High-performance filesystem operations: path, URI, stream, glob, workspace. Native `std::filesystem` wrapper with caching for frequently-requested configs.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `Path.cpp/.h` | Path manipulation | Present |
| `URI.cpp/.h` | URI handling, and the conversion to and from a native path | Present |
| `Stream.cpp/.h` | Stream abstraction | Present |
| `Glob.cpp/.h` | Glob pattern matching | Present |
| `Workspace.cpp/.h` | Workspace folder topology | Present |
| `FileService.cpp/.h` | The native file operations behind `workspace.fs` | Working |

## The file service
`FileService` is what the extension host reaches through the
`MainThreadFileSystem` shape. It answers with the error names of the upstream
file system provider -- `FileNotFound`, `FileExists`, `FileNotADirectory`,
`FileIsADirectory`, `NoPermissions`, `Unavailable` -- so an extension that
catches a specific failure still catches it in Vrutti.

A `FileType` is `Unknown` (0), `File` (1), `Directory` (2) or
`SymbolicLink` (64). A `FilePermission` is `Readonly` (1), `Locked` (2) or
`Executable` (4). The times are milliseconds since the Unix epoch.

## Tasks (Priority Order)
- [x] **FS0.** The native file operations of the `workspace.fs` surface:
      stat, read directory, read file, write file, create directory, delete,
      rename, copy. Each answer carries the upstream error name.
- [ ] **FS1.** Add file watcher (`ReadDirectoryChangesW` on Windows, `inotify` on POSIX) for external file change detection.
- [ ] **FS2.** Add recursive directory walk with ignore rules (`.gitignore` support).
- [ ] **FS3.** Add file caching layer with TTL for frequently-read configs.
- [x] **FS4.** Resolve all paths dynamically from executable directory — zero hardcoding.
- [ ] **FS5.** Add atomic write (write to temp, then rename) for settings and workspace files.
- [ ] **FS6.** Keep the file service inside the workspace it was given. A
      path that points outside it must be refused, not followed.

## Related
- `src/core/ipc/VSCodeProtocol.cpp` — the router that answers the calls
- `src/core/plugins/FileSystemService.cpp` — wraps fs into an IPlugin
- `src/core/plugins/WorkspaceService.cpp` — wraps workspace into an IPlugin
- `src/core/config/TODO.md` — settings file persistence
- `src/tests/integration/ExthostFileSystemTest.cpp` — the end to end test

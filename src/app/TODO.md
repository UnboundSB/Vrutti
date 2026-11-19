# Vrutti IDE — src/app/ TODO

**Scope:** `vrutti_ide/src/app/` only. Parent `vrutti/` never touched.

## Purpose
Native C++ application entry point. Bootstraps the IDE: parses args, fixes CWD to install dir, initializes IPC client, wires VSCodeProtocol, spawns the Node.js Extension Host, and launches the WebView2 compositor window.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `main.cpp` | `main()`: CWD fix, IPC client, pipe spawn, window launch | Fixed (Release 1) |
| `structure.md` | Directory docs | Present |
| `vrutti_app.rc` | Windows resources (icon, version) | Present |

## Recent Fixes (Release 1 — already applied)
- [x] Per-instance pipe name: `vrutti_pipe_<PID>` (was hardcoded `vrutti_pipe`)
- [x] Dynamic `node.exe` / `bootstrapper.js` path resolution from executable directory (was hardcoded `src\\ext\\bin\\node.exe`)

## Remaining Tasks (Priority Order)
- [ ] **A1.** Track the spawned Extension Host `PROCESS_INFORMATION` and terminate it cleanly on `vrutti_app.exe` exit (no orphaned Node processes).
- [ ] **A2.** Support multiple workspace folders from `argv[]` (currently only one initial workspace).
- [ ] **A3.** Add portable mode support: if `Vrutti_Portable` env or flag set, use `.vrutti` next to exe instead of `%APPDATA%`.
- [ ] **A4.** Add crash handler / unhandled exception filter to log crashes before exit.
- [ ] **A5.** Validate `pipeName` length and sanitize to prevent named-pipe injection.
- [ ] **A6.** Add `--help` / `--version` CLI flag parsing.
- [ ] **A7.** Ensure `Window` construction failure returns non-zero exit code and logs reason.

## Related
- `docs/EXTENSION_COMPATIBILITY.md` — extension compatibility mandate (READ FIRST)
- `src/core/ipc/TODO.md` — IPC client used by main.cpp
- `src/ui/compositor/TODO.md` — Window class used by main.cpp
- `src/ext/TODO.md` — Extension Host spawned by main.cpp

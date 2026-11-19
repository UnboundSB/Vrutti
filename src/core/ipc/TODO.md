# Vrutti IDE — src/core/ipc/ TODO

**Scope:** `vrutti_ide/src/core/ipc/` only. Parent `vrutti/` never touched.

## Purpose
JSON-RPC 2.0 over named pipes (Windows) / Unix domain sockets. Bridges C++ core and Node.js Extension Host.

## HARD REQUIREMENT: Extension Compatibility
The IPC protocol is the contract between C++ core and Extension Host. Extensions depend on it working predictably. `VSCodeProtocol` must handle all `MainThreadShape` messages, not just 2 stubs.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `IPCClient.cpp/.h` | Pipe client/server + pending-call tracking | Fixed (Release 1) |
| `VSCodeProtocol.cpp/.h` | VS Code protocol router (MainThreadShape / ExtHostShape) | **Stub — 2 methods only** |

## Recent Fixes (Release 1 — already applied, not compiled)
- [x] `nlohmann::json` serialization (no string concatenation)
- [x] Request/response with IDs + `resolvePending()` promise resolver
- [x] Newline-buffer accumulation before parsing
- [x] `sendNotification()` / `sendRequest()` API split

## Tasks (Priority Order — Extension Compatibility First)
- [ ] **I1.** Complete `VSCodeProtocol` router — map ALL `MainThread*` messages:
  - `MainThreadCommands.$registerCommand`, `$executeCommand`
  - `MainThreadDocuments.$trySaveDocument`, `$openDocument`, `$updateDocument`
  - `MainThreadWorkspace.$workspaceFolders`, `$configurationChanged`
  - `MainThreadTerminal.$createTerminal`, `$writeTerminal`, `$setTerminalTitle`
  - `MainThreadOutput.$appendOutput`
  - `MainThreadDebug.$startDebugging`, `$setBreakpoints`
  - `MainThreadExtension.$activateExtension`, `$installExtension`
  - `MainThreadNotification.$showInformationMessage`, `$showErrorMessage`
  - `MainThreadWebview.$createWebviewPanel`, `$setWebviewHtml`
- [ ] **I2.** Mirror all `ExtHostShape` methods that ExtHost sends to C++
- [ ] **I3.** Define per-request timeout (30s) so pending calls don't hang
- [ ] **I4.** Add message sequencing / backpressure for terminal output and document edits
- [ ] **I5.** Add connection auto-reconnect with exponential backoff
- [ ] **I6.** Validate incoming JSON against schema before dispatching
- [ ] **I7.** Log every IPC message in debug mode (method, id, payload size)

## Related
- `src/ext/protocols/TODO.md` — shared protocol catalog
- `src/ext/api/TODO.md` — Node-side API shims
- `docs/EXTENSION_COMPATIBILITY.md` — compatibility mandate

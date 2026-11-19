# Vrutti IDE — src/core/ TODO

**Scope:** `vrutti_ide/src/core/` only. Parent `vrutti/` never touched.

## Purpose
Foundational native C++ framework. Single source of truth for application state (documents, workspace, filesystem, terminal, settings). Composed of lazy-loaded DLL plugins managed by `PluginLoader`.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `concurrency/` | Thread pools, async tasks, cancellation | Real |
| `config/` | Settings persistence (SettingsManager) | Fixed (Release 1) |
| `editor/` | Piece Table document model | Real |
| `events/` | Cross-component event bus | Present |
| `fs/` | Filesystem ops, URI, path, glob, workspace | Partial |
| `ipc/` | JSON-RPC named pipe to Extension Host | Fixed (Release 1) |
| `memory/` | ArenaAllocator, Disposable | Real |
| `plugins/` | ~16 service DLLs + PluginLoader | Mixed (stubs) |
| `terminal/` | PTY (ConPTY/forkpty) | Stub |
| `utils/` | JSON, Base64, LineScanner, StringPool | Real |
| `di/` | Dependency injection container (NEW) | Empty |
| `commands/` | Command registry (NEW) | Empty |
| `configuration/` | Configuration service (NEW) | Empty |
| `storage/` | Memento / storage (NEW) | Empty |
| `theme/` | Theme service (NEW) | Empty |
| `telemetry/` | Telemetry pipeline (NEW) | Empty |
| `notifications/` | Toast/notification service (NEW) | Empty |
| `updates/` | Auto-update service (NEW) | Empty |
| `secrets/` | Secret/credential storage (NEW) | Empty |

## Core-Level Tasks (Priority Order — Extension Compatibility First)

**HARD REQUIREMENT:** If a VS Code extension works in VS Code, it must work in Vrutti. The Extension Host bridge and `vscode` API shim are Tier 0 foundation.

- [ ] **C0.** Complete Release 1 fixes: `SettingsManager` non-blocking save (done), `IPCClient` proper JSON + promise resolver (done), `PluginLoader` mutex (done).
- [ ] **C2.** Build `di/` — lightweight DI container modeled on VS Code's `instantiation` (`createDecorator`, `ServiceCollection`).
- [ ] **C3.** Build `commands/` — command registry with `registerCommand`/`executeCommand`, keybinding integration.
- [ ] **C4.** Build `configuration/` — typed config schema with defaults, change notifications.
- [ ] **C5.** Build `storage/` — memento pattern for per-workspace and global key-value storage.
- [ ] **C6.** Build `theme/` — theme service that reads `.vscode-theme` / color theme JSON and pushes CSS variables to frontend via IPC.
- [ ] **C7.** Build `telemetry/` — lightweight event telemetry (opt-in, no PII).
- [ ] **C8.** Build `notifications/` — notification service for toasts and extension alerts.
- [ ] **C9.** Build `updates/` — auto-update check/download (integrate with `installer/`).
- [ ] **C10.** Build `secrets/` — OS-native secret storage (Windows Credential Manager / libsecret) for extension auth tokens.

## Stub Plugins Needing Real Implementation
- `terminal/` — real PTY, not mock `sleep(500)` reader
- `plugins/DebugAdapterService.cpp` — remove (duplicate of `ext/dap-client.js`)
- `plugins/LanguageFeatureService.cpp` — wire completions/hover to ExtHost RPC
- `plugins/DiagnosticService.cpp` — full IMarkerData parsing
- `plugins/WorkspaceService.cpp` — real folder add/remove/broadcast
- `plugins/FileSystemService.cpp` — return real content, not `"..."`

## Related
- `docs/EXTENSION_COMPATIBILITY.md` — extension compatibility mandate (READ FIRST)
- `src/ext/api/TODO.md` — vscode API shim layer (Tier 1 priority)
- `src/ext/protocols/TODO.md` — shared IPC method catalog
- `src/app/TODO.md` — entry point
- `src/core/plugins/TODO.md` — plugin DLLs
- `src/core/ipc/TODO.md` — IPC protocol
- `src/core/webview/TODO.md` — WebView panel service for extension custom UIs
- `src/ext/TODO.md` — Extension Host (consumes core services via IPC)

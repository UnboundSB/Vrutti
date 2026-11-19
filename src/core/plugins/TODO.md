# Vrutti IDE — src/core/plugins/ TODO

**Scope:** `vrutti_ide/src/core/plugins/` only. Parent `vrutti/` never touched.

## Purpose
Native C++ service DLLs loaded at runtime by `PluginLoader`. Each exposes `createPlugin`/`destroyPlugin` externs and implements `IPlugin` (`initialize`, `shutdown`, `getName`, `executeCommand`).

## Files (16 plugins)
| File | Purpose | Status |
|------|---------|--------|
| `PluginLoader.cpp/.h` | DLL loader + lifecycle | Fixed (mutex added) |
| `IPlugin.h` | Plugin interface | Present |
| `DocumentSyncService.cpp/.h` | Piece Table doc sync, 50ms debounce | Real |
| `FileSystemService.cpp/.h` | stat/read/write/list | Partial (content mocked) |
| `TerminalService.cpp/.h` | PTY broker | Stub |
| `DebugAdapterService.cpp/.h` | DAP broker | **Remove** (duplicate of ext/dap-client.js) |
| `LanguageFeatureService.cpp/.h` | IntelliSense bridge | Stub (empty completions/hover) |
| `DiagnosticService.cpp/.h` | Problems/diagnostics | Partial |
| `WorkspaceService.cpp/.h` | Workspace folder topology | Partial (update no-op) |
| `ExtensionManager.cpp/.h` | Extension list | Partial (list only) |
| `ExtensionLifecycleManager.cpp/.h` | Extension activation | Stub |
| `ExtensionScanner.cpp/.h` | Scan extension dirs, read package.json | Real |
| `VSCodeExtHostService.cpp/.h` | Build IExtensionHostInitData | Real |
| `VSXRegistryService.cpp/.h` | Open VSX search + LRU cache | Real |
| `CommandHandlerService.cpp/.h` | Command registry | Stub |
| `MediaController.cpp/.h` | Background media | No-op |

## Plugin-Level Tasks (Priority Order)
- [ ] **P1.** `FileSystemService`: return actual file content (base64-encoded) instead of `"..."`; add file watcher.
- [ ] **P2.** `TerminalService`: implement real ConPTY (Windows) / forkpty (POSIX); streaming reader thread without mock exit.
- [ ] **P3.** `LanguageFeatureService`: route completion/hover requests to ExtHost via RPC; return real items.
- [ ] **P4.** `DiagnosticService`: fully parse `IMarkerData` (severity, message, range); emit changed events.
- [ ] **P5.** `WorkspaceService`: implement `update_workspace_folders` (add/remove); broadcast topology change.
- [ ] **P6.** `ExtensionManager`: implement install/uninstall/enable/disable lifecycle (backed by `ExtensionScanner` + `VSXRegistryService`).
- [ ] **P7.** `ExtensionLifecycleManager`: implement actual activation via `VSCodeExtHostService` RPC (`ExtHostExtensionServiceShape.$activateByEvent`).
- [ ] **P8.** `CommandHandlerService`: implement `execute_command` routing to ExtHost or native handler.
- [ ] **P9.** `DiagnosticService`: wire to `problems-view` frontend component via IPC notification.
- [ ] **P10.** Delete `DebugAdapterService.cpp`; rely purely on Node `ext/dap-client.js` (resolve split-brain).
- [ ] **P11.** `MediaController`: implement actual background media load/release (not no-op).
- [ ] **P12.** Ensure every plugin's `shutdown()` gracefully joins all threads and closes handles before DLL unload.

## Plugin Load Order (suggested)
1. `PluginLoader` (loader itself)
2. `DocumentSyncService` (text source of truth)
3. `WorkspaceService` (workspace topology)
4. `FileSystemService` (file access)
5. `TerminalService` (PTY)
6. `SettingsManager` (config) — via `config/`, not a plugin
7. `LanguageFeatureService` (IntelliSense)
8. `DiagnosticService` (problems)
9. `CommandHandlerService` (commands)
10. `ExtensionManager` / `ExtensionLifecycleManager` (extensions)
11. `VSCodeExtHostService` (ExtHost bridge)
12. `VSXRegistryService` (marketplace)

## Related
- `src/core/TODO.md` — engine
- `src/core/ipc/TODO.md` — IPC protocol plugins use
- `src/ext/TODO.md` — Extension Host (plugins talk to it via IPC)

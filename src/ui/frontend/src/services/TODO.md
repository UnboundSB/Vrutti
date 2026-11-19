# Vrutti IDE — src/ui/frontend/src/services/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/src/services/` only. Parent `vrutti/` never touched.

## Purpose
Frontend service layer: typed IPC clients, theme service, settings service. Abstracts the raw WebView2 IPC into clean service APIs for components.

## Why This Exists
Components currently call `window.sendIpcMessage` and `window.vrutti*` directly. This directory centralizes IPC into typed services, making the frontend easier to maintain and test.

## Planned Files
| File | Purpose |
|------|---------|
| `ipc-client.ts` | Typed IPC request/response |
| `theme-service.ts` | Theme application |
| `settings-service.ts` | Settings via IPC |
| `command-service.ts` | Command execution via IPC |
| `workspace-service.ts` | Workspace queries via IPC |

## Tasks (Priority Order)
- [ ] **SV1.** Implement `ipc-client.ts` — `sendRequest(method, params)` returns Promise; `sendNotification(method, params)` fire-and-forget.
- [ ] **SV2.** Implement `theme-service.ts` — `setTheme(name)` sends IPC, applies CSS variables.
- [ ] **SV3.** Implement `settings-service.ts` — `get(key)`, `set(key, value)` via IPC.
- [ ] **SV4.** Implement `command-service.ts` — `execute(id, args)` via IPC.
- [ ] **SV5.** Implement `workspace-service.ts` — `getFolders()`, `addFolder(path)` via IPC.
- [ ] **SV6.** All services handle connection loss gracefully (reconnect, queue pending requests).

## Related
- `src/ui/frontend/src/core/Registry.ts` — services register contributions
- `src/core/ipc/TODO.md` — C++ IPC protocol
- `src/ext/protocols/TODO.md` — shared protocol definitions

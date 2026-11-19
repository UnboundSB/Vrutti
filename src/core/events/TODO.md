# Vrutti IDE — src/core/events/ TODO

**Scope:** `vrutti_ide/src/core/events/` only. Parent `vrutti/` never touched.

## Purpose
Cross-component event bus for the C++ core. Allows plugins and services to communicate without direct dependencies.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `Event.h` | Event type definition | Present |
| `Observable.h` | Observable pattern | Present |
| `structure.md` | Directory docs | Present |

## Tasks (Priority Order)
- [ ] **EV1.** Implement a central `EventEmitter` / `EventBus` singleton for core-wide events.
- [ ] **EV2.** Define core event types: `workspaceChanged`, `documentChanged`, `settingChanged`, `extensionInstalled`, `terminalData`, `diagnosticChanged`.
- [ ] **EV3.** Bridge core events to IPC notifications (frontend consumption).
- [ ] **EV4.** Add weak-reference listener management to prevent memory leaks.

## Related
- `src/core/ipc/TODO.md` — IPC notification transport
- `src/ui/frontend/src/core/Registry.ts` — frontend contribution registry (mirrors this)

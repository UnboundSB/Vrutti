# Vrutti IDE — src/ui/compositor/ TODO

**Scope:** `vrutti_ide/src/ui/compositor/` only. Parent `vrutti/` never touched.

## Purpose
Native C++ WebView2 compositor window. Hosts the Lit frontend, provides IPC bindings, window management (min/max/close, drag).

## Files
| File | Purpose | Status |
|------|---------|--------|
| `Window.cpp/.h` | WebView2 window + IPC bindings | Security flaws present |
| `structure.md` | Directory docs | Present |

## Critical Security Issues (from problem.md)
- [ ] **WIN1.** Remove `_popen` shell execution in extension installer / git / run handlers. Use `CreateProcess` with arg arrays.
- [ ] **WIN2.** Remove `eval(...)` in IPC handlers. Escape all Webview parameters via JSON serialization.
- [ ] **WIN3.** Remove direct extension DOM injection (`style.innerHTML`, `script.innerHTML`). Use sandboxed iframe + postMessage.

## Remaining Tasks (Priority Order)
- [ ] **WIN4.** Fix hardcoded IPC named pipe → per-instance pipe (done in `main.cpp`, verify Window uses same).
- [ ] **WIN5.** Add `CancelIoEx` support for non-blocking pipe reads (shutdown responsiveness).
- [ ] **WIN6.** Implement window state persistence (size, position, maximized).
- [ ] **WIN7.** Add multi-window support (`vrutti.action.newWindow`).
- [ ] **WIN8.** Implement frameless window drag / snap / RDP display tracking.

## Related
- `src/app/main.cpp` — creates Window
- `src/core/ipc/TODO.md` — IPC bindings
- `src/ui/frontend/TODO.md` — frontend hosted in this window

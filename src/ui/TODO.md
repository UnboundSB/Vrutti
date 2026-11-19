# Vrutti IDE — src/ui/ TODO

**Scope:** `vrutti_ide/src/ui/` only. Parent `vrutti/` never touched.

## Purpose
UI layer: native WebView2 compositor (`compositor/`) + Lit/Vite frontend (`frontend/`) + vendor libraries (`vendor/`).

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `compositor/` | Native C++ WebView2 window (Window.cpp) | Mixed (security flaws) |
| `frontend/` | Lit + Vite TypeScript app | Mixed |
| `vendor/` | Third-party (ImGui, webview.h) | Present |

## ui-Level Tasks (Priority Order)
- [ ] **UI1.** Fix security flaws in `compositor/Window.cpp`: no `_popen`, no `eval`, no direct extension DOM injection.
- [ ] **UI2.** Wire frontend to real C++ backend IPC (not mock/localStorage).
- [ ] **UI3.** Implement split editor groups (workbench parts model).
- [ ] **UI4.** Implement save-as dialog (currently `window.alert`).
- [ ] **UI5.** Implement minimap, breadcrumbs, diff editor.

## Related
- `src/ui/compositor/TODO.md` — native window
- `src/ui/frontend/TODO.md` — Lit frontend
- `src/ui/vendor/TODO.md` — vendor libraries

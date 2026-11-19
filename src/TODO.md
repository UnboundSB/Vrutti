# Vrutti IDE — src/ TODO

**Scope:** `vrutti_ide/src/` only. Parent `vrutti/` never touched.

## Purpose
All source code for Vrutti IDE: native C++ core, Node.js Extension Host, and Lit/Vite frontend.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `app/` | Native C++ entry (`main.cpp`, window launch, pipe spawn) | Partial |
| `core/` | Native C++ engine (services, IPC, editor, FS, etc.) | Stubs/mixed |
| `ext/` | Node.js Extension Host bridge (bootstrapper, IPC, LSP, DAP) | Partial |
| `plugins/` | Native search plugin (DLL) | Partial |
| `tests/` | Integration tests | Empty/stub |
| `ui/` | Compositor (Window.cpp) + Lit frontend + vendor (ImGui) | Mixed |

## src-Level Tasks (Priority Order)
- [ ] **S1.** Verify every subdirectory has its own `TODO.md` with scoped tasks.
- [ ] **S2.** Confirm CMake build produces all plugin DLLs into `build/` alongside `vrutti_app.exe`.
- [ ] **S3.** Confirm frontend `npm run build` produces `dist/` from `src/ui/frontend/`.
- [ ] **S4.** Ensure no hardcoded `d:/vrutti/` paths remain anywhere in `src/`.
- [ ] **S5.** Ensure no `_popen` or `eval` remains in `src/` (security).
- [ ] **S6.** Ensure every `sendIpcMessage` await in frontend has a matching resolver in C++ (no leaked promises).

## Related
- `src/app/TODO.md` — entry point
- `src/core/TODO.md` — C++ engine
- `src/ext/TODO.md` — extension host bridge
- `src/ui/TODO.md` — UI layer

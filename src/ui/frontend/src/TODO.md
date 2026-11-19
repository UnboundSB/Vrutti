# Vrutti IDE — src/ui/frontend/src/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/src/` only. Parent `vrutti/` never touched.

## Purpose
Frontend source: Lit components, core registry, theme bridge, DOM bridge, and main app.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `components/` | Lit web components (editor, sidebar, terminal, etc.) | Mixed |
| `core/` | Registry, core-contributions | Present |
| `main.ts` | VruttiApp Lit element + entry | Present |
| `dom-bridge.ts` | DOM proxy for extension compatibility | Present |
| `ThemeBridge.ts` | Theme application bridge | Present |
| `shared-styles.ts` | Global CSS variables | Present |
| `globalHover.ts` | Hover styles | Present |

## Tasks (Priority Order)
- [ ] **FS1.** Audit every component for `unsafeHTML` / `innerHTML` — sanitize or replace.
- [ ] **FS2.** Verify all event listeners use arrow functions (no `this` leaks).
- [ ] **FS3.** Wire `vrutti-editor.ts` to CodeMirror 6 + C++ Piece Table sync.
- [ ] **FS4.** Wire `vrutti-terminal.ts` to C++ `TerminalService` PTY stream.
- [ ] **FS5.** Wire `vrutti-search.ts` to native search plugin.
- [ ] **FS6.** Wire `vrutti-problems-view.ts` to `DiagnosticService` IPC events.
- [ ] **FS7.** Implement `vrutti-settings.ts` with real IPC settings (not localStorage).
- [ ] **FS8.** Implement split editor layout in `vrutti-editor-layout.ts`.

## Related
- `src/ui/frontend/src/components/TODO.md` — component details
- `src/ui/frontend/src/core/TODO.md` — registry details
- `src/core/ipc/TODO.md` — IPC methods frontend consumes

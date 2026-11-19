# Vrutti IDE — src/ui/frontend/src/components/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/src/components/` only. Parent `vrutti/` never touched.

## Purpose
Lit web components: the actual UI atoms and molecules of the IDE.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| Root components | Editor, sidebar, terminal, search, scm, etc. | Present |
| `explorer/` | File explorer component | Present |
| `scm/` | Source control / git graph | Present |
| `layout/` | Layout parts (NEW) | Empty |
| `workbench/` | Workbench shell compositions (NEW) | Empty |

## Component Tasks (Priority Order)
- [ ] **CMP1.** `vrutti-editor.ts` — CodeMirror 6 + C++ Piece Table sync.
- [ ] **CMP2.** `vrutti-terminal.ts` — xterm.js + C++ PTY stream.
- [ ] **CMP3.** `vrutti-search.ts` — search UI + native plugin IPC.
- [ ] **CMP4.** `vrutti-problems-view.ts` — diagnostics from IPC.
- [ ] **CMP5.** `vrutti-settings.ts` — settings via IPC (not localStorage).
- [ ] **CMP6.** `vrutti-extensions.ts` — extension marketplace UI.
- [ ] **CMP7.** `vrutti-scm.ts` — sanitize `unsafeHTML` (XSS fix).
- [ ] **CMP8.** All components — arrow function event listeners (no `this` leaks).
- [ ] **CMP9.** All components — no direct `innerHTML`/`script.innerHTML` (sandbox extensions).

## Layout Parts (NEW — model after VS Code workbench/parts/)
- [ ] **LAY1.** `layout/activitybar.ts` — activity bar with view containers.
- [ ] **LAY2.** `layout/sidebar.ts` — sidebar with view containers.
- [ ] **LAY3.** `layout/panel.ts` — bottom panel (terminal, output, problems).
- [ ] **LAY4.** `layout/editor-group.ts` — split editor groups with drag.
- [ ] **LAY5.** `layout/statusbar.ts` — status bar items.
- [ ] **LAY6.** `layout/titlebar.ts` — custom title bar.

## Workbench Shell (NEW — model after VS Code workbench/)
- [ ] **WB1.** `workbench/workbench-shell.ts` — composes activitybar, sidebar, panel, editor, statusbar.
- [ ] **WB2.** `workbench/part-registry.ts` — register/unregister parts dynamically.
- [ ] **WB3.** `workbench/layout-service.ts` — layout persistence and restoration.

## Related
- `src/ui/frontend/src/core/Registry.ts` — contribution registry components use
- `src/ui/frontend/src/components/layout/TODO.md` — layout parts
- `src/ui/frontend/src/components/workbench/TODO.md` — workbench shell

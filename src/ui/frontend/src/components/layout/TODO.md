# Vrutti IDE — src/ui/frontend/src/components/layout/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/src/components/layout/` only. Parent `vrutti/` never touched.

## Purpose
Layout parts modeled after VS Code `workbench/browser/parts/`: activitybar, sidebar, panel, editor groups, statusbar, titlebar. These are the structural shells that hold views.

## Why This Exists
VS Code's workbench uses composite parts (`paneCompositePart`, `compositePart`) for layout. Vrutti's frontend currently has a fixed layout (single editor + bottom terminal). This directory enables split editors and dynamic layouts.

## Planned Files
| File | Purpose |
|------|---------|
| `activitybar.ts` | Activity bar container |
| `sidebar.ts` | Sidebar with view containers |
| `panel.ts` | Bottom panel (terminal, output, problems) |
| `editor-group.ts` | Split editor group with drag-to-rearrange |
| `statusbar.ts` | Status bar with items |
| `titlebar.ts` | Custom title bar |

## Tasks (Priority Order)
- [ ] **LAY1.** Implement `activitybar.ts` — click to switch sidebar views.
- [ ] **LAY2.** Implement `sidebar.ts` — collapsible view container.
- [ ] **LAY3.** Implement `panel.ts` — bottom panel with tabs.
- [ ] **LAY4.** Implement `editor-group.ts` — split editors (vertical/horizontal) with drag handles.
- [ ] **LAY5.** Implement `statusbar.ts` — status bar items (line/col, encoding, etc.).
- [ ] **LAY6.** Implement `titlebar.ts` — window controls + app title.
- [ ] **LAY7.** Compose all parts in `vrutti-editor-layout.ts` or new workbench shell.

## Related
- `src/ui/frontend/src/components/workbench/TODO.md` — workbench shell
- `src/ui/frontend/src/core/Registry.ts` — parts register contributions here
- `architecture.md` — VS Code workbench parts comparison

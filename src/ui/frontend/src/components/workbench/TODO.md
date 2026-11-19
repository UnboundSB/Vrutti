# Vrutti IDE — src/ui/frontend/src/components/workbench/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/src/components/workbench/` only. Parent `vrutti/` never touched.

## Purpose
Workbench shell composition: assembles activitybar, sidebar, panel, editor groups, statusbar, titlebar into the complete IDE shell. Modeled after VS Code `workbench.desktop.main.ts`.

## Why This Exists
Vrutti's frontend needs a formal workbench shell with part lifecycle, not just ad-hoc component composition in `main.ts`.

## Planned Files
| File | Purpose |
|------|---------|
| `workbench-shell.ts` | Main workbench composition |
| `part-registry.ts` | Register/unregister UI parts |
| `layout-service.ts` | Layout persistence/restoration |
| `part-service.ts` | Active part management |

## Tasks (Priority Order)
- [ ] **WB1.** Implement `workbench-shell.ts` — root component composing all parts.
- [ ] **WB2.** Implement `part-registry.ts` — parts register themselves (like VS Code `WorkbenchContribution`).
- [ ] **WB3.** Implement `layout-service.ts` — save/restore layout (split sizes, visible panels).
- [ ] **WB4.** Implement zen mode / center layout (optional).
- [ ] **WB5.** Implement welcome page (optional).

## Related
- `src/ui/frontend/src/components/layout/TODO.md` — individual parts
- `src/ui/frontend/src/main.ts` — current VruttiApp (will migrate to workbench shell)
- `architecture.md` — VS Code workbench comparison

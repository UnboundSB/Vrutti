# Vrutti IDE — src/ui/frontend/src/core/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/src/core/` only. Parent `vrutti/` never touched.

## Purpose
Frontend core: contribution registry, core contributions, and service layer. The `Registry.ts` is Vrutti's hand-rolled contribution system (modeled loosely on VS Code contributions).

## Files
| File | Purpose | Status |
|------|---------|--------|
| `Registry.ts` | Contribution registry (activitybar, views, panels, menus, commands, etc.) | Present |
| `core-contributions.ts` | Built-in contribution registrations | Present |
| `structure.md` | Directory docs | Present |

## Tasks (Priority Order)
- [ ] **REG1.** Audit `Registry.ts` for all contribution types: activitybar, views, panel, statusbar, menus, commands, keybindings, configurations, editors, quickpick.
- [ ] **REG2.** Add `when` clause / contextkey support for conditional contributions.
- [ ] **REG3.** Wire `Registry.ts` to C++ `CommandHandlerService` via IPC for command execution.
- [ ] **REG4.** Add keybinding resolution (parse key chords, match to commands).
- [ ] **REG5.** Add `ConfigurationSchema` contributions (settings schema from C++).
- [ ] **REG6.** Add `EditorProvider` contributions (file extension → editor component).

## Services Layer (NEW)
| File | Purpose |
|------|---------|
| `services/ipc-client.ts` | Frontend IPC client (send/receive) |
| `services/theme-service.ts` | Theme application |
| `services/settings-service.ts` | Settings via IPC |

- [ ] **SRV1.** Build `services/ipc-client.ts` — typed IPC request/response with promises.
- [ ] **SRV2.** Build `services/theme-service.ts` — apply CSS variables from C++ themes.
- [ ] **SRV3.** Build `services/settings-service.ts` — get/set settings via IPC.

## Related
- `src/ui/frontend/src/main.ts` — uses Registry
- `src/ui/frontend/src/components/layout/TODO.md` — layout parts register here
- `src/core/commands/TODO.md` — backend command registry

# Vrutti IDE — src/core/commands/ TODO

**Scope:** `vrutti_ide/src/core/commands/` only. Parent `vrutti/` never touched.

## Purpose
Command registry for the C++ core. Commands are named actions (e.g., `workbench.action.save`, `workbench.action.undo`) that can be triggered from the frontend, keybindings, or extensions.

## Why This Exists
VS Code has a `commands` platform service. Vrutti currently has `CommandHandlerService` as a stub plugin. This directory formalizes command registration, keybinding resolution, and command execution.

## Planned Files
| File | Purpose |
|------|---------|
| `CommandRegistry.h/.cpp` | Register/execute commands |
| `Command.h` | Command metadata (id, handler, precondition) |
| `Keybinding.h` | Keybinding → command mapping |

## Tasks (Priority Order)
- [ ] **CMD1.** Implement `CommandRegistry` with `registerCommand(id, handler)` and `executeCommand(id, args)`.
- [ ] **CMD2.** Add `when` clause / precondition support for command visibility.
- [ ] **CMD3.** Wire frontend `Registry.ts` keybindings to C++ command registry via IPC.
- [ ] **CMD4.** Implement `CommandHandlerService` plugin using this registry (replace stub).
- [ ] **CMD5.** Define default command set: file (save, save-as, open, close), edit (undo, redo, cut, copy, paste), window (new, close), help.

## Related
- `src/core/plugins/CommandHandlerService.cpp` — plugin wrapper
- `src/ui/frontend/src/core/Registry.ts` — frontend command/keybinding registry
- `src/core/keymap/` — (future) keybinding resolution

# Vrutti IDE — src/tests/ TODO

**Scope:** `vrutti_ide/src/tests/` only. Parent `vrutti/` never touched.

## Purpose
Integration tests for Vrutti IDE: end-to-end flows, plugin unit tests, IPC message validation.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `integration/` | End-to-end integration tests | Empty |

## Tasks (Priority Order)
- [ ] **TST1.** Set up test runner (Catch2 for C++, vitest/jest for frontend).
- [ ] **TST2.** Unit test `PieceTable` (insert, remove, undo).
- [ ] **TST3.** Unit test `SettingsManager` (load, update, concurrent save).
- [ ] **TST4.** Unit test `IPCClient` (request/response round-trip).
- [ ] **TST5.** Unit test `PluginLoader` (load/unload DLL, thread safety).
- [ ] **TST6.** Integration test: launch app, open file, edit, save, close.
- [ ] **TST7.** Integration test: spawn terminal, write command, read output.
- [ ] **TST8.** Integration test: install extension from Open VSX, verify activation.

## Related
- `src/tests/integration/TODO.md` — integration test details
- `src/core/plugins/TODO.md` — plugin unit tests
- `src/core/ipc/TODO.md` — IPC round-trip tests

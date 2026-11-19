# Vrutti IDE — src/core/storage/ TODO

**Scope:** `vrutti_ide/src/core/storage/` only. Parent `vrutti/` never touched.

## Purpose
Memento / key-value storage service: per-workspace and global persistent storage. VS Code has `memento` and `storage` platform services.

## Planned Files
| File | Purpose |
|------|---------|
| `Memento.h/.cpp` | Key-value storage with TTL |
| `StorageService.h/.cpp` | Global + workspace storage scopes |

## Tasks (Priority Order)
- [ ] **ST1.** Implement `Memento` with `get(key)` / `set(key, value)` / `delete(key)`.
- [ ] **ST2.** Implement `StorageService` with global scope and workspace scope.
- [ ] **ST3.** Persist memento to disk (JSON file in `.vrutti/storage/`).
- [ ] **ST4.** Expose via IPC: `memento/get`, `memento/set`.
- [ ] **ST5.** Add extension-specific storage isolation (each extension gets its own key namespace).

## Related
- `src/core/config/TODO.md` — settings vs memento distinction
- `src/ext/protocols/TODO.md` — IPC memento methods

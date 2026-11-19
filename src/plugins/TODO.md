# Vrutti IDE — src/plugins/ TODO

**Scope:** `vrutti_ide/src/plugins/` only. Parent `vrutti/` never touched.

## Purpose
Independent high-performance native plugins compiled as dynamic libraries (DLLs) loaded at runtime by `PluginLoader`. These are distinct from `src/core/plugins/` — they are standalone feature plugins, not core services.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `search/` | Native codebase search (regex + plain text) | Present |

## Plugin-Level Tasks (Priority Order)
- [ ] **PLG1.** Wire `search/` plugin to frontend `vrutti-search.ts` via IPC (`search/findInFiles`).
- [ ] **PLG2.** Integrate ripgrep binary (`bin/rg.exe`) as the search backend.
- [ ] **PLG3.** Add search result highlighting and file navigation.
- [ ] **PLG4.** Add replace-in-files support.
- [ ] **PLG5.** Consider additional native plugins: diff engine, git operations, language parsing.

## Related
- `src/core/plugins/PluginLoader.cpp` — loads this DLL
- `src/core/ipc/TODO.md` — IPC search methods
- `src/ui/frontend/src/components/vrutti-search.ts` — frontend search UI

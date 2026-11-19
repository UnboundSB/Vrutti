# Vrutti IDE — src/plugins/search/ TODO

**Scope:** `vrutti_ide/src/plugins/search/` only. Parent `vrutti/` never touched.

## Purpose
Native C++ search plugin: ultra-fast regex and plain-text codebase scanning without external dependencies (uses ripgrep binary).

## Files
| File | Purpose | Status |
|------|---------|--------|
| `SearchPlugin.cpp/.h` | Search plugin implementation | Present |
| `structure.md` | Directory docs | Present |

## Tasks (Priority Order)
- [ ] **SRC1.** Implement `SearchPlugin::executeCommand("search", payload)` with query, path, regex flag.
- [ ] **SRC2.** Call `bin/rg.exe` (ripgrep) for actual scanning; parse results.
- [ ] **SRC3.** Return results as JSON array (file, line, column, match text).
- [ ] **SRC4.** Add search cancellation (CancellationToken).
- [ ] **SRC5.** Add search result pagination for large codebases.

## Related
- `bin/TODO.md` — ripgrep binary
- `src/ui/frontend/src/components/vrutti-search.ts` — frontend consumer
- `src/core/concurrency/TODO.md` — cancellation support

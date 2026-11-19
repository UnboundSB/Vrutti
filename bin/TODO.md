# Vrutti IDE — bin/ TODO

**Scope:** `vrutti_ide/bin/` only. Parent `vrutti/` never touched.

## Purpose
Native tools and binaries: ripgrep (`rg.exe`) for code search, and other native utilities used by plugins.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `rg.exe` | Ripgrep binary | Present |

## Tasks (Priority Order)
- [ ] **BIN1.** Document ripgrep version and license (BSD-style).
- [ ] **BIN2.** Integrate `rg.exe` with `src/plugins/search/` for codebase search.
- [ ] **BIN3.** Include `rg.exe` in release builds (portable ZIP + installer).
- [ ] **BIN4.** Add version check: verify ripgrep binary works on launch.

## Related
- `src/plugins/search/TODO.md` — search plugin
- `src/core/fs/TODO.md` — filesystem operations
- `installer/TODO.md` — release packaging

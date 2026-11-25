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
- [x] **SRC1.** Implement `SearchPlugin::executeCommand("search", payload)` with query, path, regex flag.
- [x] **SRC2.** Call `bin/rg.exe` (ripgrep) for actual scanning; parse results.
- [x] **SRC3.** Return results as JSON (file, line, match text).
- [ ] **SRC4.** Add search cancellation (CancellationToken).
- [ ] **SRC5.** Add search result pagination for large codebases.

## Hardening
- [x] The search program is started **without a shell**: every name of a file
      or of a folder is an argument of the program, never a part of a
      command. A folder whose name holds a quote or a semicolon cannot run
      a command of its own.
- [x] The pattern is written into a file of its own that is made with the
      flag that says that it may not exist, and that belongs to the user
      alone.
- [x] The text of the program, the number of the matches and the number of
      the files have a limit. A file that is too big is left alone by a
      replace.
- [x] A pattern that the engine of the regular expressions refuses is
      caught: the file is left as it was and the plugin keeps answering.
- [x] A replace writes the new text into a file of its own and that file
      takes the place of the old one, so a file that is half written cannot
      be left behind.
- [x] The program is stopped when it takes too long.

The proof is `src/tests/integration/SearchPluginTest.cpp`.

## Related
- `bin/TODO.md` — ripgrep binary
- `src/ui/frontend/src/components/vrutti-search.ts` — frontend consumer
- `src/core/concurrency/TODO.md` — cancellation support

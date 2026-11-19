# Vrutti IDE — src/ui/frontend/src/components/explorer/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/src/components/explorer/` only. Parent `vrutti/` never touched.

## Purpose
File explorer component: tree view of workspace folders, file operations (open, rename, delete, new file).

## Files
| File | Purpose | Status |
|------|---------|--------|
| `vrutti-explorer.ts` | Explorer component | Present |
| `explorerModel.ts` | Explorer data model | Present |
| `iconMapper.ts` | File icon mapping | Present |

## Tasks (Priority Order)
- [ ] **EXP1.** Wire to `FileSystemService` IPC (`read_directory`, `stat`) for real file listing.
- [ ] **EXP2.** Implement file open → open in editor.
- [ ] **EXP3.** Implement new file / new folder / delete / rename.
- [ ] **EXP4.** Implement file watcher refresh (external changes).
- [ ] **EXP5.** Implement drag-and-drop file reordering.
- [ ] **EXP6.** Integrate with `iconMapper.ts` for file-type icons.

## Related
- `src/core/plugins/FileSystemService.cpp` — filesystem backend
- `src/core/fs/TODO.md` — fs operations
- `src/ui/frontend/src/components/vrutti-tree-view.ts` — tree view component

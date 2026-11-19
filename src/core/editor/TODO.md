# Vrutti IDE — src/core/editor/ TODO

**Scope:** `vrutti_ide/src/core/editor/` only. Parent `vrutti/` never touched.

## Purpose
Backend text document model. Uses a memory-efficient Piece Table data structure. Single source of truth for text documents; syncs to frontend CodeMirror and Extension Host via IPC.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `PieceTable.cpp/.h` | Piece Table document model | Real |
| `structure.md` | Directory docs | Present |

## Remaining Tasks (Priority Order)
- [ ] **ED1.** Add undo/redo stack to Piece Table.
- [ ] **ED2.** Add change tracking (dirty flag, version ID) — partially present.
- [ ] **ED3.** Add line/column ↔ offset conversion utilities.
- [ ] **ED4.** Add multi-cursor selection tracking (backend-side ranges).
- [ ] **ED5.** Add find/search within document (offset-based).
- [ ] **ED6.** Debounce document sync to frontend at 50ms (partially present in DocumentSyncService).
- [ ] **ED7.** Expose document content via IPC `document/get` for file read operations.

## Related
- `src/core/plugins/DocumentSyncService.cpp` — IPC sync wrapper
- `src/ui/frontend/src/components/vrutti-editor.ts` — CodeMirror frontend consumer
- `src/core/fs/TODO.md` — file read/write that populates Piece Table

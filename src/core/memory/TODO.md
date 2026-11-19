# Vrutti IDE — src/core/memory/ TODO

**Scope:** `vrutti_ide/src/core/memory/` only. Parent `vrutti/` never touched.

## Purpose
Memory management utilities: custom allocators (ArenaAllocator) and Disposable pattern for deterministic resource cleanup.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `ArenaAllocator.cpp/.h` | Arena allocator | Real |
| `Disposable.h` | Disposable interface | Present |
| `structure.md` | Directory docs | Present |

## Tasks (Priority Order)
- [ ] **MEM1.** Add memory pool for frequently-allocated small objects (JSON nodes, IPC messages).
- [ ] **MEM2.** Add memory usage tracking / reporting (for low-RAM target).
- [ ] **MEM3.** Verify all `Disposable` implementations are called in plugin `shutdown()`.
- [ ] **MEM4.** Add arena scoped allocators for per-request IPC message parsing (auto-frees after request).

## Related
- `src/core/plugins/TODO.md` — plugins must dispose resources in `shutdown()`
- `src/core/ipc/TODO.md` — IPC message allocation

# Vrutti IDE — src/core/concurrency/ TODO

**Scope:** `vrutti_ide/src/core/concurrency/` only. Parent `vrutti/` never touched.

## Purpose
Thread pools, async tasks, and cancellation primitives for the C++ core. Keeps blocking I/O off the main/UI thread.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `ThreadPool.cpp/.h` | Thread pool implementation | Real |
| `Async.h` | Async helpers | Present |
| `CancellationToken.h` | Cancellation token | Present |

## Tasks (Priority Order)
- [ ] **CON1.** Verify `ThreadPool` gracefully shuts down (joins all worker threads) on app exit.
- [ ] **CON2.** Add task prioritization (high-priority UI tasks vs background indexing).
- [ ] **CON3.** Add `parallel_for` / `parallel_map` helpers for search/indexing workloads.
- [ ] **CON4.** Integrate `CancellationToken` with long-running operations (file watcher, search, extension install).
- [ ] **CON5.** Add thread-affinity assertions: UI thread vs worker thread boundaries.

## Related
- `src/core/terminal/TODO.md` — terminal reader threads
- `src/core/plugins/TODO.md` — plugins that spawn background threads

# Core Foundation - Progress Tracker

This document tracks the detailed implementation progress of the `src/core` foundational modules.
This ensures context is preserved across sessions and instances.

## Phase 1: Memory (Completed)
- [x] Create `ArenaAllocator` for zero-copy, fast node allocation.
- [x] Test `ArenaAllocator` in main.

## Phase 2: Concurrency & Events (Completed)
- [x] Implement `ThreadPool` for async task execution.
- [x] Implement VS Code's `Event` and `Emitter` pub/sub mechanism.

## Phase 3: Editor Core (Completed)
- [x] Implement `PieceTable` for high-performance text buffer manipulations.
- [ ] Support large file loading into original buffer segment.

## Phase 4: File System (Pending)
- [x] Create `URI` abstractions.
- [ ] Implement fast asynchronous file system operations.
- [ ] Setup file watcher abstractions for live reloading.

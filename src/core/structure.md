# Core Modules Architecture & Porting Strategy

This document outlines the high-level architecture of `vrutti_ide/src/core` and tracks how we are mapping VS Code's TypeScript architecture (`vs/base/common/*`) into native, high-performance C++ concepts.

## 1. Memory Management (`core/memory/`)

*   **`Disposable.h` (Mapped from `lifecycle.ts`)**
    *   **Concept**: Replaces garbage collection reliance by strictly managing resource lifecycles using RAII and manual dispose boundaries.
    *   **Components**: `IDisposable` interface and `DisposableStore` to track and clean up subscriptions, memory allocations, and file handles deterministically.
*   **`ArenaAllocator.h`**
    *   **Concept**: Replaces V8's heap allocations for bulk node creation (e.g., ASTs, UI Widgets). Provides contiguous, cache-friendly memory blocks that can be wiped out in `O(1)` time.

## 2. Concurrency (`core/concurrency/`)

*   **`ThreadPool.h / .cpp`**
    *   **Concept**: Mapped from standard `Promise` / async execution models in TS. 
    *   **Architecture**: Offloads heavy tasks (file indexing, syntax highlighting) from the main thread (UI thread) to hardware worker threads using a lock-based task queue and `std::future`.

## 3. File System (`core/fs/`) - *Planned*

*   **URI Abstractions (Mapping `uri.ts`)**
    *   **Goal**: Create a lightweight, zero-copy `URI` parser for handling `file://`, `vscode://`, and remote schemes efficiently.

## 4. Events (`core/events/`) - *Planned*

*   **Event Emitter (Mapping `event.ts`)**
    *   **Goal**: Create a lightweight pub/sub mechanism (`Event<T>`, `Emitter<T>`) that is thread-safe and integrates directly with our `IDisposable` model for deterministic unsubscription.

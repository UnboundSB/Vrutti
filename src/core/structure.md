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

## 3. File System (`core/fs/`)

*   **`URI.h / .cpp` (Mapped from `uri.ts`)**
    *   **Concept**: A memory-optimized `URI` parser for handling `file://`, `vscode://`, and remote schemes. 
    *   **Architecture**: Radically reduces the RAM footprint compared to standard parsing by maintaining a single raw string and lazily evaluating boundaries using `uint16_t` offset caches and zero-allocation `std::string_view` returns.

## 4. Events (`core/events/`)

*   **`Event.h` (Mapped from `event.ts`)**
    *   **Concept**: Creates a lightweight pub/sub mechanism (`Event<T>`, `Emitter<T>`) that is entirely thread-safe using `std::mutex`.
    *   **Architecture**: Subscribing to an event returns an `IDisposable`. This perfectly mirrors the VS Code architecture, guaranteeing deterministic cleanup of event listeners and avoiding memory leaks commonly found in loosely coupled systems.

## 5. Editor Core (`core/editor/`)

*   **`PieceTable.h / .cpp`**
    *   **Concept**: Maps to VS Code's `pieceTreeTextBuffer`. Replaces basic strings/arrays for text file storage.
    *   **Architecture**: Uses an append-only buffer for new typing and an original buffer for the loaded file. Editing doesn't move large chunks of memory, it just splices pointers. The API exposes traditional standard string outputs (`getText`, `insert`, `remove`) so outer modules don't have to worry about the complex internal buffer mapping.

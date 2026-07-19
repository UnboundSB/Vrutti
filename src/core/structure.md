# Core Modules Architecture & Porting Strategy

This document outlines the high-level architecture of `vrutti_ide/src/core` and tracks how we are mapping the core architecture into native, high-performance C++ concepts.

## 1. Memory Management (`core/memory/`)

*   **`Disposable.h`**
    *   **Concept**: Replaces garbage collection reliance by strictly managing resource lifecycles using RAII and manual dispose boundaries.
    *   **Components**: `IDisposable` interface and `DisposableStore` to track and clean up subscriptions, memory allocations, and file handles deterministically.
*   **`ArenaAllocator.h`**
    *   **Concept**: Replaces V8's heap allocations for bulk node creation (e.g., ASTs, UI Widgets). Provides contiguous, cache-friendly memory blocks that can be wiped out in `O(1)` time.

## 2. Concurrency (`core/concurrency/`)

*   **`ThreadPool.h / .cpp`**
    *   **Concept**: Mapped from standard `Promise` / async execution models in TS. 
    *   **Architecture**: Offloads heavy tasks (file indexing, syntax highlighting) from the main thread (UI thread) to hardware worker threads using a lock-based task queue and `std::future`.

## 3. File System (`core/fs/`)

*   **`URI.h / .cpp`**
    *   **Concept**: A memory-optimized `URI` parser for handling `file://`, `vrutti://`, and remote schemes. 
    *   **Architecture**: Radically reduces the RAM footprint compared to standard parsing by maintaining a single raw string and lazily evaluating boundaries using `uint16_t` offset caches and zero-allocation `std::string_view` returns.

## 4. Events (`core/events/`)

*   **`Event.h`**
    *   **Concept**: Creates a lightweight pub/sub mechanism (`Event<T>`, `Emitter<T>`) that is entirely thread-safe using `std::mutex`.
    *   **Architecture**: Subscribing to an event returns an `IDisposable`. This perfectly guarantees deterministic cleanup of event listeners and avoids memory leaks commonly found in loosely coupled systems.

## 5. Editor Core (`core/editor/`)

*   **`PieceTable.h / .cpp`**
    *   **Concept**: Replaces basic strings/arrays for text file storage.
    *   **Architecture**: Replaced the naive vector storage with an **O(log N) Red-Black Tree** directly wired into the `ArenaAllocator`. This ensures all tree nodes are physically contiguous in RAM, providing lightning-fast CPU cache locality while guaranteeing that massive file edits never freeze the engine.
    *   **Lazy Loading**: The PieceTable uses direct disk I/O and a 4KB LRU cache to buffer file lines, keeping 5GB files near 0MB RAM footprint on open.

## 6. Utilities (`core/utils/`)

*   **`Json.h / .cpp`**
    *   **Concept**: A zero-copy JSON scanner utilizing `std::string_view`.
*   **`StringPool.h / .cpp`**
    *   **Concept**: A per-file localized string deduplicator. Reuses strings across large parsed files without introducing global multi-threading mutex contention, backed by the Arena.
*   **`LineScanner.h / .cpp`**
    *   **Concept**: A SIMD-accelerated (`std::memchr`) string scanner to instantly map `\n` line breaks across massive files in 32-byte vectorized chunks.

## 7. IPC & Extension Bridging (`core/ipc/`)

*   **`IPCClient.h / .cpp`**
    *   **Concept**: The critical bridge decoupling the Native IDE from the headless Node.js Extension Host.
    *   **Architecture**: Uses Named Pipes/Sockets to stream JSON-RPC commands. VS Code extensions running in the Node.js background process send standard edit commands, which are intercepted, parsed via `Json`, and routed instantly to the Native C++ `PieceTable`. This eliminates Electron completely while guaranteeing 100% marketplace extension compatibility.

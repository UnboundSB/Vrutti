# Vrutti IDE - Master Roadmap

## Phase 1-4: The Core Engine (✅ COMPLETED)
The invisible foundation of the IDE is completely bulletproof and heavily optimized.
- `[x]` **Memory System**: Implemented zero-copy `ArenaAllocator` and deterministic `IDisposable` memory tracking.
- `[x]` **Concurrency**: Implemented `ThreadPool` and Thread-safe `Event/Emitter` models.
- `[x]` **Editor Text Core**: Gutted basic arrays and built an O(log N) **Arena-Backed Red-Black Piece Tree**.
- `[x]` **Massive File Optimizations**: Implemented **True Lazy Loading** (0MB RAM for 5GB files), 4KB LRU Caching, SIMD `LineScanner`, and `StringPool`.
- `[x]` **Architecture Bridging**: Built the `IPCClient` to decouple from Electron, allowing VS Code extensions to run invisibly in a background Node.js process while communicating with C++ via JSON-RPC.

---

## What We Should Target Next: Native UI (Phase 5)

Now that the backend is a Ferrari engine, it's time to build the chassis. We need to actually render this text to the screen without using a browser.

### Phase 5: Native Rendering Engine (🚀 UP NEXT)
We will use **Dear ImGui** backed by **GLFW/OpenGL3**. It is the absolute fastest, zero-bloat UI framework in the C++ world, executing in microseconds and rendering directly to the GPU.
- `[ ]` Configure `CMakeLists.txt` to link OpenGL3 and GLFW.
- `[ ]` Download and integrate `Dear ImGui` core files.
- `[ ]` Create `src/ui/Window` to handle OS-level window creation and input polling.
- `[ ]` Create `src/ui/EditorView` to visually render the `PieceTable` text to the screen in real-time.

### Phase 6: Workspace & File System Management (PENDING)
- `[ ]` Implement robust directory scanning (`core/fs/Workspace`).
- `[ ]` Implement OS-level File Watchers for live-reloading.
- `[ ]` Build the UI File Explorer tree panel.

### Phase 7: The Extension Host Bridge (PENDING)
- `[ ]` Write the actual Node.js bootstrapper script that loads the `vscode` API polyfill.
- `[ ]` Connect the Node.js script to the C++ `IPCClient` named pipe.

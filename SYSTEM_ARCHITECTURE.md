# Vrutti IDE - System Architecture Manifest

This document serves as the architectural "Bible" for the Vrutti IDE. All future development **MUST** adhere to the principles, boundaries, and communication protocols defined below. Failure to do so will result in immediate rejection of the PR.

## 1. Core Mandates
- **Zero Hardcoding**: No hardcoded environment paths, themes, or workspace structures are permitted. Everything must be dynamic or configurable.
- **No Node.js / Electron**: Vrutti IDE strictly forbids the use of Node.js for backend services (except where the native VS Code Extension Host process requires it for extension execution). All heavy lifting, including process spawning (PTYs, Debug Adapters) and file system operations, must be done natively in C++.
- **Memory Efficiency**: The IDE must run with minimal memory footprint. All backend logic is separated into independent C++ DLLs that are dynamically loaded and unloaded on demand.

## 2. The Architectural Matrix

Vrutti operates on a highly decoupled 3-tier architecture:

```
[ Lit UI (Tauri Webview) ]  <--(Tauri IPC)-->  [ C++ Backend Broker (DLLs) ]  <--(RPC)-->  [ VS Code Extension Host ]
```

### 2.1 The Lit UI (Frontend)
- Built entirely with Lit and native Web Components.
- Strict JS/TS environment.
- Communicates with the backend exclusively via Tauri IPC bindings (e.g., `vrutti_*` commands).

### 2.2 The C++ Backend Broker (The Translation Layer)
The C++ backend is the true core of Vrutti. Since we do not use Monaco or Electron, the C++ layer acts as a flawless translation broker that tricks the VS Code Extension Host into believing it is running inside a native VS Code environment.

The backend is composed of lazy-loaded DLLs, including:
- **`DocumentSyncService.dll`**: The single source of truth for text. Uses a memory-efficient Piece Table data structure. Batches and debounces text sync events (50ms) to the ExtHost.
- **`FileSystemService.dll`**: Handles native `std::filesystem` operations for extensions. Enforces a caching layer for highly requested configs to prevent disk I/O thrashing.
- **`TerminalService.dll`**: Manages native OS Pseudo-Terminals (ConPTY/forkpty). Streams raw stdout/stderr bytes directly to the UI using non-blocking detached `std::thread` instances.
- **`DebugAdapterService.dll`**: The native DAP (Debug Adapter Protocol) client. Spawns debugging adapters (e.g., Python, Node) as native OS processes and manages asynchronous stdin/stdout RPC streaming.
- **`LanguageFeatureService.dll` & `DiagnosticService.dll`**: The IntelliSense bridge. Intercepts and routes autocomplete, hover, and diagnostic payloads between extensions and the UI.
- **`VSCodeExtHostService.dll`**: Generates the dynamic `IExtensionHostInitData` environment payload required to trick extensions into activating.

### 2.3 The VS Code Extension Host
- The official VS Code Extension Host (`extHost.ts`).
- Vrutti does **not** modify this code. We interact with it exclusively by speaking its native JSON-RPC protocol (the `MainThread*Shape` and `ExtHost*Shape` APIs).

## 3. The Single Source of Truth Rule
- The **C++ Backend** is the definitive owner of all application state (Text Documents, Workspace Topology, Live Diagnostics).
- The Lit UI is purely a presentation layer. It asks C++ for the state and renders it.
- The Extension Host is purely a worker process. It asks C++ for the state and processes it.
- If a conflict arises (e.g., a file is modified externally), the C++ `Piece Table` or `Workspace Topology` determines the resolution and pushes the delta to both the UI and the ExtHost simultaneously.

## 4. Thread Safety & Lifecycle Management
- **DLL Loading**: Plugins are loaded via `LoadLibrary`/`dlopen`. They expose standard C `createPlugin` and `destroyPlugin` externs.
- **Async I/O**: High-throughput streams (like PTY terminal output and DAP messages) are handled by detached `std::thread` instances.
- **Graceful Termination**: All `std::thread` loops must respect an `std::atomic<bool> active` flag. When a DLL receives a `shutdown()` signal (e.g., when the user closes a workspace), it must set the active flag to false, gracefully `join()` all detached threads, and cleanly release file/process handles before the DLL is unloaded.

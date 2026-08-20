# Core Engine (src/core/)

## Purpose
The foundational native C++ framework that powers Vrutti IDE. It provides low-level operating system abstractions and core services like IPC, file management, configuration, and event routing.

## Files
- (No immediate source files; functionality is modularized into subdirectories)


## Child Directories
- **concurrency/**: Manages thread pools, async tasks, and parallel execution logic.
- **config/**: Handles loading, parsing, and managing user settings (SettingsManager).
- **editor/**: Manages editor state and backend logic for text manipulation.
- **events/**: Event bus and messaging system for cross-component communication.
- **fs/**: High-performance filesystem operations and watchers.
- **ipc/**: Inter-Process Communication (e.g., Named Pipes) bridging the C++ engine and Node.js extension host.
- **memory/**: Memory management utilities (if any custom allocators are used).
- **plugins/**: Manages the dynamic loading (PluginLoader) and lifecycle of native DLL plugins.
- **terminal/**: Backend orchestration for the integrated terminal/PTY handling.
- **utils/**: Shared helper classes (e.g., JSON parsing/serialization, logging).


## Additional Components
- **task.md**: Component implementation.


# Directory: core

## Purpose
Houses the core backend C++ subsystems like concurrency, memory, and filesystem management.

## Child Directories
- **concurrency/**: Responsible for managing asynchronous tasks and thread pools to keep the IDE responsive.
- **config/**: Manages the IDE settings and configuration persistence.
- **editor/**: Implements the core text editing data structures, such as the Piece Table for efficient text manipulation.
- **events/**: Provides event buses and observable patterns for internal messaging.
- **fs/**: Responsible for file system operations, paths, URIs, and workspace management.
- **ipc/**: Handles inter-process communication between the C++ backend and the Node.js/Webview frontends.
- **memory/**: Implements custom memory allocators like Arena Allocators for high-performance memory management.
- **plugins/**: Responsible for the plugin architecture and dynamic library loading. Under core, it is the loader; at root, it contains specific plugins.
- **terminal/**: Manages pseudo-terminal processes and the read-eval-print loop (REPL) backend.
- **utils/**: Contains general purpose utility classes for JSON parsing, string manipulation, and base64 encoding.

## Files
- **structure.md**: Documents the purpose of this directory, its child directories, and files.
- **task.md**: Responsible for storing data related to task.md.

# Source Root (src/)

## Purpose
The primary root directory containing the complete source code for Vrutti IDE. It houses the native C++ backend engine, the Node.js extension host, dynamic native plugins, and the web-based frontend application.

## Files
- (No immediate source files; this is purely a structural root directory)

## Child Directories
- **app/**: Contains the main application entry point (e.g., main.cpp).
- **core/**: The core C++ engine, managing IPC, filesystems, memory, concurrency, and plugin lifecycle.
- **ext/**: Node.js extension host and built-in themes/extensions.
- **plugins/**: Native C++ plugins (like high-performance search) that compile to DLLs/shared objects.
- **ui/**: The frontend and presentation layer, including the native C++ window compositor and the web-based user interface.

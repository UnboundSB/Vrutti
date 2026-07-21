# Vrutti IDE - Source Code Structure

This document outlines the source code organization for the Vrutti IDE's C++ core. 
The architecture is designed for high-performance, low RAM consumption, and a zero-copy extension bridge.

## Directory Layout

- `core/` - The foundational C++ system modules.
  - `memory/` - Custom memory allocators, zero-copy buffers, and resource management.
  - `fs/` - Fast async file system operations and watchers.
  - `concurrency/` - Thread pools, task schedulers, and lock-free data structures.
  - `plugins/` - Core DLL dynamic loading architecture for lazy-loaded features.
- `plugins/` - Features extracted into standalone native DLLs (e.g., `search`).
- `ui/` - The native user interface rendering engine.
  - `compositor/` - Hardware-accelerated rendering layer.
  - `widgets/` - Base UI components, input handling, and layout engine.
  - `views/` - High-level editor windows, sidebars, panels, and toolbars.
- `bridge/` - Zero-copy extension communication layer.
  - `ipc/` - Fast inter-process communication protocols for out-of-process extensions.
  - `api/` - Host API bindings exposed to the extension host.
- `editor/` - The core text editing and manipulation component.
  - `buffer/` - Piece table or rope data structures optimized for very large files.
  - `syntax/` - Fast syntax parsing, tokenization, and highlighting engine.
- `platform/` - OS-specific abstractions and native windowing.
  - `windows/` - Win32/DirectX specific code.
  - `macos/` - Cocoa/Metal specific code.
  - `linux/` - X11/Wayland/OpenGL specific code.
- `app/` - The main application entry point, configuration, and lifecycle management.

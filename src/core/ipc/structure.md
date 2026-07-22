# Core IPC Module

This directory contains the Inter-Process Communication (IPC) architecture for Vrutti IDE.
It is responsible for establishing a zero-copy or highly optimized bridge between the C++ Native Core and the Node.js Extension Host.

## Files
- `IPCClient.h / IPCClient.cpp`:
  - Implements the named pipe / Unix domain socket client that connects to the Node.js backend.
  - Handles bidirectional JSON payload serialization/deserialization.
  - Exposes `send(message)` and `start()` lifecycle methods for the C++ backend to push filesystem syncs and LSP requests to the Extension Host.

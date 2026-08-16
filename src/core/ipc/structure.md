# IPC (src/core/ipc/)

## Purpose
Implements the Inter-Process Communication bridge connecting the native C++ engine and the Node.js extension host over Named Pipes (Windows) or Domain Sockets (Linux).

## Files
- **IPCClient.cpp**: Contains the implementation of the IPC client that listens for and dispatches JSON RPC messages.
- **IPCClient.h**: Header for the IPCClient class.

## Child Directories
- (No immediate subdirectories)

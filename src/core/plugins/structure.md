# Plugin System (src/core/plugins/)

## Purpose
Defines the interfaces and loading mechanisms for dynamically loading native C++ plugins (DLLs/Shared Objects) at runtime.

## Files
- **IPlugin.h**: The core interface that all native plugins (like SearchPlugin) must inherit from.
- **PluginLoader.cpp**: The implementation for dynamically discovering, loading, and managing the lifecycle of DLL files via OS APIs (LoadLibraryA/dlopen).
- **PluginLoader.h**: Header for the PluginLoader class.

## Child Directories
- (No immediate subdirectories)

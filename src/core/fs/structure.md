# Vrutti Core File System Structure

This directory handles the optimized file system interactions for the native core, specifically focusing on path resolution, glob matching, and lazy-loading directory trees.

## Files
- `Glob.h` / `Glob.cpp`: Minimalist string pattern matcher for file exclusions (avoids regex overhead).
- `Path.h` / `Path.cpp`: Abstract representation of a file path, ensuring cross-platform slash normalization.
- `URI.h` / `URI.cpp`: Uniform Resource Identifier parser for handling `file://` schemes passed from the Extension Host.
- `Workspace.h` / `Workspace.cpp`: The central hub for file discovery. Implements a lazy-loading scanner that only queries the operating system for directory contents when explicitly requested, saving significant RAM and CPU on startup.

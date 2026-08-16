# Search Plugin (src/plugins/search/)

## Purpose
A highly-optimized native C++ plugin built as a dynamic library (rutti_search.dll). It performs blazing-fast multi-threaded codebase scanning, regex evaluations, and file filtering without relying on external dependencies like Ripgrep.

## Files
- **SearchPlugin.cpp**: Implements the core logic for recursive file traversal, background indexing, and text matching (using optimized substring search or std::regex).
- **SearchPlugin.h**: Header defining the SearchPlugin class which inherits from IPlugin.

## Child Directories
- (No immediate subdirectories)

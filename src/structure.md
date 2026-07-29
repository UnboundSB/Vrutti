# Directory: src

## Purpose
The main source code directory containing all C++ and frontend components.

## Child Directories
- **app/**: Contains the application entry point and main executable logic.
- **core/**: Houses the core backend C++ subsystems like concurrency, memory, and filesystem management.
- **ext/**: Contains the Node.js extension host scripts and API bindings for extensions.
- **plugins/**: Responsible for the plugin architecture and dynamic library loading. Under core, it is the loader; at root, it contains specific plugins.
- **ui/**: Responsible for the user interface, bridging the native compositor and the web frontend.

## Files
- **structure.md**: Documents the purpose of this directory, its child directories, and files.

# Extension Host (src/ext/)

## Purpose
The Node.js environment responsible for loading extensions, providing the extension API, and booting the IDE's backend services.

## Files
- **bootstrapper.js**: The initial script executed by Node.js. It connects via IPC to the native C++ engine and bridges the extension environment.

## Child Directories
- **bin/**: Executable scripts or wrapper binaries used by the extension host.
- **builtin-themes/**: Default bundled themes (e.g., rutti-default-dark) containing syntax highlighting and UI color definitions.

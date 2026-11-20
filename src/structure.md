# src (src/)

## Purpose
The application source: the native core, the extension host and the user interface.

## Files
- **TODO.md**: Open questions and planned work for this folder.

## Child Directories
- **app/**: The application entry point: it wires the core services together and starts the window.
- **core/**: The native core: configuration, plugins, IPC, storage and the other services.
- **ext/**: The extension side: the launcher, the extension host entry point, the API surface and the bundled themes.
- **plugins/**: Feature plugins that are not part of the core, such as the search implementation.
- **tests/**: Test suites that ship with the source tree.
- **ui/**: The user interface layer: the native window and the web renderer.

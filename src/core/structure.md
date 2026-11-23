# core (src/core/)

## Purpose
The native core: configuration, plugins, IPC, storage and the other services.

## Files
- **task.md**: Documentation for task.
- **TODO.md**: Open questions and planned work for this folder.

## Child Directories
- **commands/**: Command registry. Named actions such as save or undo that the UI, keybindings and extensions can trigger.
- **concurrency/**: Concurrency helpers: the thread pool and the async primitives used by the core.
- **config/**: Configuration handling: the settings model and the merge rules.
- **configuration/**: Configuration services: reading, merging and writing user, workspace and policy settings.
- **di/**: Dependency injection: the container that builds services and wires them together.
- **editor/**: Editor core: the text buffer, the piece table and the document model.
- **events/**: Event bus: the in-process publish and subscribe mechanism.
- **extensions/**: The extensions module.
- **fs/**: File system services: watching, reading and writing files.
- **ipc/**: Inter-process communication: the pipe transport and the protocol that connects the core to the extension host.
- **memory/**: Memory helpers: allocators and the shared buffer utilities.
- **notifications/**: Notification service: the progress and message queue shown in the UI.
- **plugins/**: The plugin system: the native plugin interface and the loader that loads plugin libraries.
- **secrets/**: Secret storage: encrypted values kept out of the plain settings files.
- **storage/**: Storage service: the on-disk database that holds workspace and editor state.
- **telemetry/**: Telemetry service: optional, opt-in usage reporting.
- **terminal/**: Native PTY (Pseudo-Terminal) broker for the integrated terminal. Uses ConPTY on Windows, forkpty on POSIX. Streams raw stdout/stderr bytes to the frontend via...
- **theme/**: Theme service: discovery, resolution and translation of colour and icon themes.
- **updates/**: Update service: checks for and applies new versions of the application.
- **utils/**: Shared helpers: the JSON parser and serializer, path resolution, string pools and the error code registry.
- **webview/**: Webview service: the panel that hosts extension webviews.

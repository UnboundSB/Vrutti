# Plugin System (src/core/plugins/)

## Purpose
Defines the interfaces and loading mechanisms for dynamically loading native C++ plugins (DLLs/Shared Objects) at runtime.

## Files
- **IPlugin.h**: The core interface that all native plugins (like SearchPlugin) must inherit from.
- **PluginLoader.cpp**: The implementation for dynamically discovering, loading, and managing the lifecycle of DLL files via OS APIs (LoadLibraryA/dlopen).
- **PluginLoader.h**: Header for the PluginLoader class.
- **ExtensionManager.h**: Header for the ExtensionManager DLL plugin.
- **ExtensionManager.cpp**: Implementation for the dynamically loaded ExtensionManager that handles extension discovery and parsing via C++.
- **ExtensionScanner.h**: Header for ExtensionScanner, replacing ExtensionManager for extension package scanning.
- **ExtensionScanner.cpp**: Implementation for statically discovering and scanning extensions.
- **VSCodeExtHostService.h**: Header for the VSCodeExtHostService DLL plugin.
- **VSCodeExtHostService.cpp**: Implementation for dynamic VS Code Extension Host initialization payload construction.
- **MediaController.h**: Header for the MediaController DLL plugin.
- **MediaController.cpp**: Implementation for managing and aggressively caching background media in the compositor.
- **VSXRegistryService.h**: Header for the VSXRegistryService DLL plugin.
- **VSXRegistryService.cpp**: Implementation for OpenVSX registry search and LRU caching mechanism.
- **DocumentSyncService.h**: Header for the DocumentSyncService DLL plugin, managing the authoritative Piece Table.
- **DocumentSyncService.cpp**: Implementation for the document translation broker, handling text edits and IPC/RPC batching.
- **ExtensionLifecycleManager.h**: Header for the ExtensionLifecycleManager DLL plugin.
- **ExtensionLifecycleManager.cpp**: Implementation for dynamic VS Code extension activation based on activationEvents.
- **CommandHandlerService.h**: Header for the CommandHandlerService DLL plugin.
- **CommandHandlerService.cpp**: Implementation for maintaining the registry of active commands and routing execution via RPC.
- **LanguageFeatureService.h**: Header for LanguageFeatureService DLL plugin.
- **LanguageFeatureService.cpp**: Implementation for intercepting language features (hover, completion) registrations and requests.
- **DiagnosticService.h**: Header for DiagnosticService DLL plugin.
- **DiagnosticService.cpp**: Implementation for managing document diagnostics (errors/warnings) and emitting updates to UI.
- **FileSystemService.h**: Header for FileSystemService DLL plugin.
- **FileSystemService.cpp**: Implementation for native file system access and ExtHost FileSystem API bridging.
- **WorkspaceService.h**: Header for WorkspaceService DLL plugin.
- **WorkspaceService.cpp**: Implementation for managing workspace folders and ExtHost Workspace API bridging.
- **DebugAdapterService.h**: Header for DebugAdapterService DLL plugin.
- **DebugAdapterService.cpp**: Implementation for managing DAP (Debug Adapter Protocol) sessions and process lifecycle.
- **TerminalService.h**: Header for TerminalService DLL plugin.
- **TerminalService.cpp**: Implementation for native PTY (Pseudo-Terminal) bridging and shell I/O.
- **ThemeService.h**: Header for the ThemeService DLL plugin.
- **ThemeService.cpp**: Implementation for discovering, resolving and translating colour/icon themes.

## Public Methods
- **IPlugin::initialize()**: Prepares the plugin; returns true when the plugin is ready.
- **IPlugin::shutdown()**: Releases everything the plugin acquired.
- **IPlugin::getName()**: Returns the plugin's display name.
- **IPlugin::executeCommand(command, payload)**: Runs a named command and returns its JSON result.
- **ThemeService::executeCommand(command, payload)**: `list` / `listIcons` return the available themes; `load` / `loadIcon` return the resolved theme for the given id or label.
- **PluginLoader::loadPlugin(path)**: Loads a plugin DLL and returns its instance, or null on failure.
- **PluginLoader::unloadPlugin(name)**: Unloads a previously loaded plugin.
- **PluginLoader::getPlugin(name)**: Returns an already loaded plugin instance, or null.

## Child Directories
- (No immediate subdirectories)

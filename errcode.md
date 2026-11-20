# Error codes

Every failure that crosses a process or module boundary carries one of these
codes, so a log line or a crash report tells you which subsystem failed and
roughly why -- without a stack trace and without reading the source.

The format is `VRU-XXXX`, where the high byte selects the subsystem and the
low byte the failure inside it.

| Range | Subsystem | Covers |
| ----- | --------- | ------ |
| `0x00xx` | Core | Core and startup |
| `0x01xx` | IPC | IPC and extension host |
| `0x02xx` | JSON | JSON |
| `0x03xx` | Config | Configuration and settings |
| `0x04xx` | Plugin | Plugins |
| `0x05xx` | FS | File system |
| `0x06xx` | Workspace | Workspace |
| `0x07xx` | UI | User interface |
| `0x08xx` | Process | Processes and terminals |
| `0x09xx` | Extension | Extensions and marketplace |
| `0x0Axx` | Theme | Themes |
| `0x0Bxx` | Debug | Debug adapters |

## Registry

| Code | Name | Subsystem | Meaning |
| ---- | ---- | --------- | ------- |
| `VRU-0000` | `None` | Core | No error. |
| `VRU-0001` | `CoreUnknownError` | Core | A failure that does not fit any other category. |
| `VRU-0002` | `CoreInitFailed` | Core | The core could not be initialised. |
| `VRU-0003` | `CoreResourceMissing` | Core | A required file or directory is missing. |
| `VRU-0004` | `CoreUnexpectedState` | Core | The core was reached in a state it cannot handle. |
| `VRU-0005` | `CoreOperationFailed` | Core | An operation failed for an unspecified reason. |
| `VRU-0006` | `CoreTimeout` | Core | An operation did not finish in time. |
| `VRU-0007` | `CoreCancelled` | Core | An operation was cancelled before it finished. |
| `VRU-0008` | `CoreNotImplemented` | Core | The requested feature does not exist yet. |
| `VRU-0009` | `CorePermissionDenied` | Core | The operating system denied access. |
| `VRU-000A` | `CoreInvalidArgument` | Core | An argument was missing, empty or malformed. |
| `VRU-0101` | `IpcConnectionFailed` | IPC | The renderer and the extension host could not connect. |
| `VRU-0102` | `IpcPipeCreateFailed` | IPC | The named pipe could not be created. |
| `VRU-0103` | `IpcWriteFailed` | IPC | A frame could not be written to the pipe. |
| `VRU-0104` | `IpcReadFailed` | IPC | A frame could not be read from the pipe. |
| `VRU-0105` | `IpcFrameDecodeFailed` | IPC | A received frame was not valid JSON. |
| `VRU-0106` | `IpcFrameEncodeFailed` | IPC | A frame could not be turned into JSON. |
| `VRU-0107` | `IpcTimeout` | IPC | The peer did not answer in time. |
| `VRU-0108` | `IpcProtocolMismatch` | IPC | The peer speaks a different wire protocol. |
| `VRU-0109` | `IpcRequestCancelled` | IPC | A request was cancelled before it was answered. |
| `VRU-010A` | `IpcHandlerMissing` | IPC | No handler is registered for the requested method. |
| `VRU-010B` | `IpcUnknownShape` | IPC | The request addresses an RPC shape that does not exist. |
| `VRU-010C` | `IpcHostNotReady` | IPC | The extension host was contacted before it was ready. |
| `VRU-010D` | `IpcHostCrashed` | IPC | The extension host process died. |
| `VRU-010E` | `IpcHostSpawnFailed` | IPC | The extension host process could not be started. |
| `VRU-010F` | `IpcDuplicateRequest` | IPC | A request id was used twice. |
| `VRU-0110` | `IpcHandshakeFailed` | IPC | The start-up handshake did not complete. |
| `VRU-0201` | `JsonParseFailed` | JSON | The text is not valid JSON. |
| `VRU-0202` | `JsonUnexpectedToken` | JSON | A token appeared where it is not allowed. |
| `VRU-0203` | `JsonInvalidEscape` | JSON | An escape sequence is malformed. |
| `VRU-0204` | `JsonTypeMismatch` | JSON | A value has a different type than expected. |
| `VRU-0205` | `JsonSerializeFailed` | JSON | A tree could not be turned into JSON. |
| `VRU-0206` | `JsonUnexpectedEnd` | JSON | The text ended before the value was complete. |
| `VRU-0207` | `JsonRootNotObject` | JSON | The root of the document is not an object. |
| `VRU-0301` | `ConfigReadFailed` | Config | The settings file could not be read. |
| `VRU-0302` | `ConfigWriteFailed` | Config | The settings file could not be written. |
| `VRU-0303` | `ConfigParseFailed` | Config | The settings file is not valid JSON. |
| `VRU-0304` | `ConfigInvalidValue` | Config | A setting has a value it cannot take. |
| `VRU-0305` | `ConfigKeyMissing` | Config | A setting that is required is not set. |
| `VRU-0306` | `ConfigLocked` | Config | A setting is locked and cannot be changed. |
| `VRU-0401` | `PluginLoadFailed` | Plugin | A plugin DLL could not be loaded. |
| `VRU-0402` | `PluginNotFound` | Plugin | The requested plugin does not exist. |
| `VRU-0403` | `PluginInitFailed` | Plugin | A plugin failed during initialisation. |
| `VRU-0404` | `PluginCommandFailed` | Plugin | A plugin command returned a failure. |
| `VRU-0405` | `PluginDuplicate` | Plugin | A plugin with the same name is already loaded. |
| `VRU-0406` | `PluginUnloadFailed` | Plugin | A plugin could not be unloaded. |
| `VRU-0501` | `FsNotFound` | FS | The path does not exist. |
| `VRU-0502` | `FsPermissionDenied` | FS | Access to the path was denied. |
| `VRU-0503` | `FsReadFailed` | FS | The file could not be read. |
| `VRU-0504` | `FsWriteFailed` | FS | The file could not be written. |
| `VRU-0505` | `FsDeleteFailed` | FS | The file could not be deleted. |
| `VRU-0506` | `FsRenameFailed` | FS | The file could not be renamed. |
| `VRU-0507` | `FsPathInvalid` | FS | The path is malformed. |
| `VRU-0508` | `FsAlreadyExists` | FS | The path already exists. |
| `VRU-0509` | `FsIoFailed` | FS | An input/output operation failed. |
| `VRU-0601` | `WorkspaceNotFound` | Workspace | No workspace is open. |
| `VRU-0602` | `WorkspaceInvalid` | Workspace | The workspace data is malformed. |
| `VRU-0603` | `WorkspaceFolderMissing` | Workspace | A folder of the workspace no longer exists. |
| `VRU-0604` | `WorkspaceAlreadyOpen` | Workspace | The workspace is already open. |
| `VRU-0605` | `WorkspaceNotInitialized` | Workspace | The workspace was used before it was set up. |
| `VRU-0701` | `UiWindowCreateFailed` | UI | The window could not be created. |
| `VRU-0702` | `UiWebviewLoadFailed` | UI | The webview could not load its page. |
| `VRU-0703` | `UiBridgeFailed` | UI | The bridge between the renderer and the core failed. |
| `VRU-0704` | `UiScriptEvalFailed` | UI | A script could not be evaluated. |
| `VRU-0705` | `UiRenderFailed` | UI | The page could not be rendered. |
| `VRU-0706` | `UiDisposed` | UI | The object was used after it was disposed. |
| `VRU-0801` | `ProcessSpawnFailed` | Process | The process could not be started. |
| `VRU-0802` | `ProcessExitNonZero` | Process | The process exited with a failure code. |
| `VRU-0803` | `ProcessTimeout` | Process | The process did not finish in time. |
| `VRU-0804` | `TerminalUnavailable` | Process | No terminal is available. |
| `VRU-0805` | `TerminalCreateFailed` | Process | The terminal could not be created. |
| `VRU-0806` | `TerminalWriteFailed` | Process | The terminal could not be written to. |
| `VRU-0901` | `ExtensionNotFound` | Extension | The extension does not exist. |
| `VRU-0902` | `ExtensionInstallFailed` | Extension | The extension could not be installed. |
| `VRU-0903` | `ExtensionUninstallFailed` | Extension | The extension could not be removed. |
| `VRU-0904` | `ExtensionActivateFailed` | Extension | The extension failed to activate. |
| `VRU-0905` | `ExtensionManifestInvalid` | Extension | The extension manifest is malformed. |
| `VRU-0906` | `ExtensionDuplicateId` | Extension | Another extension already uses this id. |
| `VRU-0907` | `ExtensionVersionConflict` | Extension | The extension version conflicts with another one. |
| `VRU-0908` | `RegistryUnavailable` | Extension | The extension registry could not be reached. |
| `VRU-0A01` | `ThemeNotFound` | Theme | The theme does not exist. |
| `VRU-0A02` | `ThemeParseFailed` | Theme | The theme file is not valid. |
| `VRU-0A03` | `ThemeIncludeFailed` | Theme | The base theme of the theme could not be loaded. |
| `VRU-0A04` | `ThemeApplyFailed` | Theme | The theme could not be applied. |
| `VRU-0A05` | `ThemeInvalid` | Theme | The theme data is malformed. |
| `VRU-0B01` | `DebugAdapterNotFound` | Debug | No debug adapter is registered for the session. |
| `VRU-0B02` | `DebugAdapterStartFailed` | Debug | The debug adapter could not be started. |
| `VRU-0B03` | `DebugAdapterProtocolError` | Debug | The debug adapter sent an invalid message. |
| `VRU-0B04` | `DebugAdapterExited` | Debug | The debug adapter exited unexpectedly. |

## How to use them

C++:

```cpp
#include "core/utils/ErrorCodes.h"

using vrutti::core::utils::ErrorCode;
using vrutti::core::utils::reportError;

reportError("IPC", ErrorCode::IpcPipeCreateFailed, "pipe " + pipeName);
```

Extension host (JavaScript):

```js
const { ErrorCode, formatError } = require('./errorCodes');

console.error(formatError(ErrorCode.IpcHostCrashed, `exit code ${code}`));
```

## Keeping the registry in sync

`src/core/utils/ErrorCodes.h` is the single source of truth.
`scripts/generate_error_codes.py` regenerates the JavaScript mirror and
the table above from it:

```
python scripts/generate_error_codes.py
python scripts/generate_error_codes.py --check
```

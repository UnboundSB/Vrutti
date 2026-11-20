#pragma once

#include <string>

namespace vrutti::core::utils {

    // -------------------------------------------------------------------------
    // Central error code registry.
    //
    // Every failure that crosses a process or module boundary carries one of
    // these codes. A log line then tells you which subsystem failed and
    // roughly why, without needing a stack trace or the source at hand.
    //
    // The numeric value is also the display id: the low byte identifies the
    // subsystem, the high byte the failure inside it. `ErrorCode::IpcPipe-
    // CreateFailed` (0x0102) is reported as "VRU-0102".
    //
    //     0x00xx  core and startup
    //     0x01xx  IPC and extension host
    //     0x02xx  JSON
    //     0x03xx  configuration and settings
    //     0x04xx  plugins
    //     0x05xx  file system
    //     0x06xx  workspace
    //     0x07xx  user interface
    //     0x08xx  processes and terminals
    //     0x09xx  extensions and marketplace
    //     0x0Axx  themes
    //     0x0Bxx  debug adapters
    //
    // This header is the single source of truth. `scripts/generate_error_codes.py`
    // regenerates the JavaScript mirror and the table in `errcode.md` from it,
    // so the three never drift apart.
    // -------------------------------------------------------------------------
    enum class ErrorCode : unsigned short {
        None = 0x0000,

        // --- Core and startup (0x00xx) ---------------------------------------
        CoreUnknownError      = 0x0001,
        CoreInitFailed        = 0x0002,
        CoreResourceMissing   = 0x0003,
        CoreUnexpectedState   = 0x0004,
        CoreOperationFailed   = 0x0005,
        CoreTimeout           = 0x0006,
        CoreCancelled         = 0x0007,
        CoreNotImplemented    = 0x0008,
        CorePermissionDenied  = 0x0009,
        CoreInvalidArgument   = 0x000A,

        // --- IPC and extension host (0x01xx) ---------------------------------
        IpcConnectionFailed   = 0x0101,
        IpcPipeCreateFailed   = 0x0102,
        IpcWriteFailed        = 0x0103,
        IpcReadFailed         = 0x0104,
        IpcFrameDecodeFailed  = 0x0105,
        IpcFrameEncodeFailed  = 0x0106,
        IpcTimeout            = 0x0107,
        IpcProtocolMismatch   = 0x0108,
        IpcRequestCancelled   = 0x0109,
        IpcHandlerMissing     = 0x010A,
        IpcUnknownShape       = 0x010B,
        IpcHostNotReady       = 0x010C,
        IpcHostCrashed        = 0x010D,
        IpcHostSpawnFailed    = 0x010E,
        IpcDuplicateRequest   = 0x010F,
        IpcHandshakeFailed    = 0x0110,

        // --- JSON (0x02xx) ---------------------------------------------------
        JsonParseFailed       = 0x0201,
        JsonUnexpectedToken   = 0x0202,
        JsonInvalidEscape     = 0x0203,
        JsonTypeMismatch      = 0x0204,
        JsonSerializeFailed   = 0x0205,
        JsonUnexpectedEnd     = 0x0206,
        JsonRootNotObject     = 0x0207,

        // --- Configuration and settings (0x03xx) ----------------------------
        ConfigReadFailed      = 0x0301,
        ConfigWriteFailed     = 0x0302,
        ConfigParseFailed     = 0x0303,
        ConfigInvalidValue    = 0x0304,
        ConfigKeyMissing      = 0x0305,
        ConfigLocked          = 0x0306,

        // --- Plugins (0x04xx) -----------------------------------------------
        PluginLoadFailed      = 0x0401,
        PluginNotFound        = 0x0402,
        PluginInitFailed      = 0x0403,
        PluginCommandFailed   = 0x0404,
        PluginDuplicate       = 0x0405,
        PluginUnloadFailed    = 0x0406,

        // --- File system (0x05xx) -------------------------------------------
        FsNotFound            = 0x0501,
        FsPermissionDenied    = 0x0502,
        FsReadFailed          = 0x0503,
        FsWriteFailed         = 0x0504,
        FsDeleteFailed        = 0x0505,
        FsRenameFailed        = 0x0506,
        FsPathInvalid         = 0x0507,
        FsAlreadyExists       = 0x0508,
        FsIoFailed            = 0x0509,

        // --- Workspace (0x06xx) ---------------------------------------------
        WorkspaceNotFound     = 0x0601,
        WorkspaceInvalid      = 0x0602,
        WorkspaceFolderMissing = 0x0603,
        WorkspaceAlreadyOpen  = 0x0604,
        WorkspaceNotInitialized = 0x0605,

        // --- User interface (0x07xx) ----------------------------------------
        UiWindowCreateFailed  = 0x0701,
        UiWebviewLoadFailed   = 0x0702,
        UiBridgeFailed        = 0x0703,
        UiScriptEvalFailed    = 0x0704,
        UiRenderFailed        = 0x0705,
        UiDisposed            = 0x0706,

        // --- Processes and terminals (0x08xx) -------------------------------
        ProcessSpawnFailed    = 0x0801,
        ProcessExitNonZero    = 0x0802,
        ProcessTimeout        = 0x0803,
        TerminalUnavailable   = 0x0804,
        TerminalCreateFailed  = 0x0805,
        TerminalWriteFailed   = 0x0806,

        // --- Extensions and marketplace (0x09xx) ----------------------------
        ExtensionNotFound     = 0x0901,
        ExtensionInstallFailed = 0x0902,
        ExtensionUninstallFailed = 0x0903,
        ExtensionActivateFailed = 0x0904,
        ExtensionManifestInvalid = 0x0905,
        ExtensionDuplicateId  = 0x0906,
        ExtensionVersionConflict = 0x0907,
        RegistryUnavailable   = 0x0908,

        // --- Themes (0x0Axx) -------------------------------------------------
        ThemeNotFound         = 0x0A01,
        ThemeParseFailed      = 0x0A02,
        ThemeIncludeFailed    = 0x0A03,
        ThemeApplyFailed      = 0x0A04,
        ThemeInvalid          = 0x0A05,

        // --- Debug adapters (0x0Bxx) -----------------------------------------
        DebugAdapterNotFound  = 0x0B01,
        DebugAdapterStartFailed = 0x0B02,
        DebugAdapterProtocolError = 0x0B03,
        DebugAdapterExited    = 0x0B04,
    };

    // Returns the display id of a code, e.g. "VRU-0102".
    std::string errorCodeId(ErrorCode code);

    // Returns the symbolic name of a code, e.g. "IpcPipeCreateFailed".
    const char* errorCodeName(ErrorCode code);

    // Formats a code and a detail message for a log line:
    //   "VRU-0102 IpcPipeCreateFailed: <message>"
    std::string formatError(ErrorCode code, const std::string& message);

    // Writes a formatted error to the error stream, prefixed with the
    // subsystem tag used by the rest of the log output.
    void reportError(const char* tag, ErrorCode code, const std::string& message);

} // namespace vrutti::core::utils

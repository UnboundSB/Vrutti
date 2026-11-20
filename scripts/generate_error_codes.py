#!/usr/bin/env python3
"""Keeps the error code registry in sync.

`src/core/utils/ErrorCodes.h` is the single source of truth. This script
parses it and regenerates:

  * `src/ext/errorCodes.js`  -- the mirror used by the extension host
  * `errcode.md`              -- the human readable registry

Run it after changing the enum:

    python scripts/generate_error_codes.py

Use `--check` to verify that the generated files are up to date without
writing anything (useful in CI).
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent

HEADER_PATH = REPO_ROOT / "src" / "core" / "utils" / "ErrorCodes.h"
CPP_PATH = REPO_ROOT / "src" / "core" / "utils" / "ErrorCodes.cpp"
JS_PATH = REPO_ROOT / "src" / "ext" / "errorCodes.js"
DOC_PATH = REPO_ROOT / "errcode.md"

# The enum member: `Name = 0xNNNN,`
MEMBER_RE = re.compile(r"^\s*([A-Za-z][A-Za-z0-9_]*)\s*=\s*0x([0-9A-Fa-f]{4})\s*,", re.M)

# The name table entry in ErrorCodes.cpp: `{ 0xNNNN, "Name" },`
TABLE_RE = re.compile(r"\{\s*0x([0-9A-Fa-f]{4})\s*,\s*\"([A-Za-z][A-Za-z0-9_]*)\"\s*\}")

# A section marker in the header, used to group the codes.
SECTION_RE = re.compile(r"^\s*//\s*---\s*(.*?)\s*\(0x([0-9A-Fa-f]{2})xx\)\s*-+$", re.M)

# The description of a code lives in the header as a trailing comment.
DESCRIPTIONS = {
    "None": "No error.",
    "CoreUnknownError": "A failure that does not fit any other category.",
    "CoreInitFailed": "The core could not be initialised.",
    "CoreResourceMissing": "A required file or directory is missing.",
    "CoreUnexpectedState": "The core was reached in a state it cannot handle.",
    "CoreOperationFailed": "An operation failed for an unspecified reason.",
    "CoreTimeout": "An operation did not finish in time.",
    "CoreCancelled": "An operation was cancelled before it finished.",
    "CoreNotImplemented": "The requested feature does not exist yet.",
    "CorePermissionDenied": "The operating system denied access.",
    "CoreInvalidArgument": "An argument was missing, empty or malformed.",
    "IpcConnectionFailed": "The renderer and the extension host could not connect.",
    "IpcPipeCreateFailed": "The named pipe could not be created.",
    "IpcWriteFailed": "A frame could not be written to the pipe.",
    "IpcReadFailed": "A frame could not be read from the pipe.",
    "IpcFrameDecodeFailed": "A received frame was not valid JSON.",
    "IpcFrameEncodeFailed": "A frame could not be turned into JSON.",
    "IpcTimeout": "The peer did not answer in time.",
    "IpcProtocolMismatch": "The peer speaks a different wire protocol.",
    "IpcRequestCancelled": "A request was cancelled before it was answered.",
    "IpcHandlerMissing": "No handler is registered for the requested method.",
    "IpcUnknownShape": "The request addresses an RPC shape that does not exist.",
    "IpcHostNotReady": "The extension host was contacted before it was ready.",
    "IpcHostCrashed": "The extension host process died.",
    "IpcHostSpawnFailed": "The extension host process could not be started.",
    "IpcDuplicateRequest": "A request id was used twice.",
    "IpcHandshakeFailed": "The start-up handshake did not complete.",
    "JsonParseFailed": "The text is not valid JSON.",
    "JsonUnexpectedToken": "A token appeared where it is not allowed.",
    "JsonInvalidEscape": "An escape sequence is malformed.",
    "JsonTypeMismatch": "A value has a different type than expected.",
    "JsonSerializeFailed": "A tree could not be turned into JSON.",
    "JsonUnexpectedEnd": "The text ended before the value was complete.",
    "JsonRootNotObject": "The root of the document is not an object.",
    "ConfigReadFailed": "The settings file could not be read.",
    "ConfigWriteFailed": "The settings file could not be written.",
    "ConfigParseFailed": "The settings file is not valid JSON.",
    "ConfigInvalidValue": "A setting has a value it cannot take.",
    "ConfigKeyMissing": "A setting that is required is not set.",
    "ConfigLocked": "A setting is locked and cannot be changed.",
    "PluginLoadFailed": "A plugin DLL could not be loaded.",
    "PluginNotFound": "The requested plugin does not exist.",
    "PluginInitFailed": "A plugin failed during initialisation.",
    "PluginCommandFailed": "A plugin command returned a failure.",
    "PluginDuplicate": "A plugin with the same name is already loaded.",
    "PluginUnloadFailed": "A plugin could not be unloaded.",
    "FsNotFound": "The path does not exist.",
    "FsPermissionDenied": "Access to the path was denied.",
    "FsReadFailed": "The file could not be read.",
    "FsWriteFailed": "The file could not be written.",
    "FsDeleteFailed": "The file could not be deleted.",
    "FsRenameFailed": "The file could not be renamed.",
    "FsPathInvalid": "The path is malformed.",
    "FsAlreadyExists": "The path already exists.",
    "FsIoFailed": "An input/output operation failed.",
    "WorkspaceNotFound": "No workspace is open.",
    "WorkspaceInvalid": "The workspace data is malformed.",
    "WorkspaceFolderMissing": "A folder of the workspace no longer exists.",
    "WorkspaceAlreadyOpen": "The workspace is already open.",
    "WorkspaceNotInitialized": "The workspace was used before it was set up.",
    "UiWindowCreateFailed": "The window could not be created.",
    "UiWebviewLoadFailed": "The webview could not load its page.",
    "UiBridgeFailed": "The bridge between the renderer and the core failed.",
    "UiScriptEvalFailed": "A script could not be evaluated.",
    "UiRenderFailed": "The page could not be rendered.",
    "UiDisposed": "The object was used after it was disposed.",
    "ProcessSpawnFailed": "The process could not be started.",
    "ProcessExitNonZero": "The process exited with a failure code.",
    "ProcessTimeout": "The process did not finish in time.",
    "TerminalUnavailable": "No terminal is available.",
    "TerminalCreateFailed": "The terminal could not be created.",
    "TerminalWriteFailed": "The terminal could not be written to.",
    "ExtensionNotFound": "The extension does not exist.",
    "ExtensionInstallFailed": "The extension could not be installed.",
    "ExtensionUninstallFailed": "The extension could not be removed.",
    "ExtensionActivateFailed": "The extension failed to activate.",
    "ExtensionManifestInvalid": "The extension manifest is malformed.",
    "ExtensionDuplicateId": "Another extension already uses this id.",
    "ExtensionVersionConflict": "The extension version conflicts with another one.",
    "RegistryUnavailable": "The extension registry could not be reached.",
    "ThemeNotFound": "The theme does not exist.",
    "ThemeParseFailed": "The theme file is not valid.",
    "ThemeIncludeFailed": "The base theme of the theme could not be loaded.",
    "ThemeApplyFailed": "The theme could not be applied.",
    "ThemeInvalid": "The theme data is malformed.",
    "DebugAdapterNotFound": "No debug adapter is registered for the session.",
    "DebugAdapterStartFailed": "The debug adapter could not be started.",
    "DebugAdapterProtocolError": "The debug adapter sent an invalid message.",
    "DebugAdapterExited": "The debug adapter exited unexpectedly.",
}

SUBSYSTEMS = [
    (0x00, "Core", "Core and startup"),
    (0x01, "IPC", "IPC and extension host"),
    (0x02, "JSON", "JSON"),
    (0x03, "Config", "Configuration and settings"),
    (0x04, "Plugin", "Plugins"),
    (0x05, "FS", "File system"),
    (0x06, "Workspace", "Workspace"),
    (0x07, "UI", "User interface"),
    (0x08, "Process", "Processes and terminals"),
    (0x09, "Extension", "Extensions and marketplace"),
    (0x0A, "Theme", "Themes"),
    (0x0B, "Debug", "Debug adapters"),
]


def parse_codes() -> list[tuple[int, str]]:
    """Returns the (value, name) pairs of the error code enum."""
    text = HEADER_PATH.read_text(encoding="utf-8")
    # Only look at the enum body.
    start = text.index("enum class ErrorCode")
    end = text.index("};", start)
    body = text[start:end]
    return [(int(value, 16), name) for name, value in MEMBER_RE.findall(body)]


def parse_cpp_table() -> list[tuple[int, str]]:
    """Returns the (value, name) pairs of the C++ name table."""
    text = CPP_PATH.read_text(encoding="utf-8")
    return [(int(value, 16), name) for value, name in TABLE_RE.findall(text)]


def subsystem_of(value: int) -> tuple[str, str]:
    for prefix, short, title in SUBSYSTEMS:
        if (value >> 8) == prefix:
            return short, title
    return "Other", "Other"


def render_js(codes: list[tuple[int, str]]) -> str:
    lines = [
        "// Error code mirror -- GENERATED FILE, DO NOT EDIT.",
        "//",
        "// The source of truth is src/core/utils/ErrorCodes.h.",
        "// Regenerate with: python scripts/generate_error_codes.py",
        "",
        "'use strict';",
        "",
        "/** Display id of a code, e.g. 0x0102 -> 'VRU-0102'. */",
        "function errorCodeId(value) {",
        "    return 'VRU-' + (value & 0xffff).toString(16).toUpperCase().padStart(4, '0');",
        "}",
        "",
        "/** Symbolic name of a code, e.g. 0x0102 -> 'IpcPipeCreateFailed'. */",
        "function errorCodeName(value) {",
        "    return ERROR_CODE_NAMES[value] || 'UnknownErrorCode';",
        "}",
        "",
        "/** Formats a code and a detail message for a log line. */",
        "function formatError(value, message) {",
        "    const text = errorCodeId(value) + ' ' + errorCodeName(value);",
        "    return message ? text + ': ' + message : text;",
        "}",
        "",
        "const ERROR_CODE_NAMES = {",
    ]
    for value, name in codes:
        lines.append(f"    0x{value:04X}: '{name}',")
    lines.append("};")
    lines.append("")
    lines.append("const ErrorCode = {")
    for value, name in codes:
        lines.append(f"    {name}: 0x{value:04X},")
    lines.append("};")
    lines.append("")
    lines.append("module.exports = { ErrorCode, ERROR_CODE_NAMES, errorCodeId, errorCodeName, formatError };")
    lines.append("")
    return "\n".join(lines)


DOC_HEADER = """# Error codes

Every failure that crosses a process or module boundary carries one of these
codes, so a log line or a crash report tells you which subsystem failed and
roughly why -- without a stack trace and without reading the source.

The format is `VRU-XXXX`, where the high byte selects the subsystem and the
low byte the failure inside it.

| Range | Subsystem | Covers |
| ----- | --------- | ------ |
"""


def render_doc(codes: list[tuple[int, str]]) -> str:
    out = [DOC_HEADER.rstrip("\n")]
    for prefix, short, title in SUBSYSTEMS:
        out.append(f"| `0x{prefix:02X}xx` | {short} | {title} |")
    out.append("")
    out.append("## Registry")
    out.append("")
    out.append("| Code | Name | Subsystem | Meaning |")
    out.append("| ---- | ---- | --------- | ------- |")
    for value, name in codes:
        short, _ = subsystem_of(value)
        meaning = DESCRIPTIONS.get(name, "")
        out.append(f"| `VRU-{value:04X}` | `{name}` | {short} | {meaning} |")
    out.append("")
    out.append("## How to use them")
    out.append("")
    out.append("C++:")
    out.append("")
    out.append("```cpp")
    out.append('#include "core/utils/ErrorCodes.h"')
    out.append("")
    out.append("using vrutti::core::utils::ErrorCode;")
    out.append("using vrutti::core::utils::reportError;")
    out.append("")
    out.append('reportError("IPC", ErrorCode::IpcPipeCreateFailed, "pipe " + pipeName);')
    out.append("```")
    out.append("")
    out.append("Extension host (JavaScript):")
    out.append("")
    out.append("```js")
    out.append("const { ErrorCode, formatError } = require('./errorCodes');")
    out.append("")
    out.append("console.error(formatError(ErrorCode.IpcHostCrashed, `exit code ${code}`));")
    out.append("```")
    out.append("")
    out.append("## Keeping the registry in sync")
    out.append("")
    out.append("`src/core/utils/ErrorCodes.h` is the single source of truth.")
    out.append("`scripts/generate_error_codes.py` regenerates the JavaScript mirror and")
    out.append("the table above from it:")
    out.append("")
    out.append("```")
    out.append("python scripts/generate_error_codes.py")
    out.append("python scripts/generate_error_codes.py --check")
    out.append("```")
    out.append("")
    return "\n".join(out)


def write_or_check(path: pathlib.Path, content: str, check: bool) -> bool:
    current = path.read_text(encoding="utf-8") if path.exists() else ""
    if current == content:
        return True
    if check:
        print(f"[error-codes] {path.relative_to(REPO_ROOT)} is out of date", file=sys.stderr)
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    print(f"[error-codes] wrote {path.relative_to(REPO_ROOT)}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verify the generated files are up to date")
    args = parser.parse_args()

    codes = parse_codes()
    if not codes:
        print("[error-codes] no error codes found in the header", file=sys.stderr)
        return 1

    # The C++ name table must cover exactly the same codes.
    table = parse_cpp_table()
    if table != codes:
        only_header = {name for _, name in codes} - {name for _, name in table}
        only_table = {name for _, name in table} - {name for _, name in codes}
        print("[error-codes] the C++ name table and the header disagree", file=sys.stderr)
        if only_header:
            print(f"  missing from the table: {sorted(only_header)}", file=sys.stderr)
        if only_table:
            print(f" missing from the header: {sorted(only_table)}", file=sys.stderr)
        return 1

    ok = True
    ok &= write_or_check(JS_PATH, render_js(codes), args.check)
    ok &= write_or_check(DOC_PATH, render_doc(codes), args.check)

    if not ok:
        return 1

    print(f"[error-codes] {len(codes)} codes, registry is in sync")
    return 0


if __name__ == "__main__":
    sys.exit(main())

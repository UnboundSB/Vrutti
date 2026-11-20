# Extension Host (src/ext/)

## Purpose
The Node.js environment responsible for loading extensions, providing the extension API, and booting the IDE's backend services.

## Files
- **bootstrapper.js**: The initial script executed by Node.js. It connects via IPC to the native C++ engine and bridges the extension environment.

## Child Directories
- **bin/**: Executable scripts or wrapper binaries used by the extension host.
- **builtin-themes/**: Default bundled themes (e.g.,  rutti-default-dark) containing syntax highlighting and UI color definitions.


## Additional Components
- **bootstrapper.js**: The initial script executed by Node.js. It connects via IPC to the native C++ engine and bridges the extension environment.
- **crypto-polyfill.js**: Preloaded module that installs the crypto primitives the extension host expects.
- **dap-client.js**: Implementation for the Debug Adapter Protocol (DAP) client.
- **errorCodes.js**: Generated mirror of the central error code registry (see `errcode.md`).
- **ipc.js**: Inter-process communication handler for the Node.js extension host.
- **lsp-client.js**: Implementation for Language Server Protocol (LSP) integration.
- **package-lock.json**: Dependency tree lockfile.
- **package.json**: Node project definition.
- **textmate-engine.js**: Engine for parsing and tokenizing text using TextMate grammars.
- **theme-resolver.js**: Resolves and maps theme files into IDE usable colors.
- **test_runner.js**: Test execution wrapper for extensions.

## Public Methods
- **errorCodeId(value)**: Returns the display id of an error code, e.g. "VRU-0102".
- **errorCodeName(value)**: Returns the symbolic name of an error code.
- **formatError(value, message)**: Formats an error code and a detail message for a log line.

